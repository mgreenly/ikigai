package mcp

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"appkit/server"
)

func TestHealthToolReturnsAppkitEnvelope(t *testing.T) {
	h := NewHandler("test-version", "wiki", nil)
	body := bytes.NewBufferString(`{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"health"}}`)
	rec := httptest.NewRecorder()

	h.ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/mcp", body))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	var got struct {
		Result struct {
			Content []struct {
				Text string `json:"text"`
			} `json:"content"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &got); err != nil {
		t.Fatalf("response JSON: %v", err)
	}
	if len(got.Result.Content) != 1 {
		t.Fatalf("content len = %d, want 1", len(got.Result.Content))
	}
	var env map[string]any
	if err := json.Unmarshal([]byte(got.Result.Content[0].Text), &env); err != nil {
		t.Fatalf("health text JSON: %v", err)
	}
	if env["service"] != "wiki" {
		t.Fatalf("service = %v, want wiki", env["service"])
	}
	if env["version"] != "test-version" {
		t.Fatalf("version = %v, want test-version", env["version"])
	}
	if env["status"] != "ok" {
		t.Fatalf("status = %v, want ok", env["status"])
	}
}

func TestInitializeAdvertisesWikiMCPServer(t *testing.T) {
	// R-6RVX-P1IG
	h := NewHandler("test-version", "wiki", nil)
	body := bytes.NewBufferString(`{"jsonrpc":"2.0","id":"init","method":"initialize"}`)
	rec := httptest.NewRecorder()

	h.ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/mcp", body))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	var got struct {
		Result struct {
			ProtocolVersion string `json:"protocolVersion"`
			Capabilities    struct {
				Tools map[string]any `json:"tools"`
			} `json:"capabilities"`
			ServerInfo struct {
				Name    string `json:"name"`
				Version string `json:"version"`
			} `json:"serverInfo"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &got); err != nil {
		t.Fatalf("response JSON: %v", err)
	}
	if got.Result.ProtocolVersion != "2025-03-26" {
		t.Fatalf("protocolVersion = %q, want 2025-03-26", got.Result.ProtocolVersion)
	}
	if got.Result.Capabilities.Tools == nil {
		t.Fatal("capabilities.tools is nil")
	}
	if got.Result.ServerInfo.Name != "Wiki" {
		t.Fatalf("serverInfo.name = %q, want Wiki", got.Result.ServerInfo.Name)
	}
	if got.Result.ServerInfo.Version != "test-version" {
		t.Fatalf("serverInfo.version = %q, want test-version", got.Result.ServerInfo.Version)
	}
}

func TestToolsListAdvertisesConfiguredWikiSurface(t *testing.T) {
	// R-MUQ4-K1JS
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithIngestService(&capturingWiki{}),
		WithJobStatusService(&capturingWiki{}),
		WithAskFunc((&capturingAsker{}).Ask),
		WithSubjectsService(&capturingWiki{}),
		WithClaimsService(&capturingWiki{}),
		WithPagePathService(&capturingWiki{}),
	))
	rec := callMCP(t, h, `{"jsonrpc":"2.0","id":"list","method":"tools/list"}`, "owner@example.com")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	var got struct {
		Result struct {
			Tools []struct {
				Name        string         `json:"name"`
				InputSchema map[string]any `json:"inputSchema"`
			} `json:"tools"`
		} `json:"result"`
	}
	decodeJSON(t, rec.Body.Bytes(), &got)
	names := map[string]bool{}
	for _, tool := range got.Result.Tools {
		if names[tool.Name] {
			t.Fatalf("tools/list duplicated %s", tool.Name)
		}
		names[tool.Name] = true
		if tool.InputSchema["type"] != "object" {
			t.Fatalf("%s schema type = %v, want object", tool.Name, tool.InputSchema["type"])
		}
	}
	want := map[string]bool{
		"ingest":     true,
		"status":     true,
		"ask":        true,
		"subjects":   true,
		"claims":     true,
		"page":       true,
		"health":     true,
		"reflection": true,
	}
	if len(names) != len(want) {
		t.Fatalf("tools/list names = %#v, want exact %#v", names, want)
	}
	for name := range want {
		if !names[name] {
			t.Fatalf("tools/list missing %s in %#v", name, names)
		}
	}
	for name := range names {
		if !want[name] {
			t.Fatalf("tools/list included unexpected %s in %#v", name, names)
		}
	}
}

func TestToolsListInputSchemasUseValidRequiredFields(t *testing.T) {
	// R-N4KO-2WTZ
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithIngestService(&capturingWiki{}),
		WithJobStatusService(&capturingWiki{}),
		WithAskFunc((&capturingAsker{}).Ask),
		WithSubjectsService(&capturingWiki{}),
		WithClaimsService(&capturingWiki{}),
		WithPagePathService(&capturingWiki{}),
	))
	rec := callMCP(t, h, `{"jsonrpc":"2.0","id":"list","method":"tools/list"}`, "owner@example.com")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	var got struct {
		Result struct {
			Tools []struct {
				Name        string         `json:"name"`
				InputSchema map[string]any `json:"inputSchema"`
			} `json:"tools"`
		} `json:"result"`
	}
	decodeJSON(t, rec.Body.Bytes(), &got)
	if len(got.Result.Tools) == 0 {
		t.Fatal("tools/list returned no tools")
	}
	seenSubjects := false
	for _, tool := range got.Result.Tools {
		if tool.InputSchema["type"] != "object" {
			t.Fatalf("%s schema type = %v, want object", tool.Name, tool.InputSchema["type"])
		}
		required, ok := tool.InputSchema["required"]
		if tool.Name == "subjects" {
			seenSubjects = true
			if ok {
				t.Fatalf("subjects schema required = %#v, want omitted", required)
			}
			continue
		}
		if !ok {
			continue
		}
		values, ok := required.([]any)
		if !ok {
			t.Fatalf("%s schema required = %T, want array of strings", tool.Name, required)
		}
		if len(values) == 0 {
			t.Fatalf("%s schema required is empty, want omitted or non-empty", tool.Name)
		}
		for i, value := range values {
			if _, ok := value.(string); !ok {
				t.Fatalf("%s schema required[%d] = %T, want string", tool.Name, i, value)
			}
		}
	}
	if !seenSubjects {
		t.Fatal("tools/list missing subjects tool")
	}
}

func TestReflectionToolReturnsEmptyEventEdges(t *testing.T) {
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil))
	rec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"reflection",
		"method":"tools/call",
		"params":{"name":"reflection","arguments":{}}
	}`, "owner@example.com")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	var body struct {
		Publishes  []string `json:"publishes"`
		Subscribes []string `json:"subscribes"`
	}
	decodeToolText(t, rec.Body.Bytes(), &body)
	if len(body.Publishes) != 0 || len(body.Subscribes) != 0 {
		t.Fatalf("reflection = %#v, want empty publishes/subscribes", body)
	}
}

func TestUnknownReadsReturnCleanNotFoundResults(t *testing.T) {
	wiki := &capturingWiki{
		statusErr: sql.ErrNoRows,
		claimsErr: sql.ErrNoRows,
		pageErr:   sql.ErrNoRows,
	}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithJobStatusService(wiki),
		WithClaimsService(wiki),
		WithPagePathService(wiki),
	))
	for _, tc := range []struct {
		name string
		body string
		kind string
		id   string
	}{
		{
			name: "status",
			body: `{"jsonrpc":"2.0","id":"status","method":"tools/call","params":{"name":"status","arguments":{"job_id":"job-missing"}}}`,
			kind: "job",
			id:   "job-missing",
		},
		{
			name: "claims",
			body: `{"jsonrpc":"2.0","id":"claims","method":"tools/call","params":{"name":"claims","arguments":{"subject":"entity/missing"}}}`,
			kind: "subject",
			id:   "entity/missing",
		},
		{
			name: "page",
			body: `{"jsonrpc":"2.0","id":"page","method":"tools/call","params":{"name":"page","arguments":{"subject":"entity/missing"}}}`,
			kind: "subject",
			id:   "entity/missing",
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			rec := callMCP(t, h, tc.body, "owner@example.com")
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want 200", rec.Code)
			}
			var body struct {
				Found bool   `json:"found"`
				Kind  string `json:"kind"`
				ID    string `json:"id"`
			}
			decodeToolText(t, rec.Body.Bytes(), &body)
			if body.Found || body.Kind != tc.kind || body.ID != tc.id {
				t.Fatalf("not found = %#v, want %s %s found=false", body, tc.kind, tc.id)
			}
			var raw struct {
				Result struct {
					IsError bool `json:"isError"`
				} `json:"result"`
			}
			decodeJSON(t, rec.Body.Bytes(), &raw)
			if raw.Result.IsError {
				t.Fatal("result isError = true, want clean not-found result")
			}
		})
	}
}

func TestIngestToolUsesAuthenticatedIdentity(t *testing.T) {
	// R-MVY0-XTAH
	wiki := &capturingWiki{ingestID: "job-123"}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithIngestService(wiki)))
	rec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"ingest",
		"method":"tools/call",
		"params":{
			"name":"ingest",
			"arguments":{"text":"source text","title":"Source","tags":["one","two"]}
		}
	}`, "owner@example.com")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if wiki.ingestOwner != "owner@example.com" {
		t.Fatalf("ingest owner = %q, want authenticated owner", wiki.ingestOwner)
	}
	if wiki.ingestText != "source text" || wiki.ingestTitle != "Source" {
		t.Fatalf("ingest captured text/title = %q/%q", wiki.ingestText, wiki.ingestTitle)
	}
	if len(wiki.ingestTags) != 2 || wiki.ingestTags[0] != "one" || wiki.ingestTags[1] != "two" {
		t.Fatalf("ingest tags = %#v, want [one two]", wiki.ingestTags)
	}
	var body struct {
		JobID string `json:"job_id"`
	}
	decodeToolText(t, rec.Body.Bytes(), &body)
	if body.JobID != "job-123" {
		t.Fatalf("job_id = %q, want job-123", body.JobID)
	}
}

func TestJobStatusToolReturnsDomainStatus(t *testing.T) {
	// R-MX5X-BL16
	received := time.Date(2026, 6, 20, 12, 0, 0, 0, time.UTC)
	finished := received.Add(3 * time.Second)
	wiki := &capturingWiki{status: jobStatus{
		ID:         "job-123",
		Status:     "done",
		ReceivedAt: received,
		FinishedAt: &finished,
		Subjects:   []string{"subject-1"},
	}}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithJobStatusService(wiki)))
	rec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"status",
		"method":"tools/call",
		"params":{"name":"status","arguments":{"job_id":"job-123"}}
	}`, "owner@example.com")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if wiki.statusJobID != "job-123" {
		t.Fatalf("status job id = %q, want job-123", wiki.statusJobID)
	}
	var body jobStatus
	decodeToolText(t, rec.Body.Bytes(), &body)
	if body.ID != "" || body.Status != "done" {
		t.Fatalf("job status = %#v, want done with no serialized job id", body)
	}
	if len(body.Subjects) != 1 || body.Subjects[0] != "subject-1" {
		t.Fatalf("subjects = %#v, want [subject-1]", body.Subjects)
	}
}

func TestPageToolUsesTypeSlugPath(t *testing.T) {
	// R-01OQ-Y5YV
	wiki := &capturingWiki{page: page{
		ID:        "page-1",
		SubjectID: "subject-1",
		Title:     "Acme Robotics",
		Body:      "Acme Robotics overview.",
	}}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithPagePathService(wiki)))
	listRec := callMCP(t, h, `{"jsonrpc":"2.0","id":"list","method":"tools/list"}`, "owner@example.com")
	if listRec.Code != http.StatusOK {
		t.Fatalf("tools/list status = %d, want 200", listRec.Code)
	}
	var list struct {
		Result struct {
			Tools []struct {
				Name        string         `json:"name"`
				InputSchema map[string]any `json:"inputSchema"`
			} `json:"tools"`
		} `json:"result"`
	}
	decodeJSON(t, listRec.Body.Bytes(), &list)
	foundPageSchema := false
	for _, tool := range list.Result.Tools {
		if tool.Name != "page" {
			continue
		}
		foundPageSchema = true
		required, ok := tool.InputSchema["required"].([]any)
		if !ok || len(required) != 1 || required[0] != "subject" {
			t.Fatalf("page required = %#v, want [subject]", tool.InputSchema["required"])
		}
		properties, ok := tool.InputSchema["properties"].(map[string]any)
		if !ok {
			t.Fatalf("page properties = %T, want object", tool.InputSchema["properties"])
		}
		subject, ok := properties["subject"].(map[string]any)
		if !ok || subject["type"] != "string" {
			t.Fatalf("page subject schema = %#v, want string property", properties["subject"])
		}
		for _, forbidden := range []string{"path", "subject_id"} {
			if _, ok := properties[forbidden]; ok {
				t.Fatalf("page properties = %#v, want no %s argument", properties, forbidden)
			}
		}
	}
	if !foundPageSchema {
		t.Fatal("tools/list missing page schema")
	}

	rec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"page",
		"method":"tools/call",
		"params":{"name":"page","arguments":{"subject":"entity/acme-robotics"}}
	}`, "owner@example.com")
	if rec.Code != http.StatusOK {
		t.Fatalf("page status = %d, want 200", rec.Code)
	}
	if wiki.pagePath != "entity/acme-robotics" {
		t.Fatalf("page path = %q, want type/slug path", wiki.pagePath)
	}
	var body struct {
		Subject string `json:"subject"`
		Title   string `json:"title"`
		Body    string `json:"body"`
	}
	decodeToolText(t, rec.Body.Bytes(), &body)
	if body.Subject != "entity/acme-robotics" || body.Title != "Acme Robotics" {
		t.Fatalf("page body = %#v, want returned page", body)
	}
}

func TestReadToolsSerializePublicPathsWithoutSubjectIDs(t *testing.T) {
	// R-03GW-PX5K
	internalSubjectID := "01HZX4Q0SUBJECTULID00000001"
	wiki := &capturingWiki{
		status: jobStatus{
			ID:       "job-123",
			Status:   "done",
			Subjects: []string{"entity/acme-robotics"},
		},
		subjects: []subject{{
			ID:       internalSubjectID,
			Name:     "Acme Robotics",
			NormName: "acme robotics",
			Type:     "entity",
		}},
		claims: []claim{{
			ID:        "claim-1",
			SubjectID: internalSubjectID,
			JobID:     "job-123",
			Body:      "Acme Robotics runs a Tulsa lab.",
		}},
		page: page{
			ID:        "page-1",
			SubjectID: internalSubjectID,
			Title:     "Acme Robotics",
			Body:      "Acme Robotics overview.",
		},
	}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithJobStatusService(wiki),
		WithSubjectsService(wiki),
		WithClaimsService(wiki),
		WithPagePathService(wiki),
	))

	for _, tc := range []struct {
		name    string
		request string
		check   func(t *testing.T, text string)
	}{
		{
			name: "status",
			request: `{"jsonrpc":"2.0","id":"status","method":"tools/call","params":{
				"name":"status","arguments":{"job_id":"job-123"}
			}}`,
			check: func(t *testing.T, text string) {
				t.Helper()
				var body struct {
					Status   string   `json:"status"`
					Subjects []string `json:"subjects"`
				}
				decodeJSON(t, []byte(text), &body)
				if strings.Contains(text, `"job_id"`) {
					t.Fatalf("status body = %s, want no serialized job_id", text)
				}
				if body.Status != "done" {
					t.Fatalf("status body = %#v, want status field", body)
				}
				if len(body.Subjects) != 1 || body.Subjects[0] != "entity/acme-robotics" {
					t.Fatalf("status subjects = %#v, want public path", body.Subjects)
				}
			},
		},
		{
			name: "subjects",
			request: `{"jsonrpc":"2.0","id":"subjects","method":"tools/call","params":{
				"name":"subjects","arguments":{"type":"entity","name":"acme"}
			}}`,
			check: func(t *testing.T, text string) {
				t.Helper()
				var body []struct {
					Path    string `json:"path"`
					Name    string `json:"name"`
					Type    string `json:"type"`
					HasPage bool   `json:"has_page"`
				}
				decodeJSON(t, []byte(text), &body)
				if !strings.Contains(text, `"has_page"`) {
					t.Fatalf("subjects body = %s, want has_page field", text)
				}
				if len(body) != 1 || body[0].Path != "entity/acme-robotics" || body[0].Name != "Acme Robotics" || body[0].Type != "entity" {
					t.Fatalf("subjects body = %#v, want public path/name/type", body)
				}
			},
		},
		{
			name: "claims",
			request: `{"jsonrpc":"2.0","id":"claims","method":"tools/call","params":{
				"name":"claims","arguments":{"subject":"entity/acme-robotics"}
			}}`,
			check: func(t *testing.T, text string) {
				t.Helper()
				var body []struct {
					ID   string `json:"id"`
					Text string `json:"text"`
					Job  string `json:"job"`
				}
				decodeJSON(t, []byte(text), &body)
				for _, forbidden := range []string{`"path"`, `"body"`} {
					if strings.Contains(text, forbidden) {
						t.Fatalf("claims body = %s, leaked %s", text, forbidden)
					}
				}
				if len(body) != 1 || body[0].ID != "claim-1" || body[0].Text != "Acme Robotics runs a Tulsa lab." || body[0].Job != "job-123" {
					t.Fatalf("claims body = %#v, want public id/text/job", body)
				}
			},
		},
		{
			name: "page",
			request: `{"jsonrpc":"2.0","id":"page","method":"tools/call","params":{
				"name":"page","arguments":{"subject":"entity/acme-robotics"}
			}}`,
			check: func(t *testing.T, text string) {
				t.Helper()
				var body struct {
					Subject string `json:"subject"`
					Title   string `json:"title"`
					Body    string `json:"body"`
				}
				decodeJSON(t, []byte(text), &body)
				if strings.Contains(text, `"path"`) {
					t.Fatalf("page body = %s, want subject field instead of path", text)
				}
				if body.Subject != "entity/acme-robotics" || body.Title != "Acme Robotics" || body.Body != "Acme Robotics overview." {
					t.Fatalf("page body = %#v, want public path/title/body", body)
				}
			},
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			rec := callMCP(t, h, tc.request, "owner@example.com")
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
			}
			text := toolTextString(t, rec.Body.Bytes())
			if tc.name != "claims" && !strings.Contains(text, "entity/acme-robotics") {
				t.Fatalf("tool text = %s, want public path", text)
			}
			for _, forbidden := range []string{internalSubjectID, "SubjectID", "subject_id", "NormName", "norm_name", "job_id"} {
				if strings.Contains(text, forbidden) {
					t.Fatalf("tool text = %s, leaked %q", text, forbidden)
				}
			}
			tc.check(t, text)
		})
	}
}

func TestAskToolUsesAuthenticatedIdentity(t *testing.T) {
	// R-MYDT-PCRV
	asker := &capturingAsker{answer: answer{
		Found: true,
		Text:  "Ada wrote the note.",
		Citations: []citation{{
			Path:  "entity/ada",
			Title: "Ada",
		}},
	}}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithAskFunc(asker.Ask)))
	rec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"ask",
		"method":"tools/call",
		"params":{"name":"ask","arguments":{"question":"Who wrote it?"}}
	}`, "owner@example.com")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if asker.owner != "owner@example.com" {
		t.Fatalf("ask owner = %q, want authenticated owner", asker.owner)
	}
	if asker.question != "Who wrote it?" {
		t.Fatalf("ask question = %q, want forwarded question", asker.question)
	}
	var body struct {
		Found     bool   `json:"found"`
		Answer    string `json:"answer"`
		Citations []struct {
			Path  string `json:"path"`
			Title string `json:"title"`
		} `json:"citations"`
	}
	decodeToolText(t, rec.Body.Bytes(), &body)
	if !body.Found || body.Answer != "Ada wrote the note." {
		t.Fatalf("answer = %#v, want found Ada answer", body)
	}
	if len(body.Citations) != 1 || body.Citations[0].Path != "entity/ada" {
		t.Fatalf("citations = %#v, want entity/ada citation", body.Citations)
	}
}

func TestAskToolReturnsPathTitleCitations(t *testing.T) {
	// R-044J-PPG9
	asker := &capturingAsker{answer: answer{
		Found: true,
		Text:  "Ada wrote the note.",
		Citations: []citation{{
			Path:  "person/ada-lovelace",
			Title: "Ada Lovelace",
		}},
	}}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithAskFunc(asker.Ask)))
	rec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"ask",
		"method":"tools/call",
		"params":{"name":"ask","arguments":{"question":"Who wrote it?"}}
	}`, "owner@example.com")

	if rec.Code != http.StatusOK {
		t.Fatalf("ask status = %d, want 200", rec.Code)
	}
	var body struct {
		Found     bool   `json:"found"`
		Answer    string `json:"answer"`
		Citations []struct {
			Path    string `json:"path"`
			Title   string `json:"title"`
			Subject string `json:"subject"`
		} `json:"citations"`
	}
	decodeToolText(t, rec.Body.Bytes(), &body)
	if !body.Found || body.Answer != "Ada wrote the note." {
		t.Fatalf("ask body = %#v, want found answer text", body)
	}
	if len(body.Citations) != 1 || body.Citations[0].Path != "person/ada-lovelace" || body.Citations[0].Title != "Ada Lovelace" {
		t.Fatalf("ask citations = %#v, want path/title citation", body.Citations)
	}
	if body.Citations[0].Subject != "" {
		t.Fatalf("ask citation subject = %q, want omitted internal subject id", body.Citations[0].Subject)
	}
}

func TestAskToolUsesPhase17InputAndResultShape(t *testing.T) {
	// R-6A8D-0RK9
	asker := &capturingAsker{answer: answer{
		Found: true,
		Text:  "Ada wrote the note.",
		Citations: []citation{{
			Path:  "person/ada",
			Title: "Ada",
		}},
	}}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithAskFunc(asker.Ask)))
	listRec := callMCP(t, h, `{"jsonrpc":"2.0","id":"list","method":"tools/list"}`, "owner@example.com")
	if listRec.Code != http.StatusOK {
		t.Fatalf("tools/list status = %d, want 200", listRec.Code)
	}
	var list struct {
		Result struct {
			Tools []struct {
				Name        string         `json:"name"`
				InputSchema map[string]any `json:"inputSchema"`
			} `json:"tools"`
		} `json:"result"`
	}
	decodeJSON(t, listRec.Body.Bytes(), &list)
	foundAskSchema := false
	for _, tool := range list.Result.Tools {
		if tool.Name != "ask" {
			continue
		}
		foundAskSchema = true
		required, ok := tool.InputSchema["required"].([]any)
		if !ok || len(required) != 1 || required[0] != "question" {
			t.Fatalf("ask required = %#v, want [question]", tool.InputSchema["required"])
		}
		properties, ok := tool.InputSchema["properties"].(map[string]any)
		if !ok {
			t.Fatalf("ask properties = %T, want object", tool.InputSchema["properties"])
		}
		question, ok := properties["question"].(map[string]any)
		if !ok || question["type"] != "string" {
			t.Fatalf("ask question schema = %#v, want string property", properties["question"])
		}
	}
	if !foundAskSchema {
		t.Fatal("tools/list missing ask schema")
	}

	rec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"ask",
		"method":"tools/call",
		"params":{"name":"ask","arguments":{"question":"Who wrote it?"}}
	}`, "owner@example.com")
	if rec.Code != http.StatusOK {
		t.Fatalf("ask status = %d, want 200", rec.Code)
	}
	var body struct {
		Found     bool   `json:"found"`
		Answer    string `json:"answer"`
		Citations []struct {
			Path  string `json:"path"`
			Title string `json:"title"`
		} `json:"citations"`
	}
	decodeToolText(t, rec.Body.Bytes(), &body)
	if !body.Found || body.Answer != "Ada wrote the note." {
		t.Fatalf("ask body = %#v, want found answer text", body)
	}
	if len(body.Citations) != 1 || body.Citations[0].Path != "person/ada" || body.Citations[0].Title != "Ada" {
		t.Fatalf("ask citations = %#v, want path/title citation", body.Citations)
	}
}

func TestMCPToolsAreBehindRequireIdentity(t *testing.T) {
	// R-MZLQ-34IK
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithIngestService(&capturingWiki{})))
	rec := callMCP(t, h, `{"jsonrpc":"2.0","id":"list","method":"tools/list"}`, "")

	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401 without authenticated identity", rec.Code)
	}
	if got := rec.Header().Get("WWW-Authenticate"); got == "" {
		t.Fatal("WWW-Authenticate challenge is empty")
	}
	var body map[string]any
	decodeJSON(t, rec.Body.Bytes(), &body)
	if body["error"] != "unauthorized" {
		t.Fatalf("error = %v, want unauthorized", body["error"])
	}
}

type capturingWiki struct {
	ingestID    string
	ingestOwner string
	ingestText  string
	ingestTitle string
	ingestTags  []string
	statusJobID string
	status      jobStatus
	statusErr   error
	claims      []claim
	claimsErr   error
	page        page
	pagePath    string
	pageErr     error
	subjects    []subject
}

func (w *capturingWiki) Ingest(_ context.Context, owner, text, title string, tags []string) (string, error) {
	w.ingestOwner = owner
	w.ingestText = text
	w.ingestTitle = title
	w.ingestTags = append([]string(nil), tags...)
	if w.ingestID == "" {
		return "job-1", nil
	}
	return w.ingestID, nil
}

func (w *capturingWiki) JobStatus(_ context.Context, jobID string) (jobStatus, error) {
	w.statusJobID = jobID
	if w.statusErr != nil {
		return jobStatus{}, w.statusErr
	}
	return w.status, nil
}

func (w *capturingWiki) Subjects(_ context.Context, _, _ string) ([]subject, error) {
	return w.subjects, nil
}

func (w *capturingWiki) ClaimsBySubject(_ context.Context, _ string) ([]claim, error) {
	if w.claimsErr != nil {
		return nil, w.claimsErr
	}
	return w.claims, nil
}

func (w *capturingWiki) PageByPath(_ context.Context, path string) (page, error) {
	w.pagePath = path
	if w.pageErr != nil {
		return page{}, w.pageErr
	}
	return w.page, nil
}

type jobStatus struct {
	ID         string     `json:"job_id"`
	Status     string     `json:"status"`
	ReceivedAt time.Time  `json:"received_at"`
	StartedAt  *time.Time `json:"started_at"`
	FinishedAt *time.Time `json:"finished_at"`
	Error      string     `json:"error"`
	Subjects   []string   `json:"subjects"`
}

type subject struct {
	ID       string
	Name     string
	NormName string
	Type     string
	HasPage  bool
}

type claim struct {
	ID        string
	SubjectID string
	JobID     string
	Body      string
}

type page struct {
	ID        string
	SubjectID string
	Title     string
	Body      string
}

type answer struct {
	Found     bool
	Text      string
	Citations []citation
}

type citation struct {
	Path  string
	Title string
}

type capturingAsker struct {
	owner    string
	question string
	answer   answer
}

func (a *capturingAsker) Ask(_ context.Context, owner, question string) (answer, error) {
	a.owner = owner
	a.question = question
	return a.answer, nil
}

func gatedHandler(t *testing.T, mcp http.Handler) http.Handler {
	t.Helper()
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		MCP:        mcp,
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}
	return srv.Handler
}

func callMCP(t *testing.T, h http.Handler, body, owner string) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest(http.MethodPost, "/mcp", bytes.NewBufferString(body))
	if owner != "" {
		req.Header.Set("X-Owner-Email", owner)
		req.Header.Set("X-Client-Id", "client-1")
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	return rec
}

func decodeToolText(t *testing.T, raw []byte, dst any) {
	t.Helper()
	decodeJSON(t, []byte(toolTextString(t, raw)), dst)
}

func toolTextString(t *testing.T, raw []byte) string {
	t.Helper()
	var got struct {
		Result struct {
			Content []struct {
				Text string `json:"text"`
			} `json:"content"`
		} `json:"result"`
	}
	decodeJSON(t, raw, &got)
	if len(got.Result.Content) != 1 {
		t.Fatalf("content len = %d, want 1", len(got.Result.Content))
	}
	return got.Result.Content[0].Text
}

func decodeJSON(t *testing.T, raw []byte, dst any) {
	t.Helper()
	if err := json.Unmarshal(raw, dst); err != nil {
		t.Fatalf("decode JSON %s: %v", string(raw), err)
	}
}
