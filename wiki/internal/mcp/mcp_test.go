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

	paging "wiki/internal/page"
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
		WithJobAbortService(&capturingWiki{}),
		WithJobRerunService(&capturingWiki{}),
		WithJobListService(&capturingWiki{}),
		WithJobsCountService(&capturingWiki{}),
		WithAskFunc((&capturingAsker{}).Ask),
		WithSubjectListService(&capturingWiki{}),
		WithClaimListService(&capturingWiki{}),
		WithPagePathService(&capturingWiki{}),
		WithLLMCallListService(&capturingCalls{}),
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
		"abort":      true,
		"rerun":      true,
		"jobs":       true,
		"jobs_count": true,
		"ask":        true,
		"subjects":   true,
		"claims":     true,
		"page":       true,
		"llm_calls":  true,
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
	// R-3G73-064M
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithIngestService(&capturingWiki{}),
		WithJobStatusService(&capturingWiki{}),
		WithJobAbortService(&capturingWiki{}),
		WithJobRerunService(&capturingWiki{}),
		WithJobListService(&capturingWiki{}),
		WithJobsCountService(&capturingWiki{}),
		WithAskFunc((&capturingAsker{}).Ask),
		WithSubjectListService(&capturingWiki{}),
		WithClaimListService(&capturingWiki{}),
		WithPagePathService(&capturingWiki{}),
		WithLLMCallListService(&capturingCalls{}),
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
	optionalOnly := map[string]bool{"jobs": false, "jobs_count": false, "llm_calls": false}
	for _, tool := range got.Result.Tools {
		if tool.InputSchema["type"] != "object" {
			t.Fatalf("%s schema type = %v, want object", tool.Name, tool.InputSchema["type"])
		}
		required, ok := tool.InputSchema["required"]
		if _, tracked := optionalOnly[tool.Name]; tracked {
			optionalOnly[tool.Name] = true
			if ok {
				t.Fatalf("%s schema required = %#v, want omitted", tool.Name, required)
			}
			continue
		}
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
	for name, seen := range optionalOnly {
		if !seen {
			t.Fatalf("tools/list missing %s tool", name)
		}
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
				var body struct {
					Subjects []struct {
						Path    string `json:"path"`
						Name    string `json:"name"`
						Type    string `json:"type"`
						HasPage bool   `json:"has_page"`
					} `json:"subjects"`
					Next string `json:"next_cursor"`
				}
				decodeJSON(t, []byte(text), &body)
				if !strings.Contains(text, `"has_page"`) {
					t.Fatalf("subjects body = %s, want has_page field", text)
				}
				if len(body.Subjects) != 1 || body.Subjects[0].Path != "entity/acme-robotics" || body.Subjects[0].Name != "Acme Robotics" || body.Subjects[0].Type != "entity" || body.Next != "" {
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
				var body struct {
					Claims []struct {
						ID   string `json:"id"`
						Text string `json:"text"`
						Job  string `json:"job"`
					} `json:"claims"`
					Next string `json:"next_cursor"`
				}
				decodeJSON(t, []byte(text), &body)
				for _, forbidden := range []string{`"path"`, `"body"`} {
					if strings.Contains(text, forbidden) {
						t.Fatalf("claims body = %s, leaked %s", text, forbidden)
					}
				}
				if len(body.Claims) != 1 || body.Claims[0].ID != "claim-1" || body.Claims[0].Text != "Acme Robotics runs a Tulsa lab." || body.Claims[0].Job != "job-123" || body.Next != "" {
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

func TestJobControlToolsCallDomainServices(t *testing.T) {
	// R-38VO-PJOG
	wiki := &capturingWiki{
		abortResult: abortResult{Aborted: true, Status: "aborted"},
		rerunResult: rerunResult{Requeued: true, Status: "pending"},
	}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithJobAbortService(wiki),
		WithJobRerunService(wiki),
	))

	abortRec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"abort",
		"method":"tools/call",
		"params":{"name":"abort","arguments":{"job_id":"job-123"}}
	}`, "owner@example.com")
	if abortRec.Code != http.StatusOK {
		t.Fatalf("abort status = %d, want 200", abortRec.Code)
	}
	if wiki.abortJobID != "job-123" {
		t.Fatalf("abort job id = %q, want job-123", wiki.abortJobID)
	}
	var abortBody struct {
		Aborted bool   `json:"aborted"`
		Status  string `json:"status"`
	}
	decodeToolText(t, abortRec.Body.Bytes(), &abortBody)
	if !abortBody.Aborted || abortBody.Status != "aborted" {
		t.Fatalf("abort body = %#v, want aborted result", abortBody)
	}

	rerunRec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"rerun",
		"method":"tools/call",
		"params":{"name":"rerun","arguments":{"job_id":"job-456"}}
	}`, "owner@example.com")
	if rerunRec.Code != http.StatusOK {
		t.Fatalf("rerun status = %d, want 200", rerunRec.Code)
	}
	if wiki.rerunJobID != "job-456" {
		t.Fatalf("rerun job id = %q, want job-456", wiki.rerunJobID)
	}
	var rerunBody struct {
		Requeued bool   `json:"requeued"`
		Status   string `json:"status"`
	}
	decodeToolText(t, rerunRec.Body.Bytes(), &rerunBody)
	if !rerunBody.Requeued || rerunBody.Status != "pending" {
		t.Fatalf("rerun body = %#v, want requeued result", rerunBody)
	}
}

func TestJobsCountUsesSameFiltersAsJobsAndReturnsOnlyCount(t *testing.T) {
	// R-Y36L-E3W6
	// R-37NS-BRXR
	started := time.Date(2026, 6, 22, 12, 30, 0, 0, time.UTC)
	wiki := &capturingWiki{
		jobs: []job{
			{ID: "job-done", Status: "done", ReceivedAt: started},
			{ID: "job-failed", Status: "failed", ReceivedAt: started.Add(-time.Hour)},
		},
		jobCount: 2,
	}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithJobListService(wiki),
		WithJobsCountService(wiki),
	))
	args := `"arguments":{"status":["done","failed"],"since":"2026-06-22T00:00:00Z","until":"2026-06-23T00:00:00Z"}`

	jobsRec := callMCP(t, h, `{"jsonrpc":"2.0","id":"jobs","method":"tools/call","params":{"name":"jobs",`+args+`}}`, "owner@example.com")
	if jobsRec.Code != http.StatusOK {
		t.Fatalf("jobs status = %d, want 200", jobsRec.Code)
	}
	var jobsBody struct {
		Jobs []struct {
			ID     string `json:"id"`
			Status string `json:"status"`
		} `json:"jobs"`
		Next string `json:"next_cursor"`
	}
	decodeToolText(t, jobsRec.Body.Bytes(), &jobsBody)
	if len(wiki.jobFilter.Statuses) != 2 || wiki.jobFilter.Statuses[0] != "done" || wiki.jobFilter.Statuses[1] != "failed" {
		t.Fatalf("jobs statuses = %#v, want done+failed", wiki.jobFilter.Statuses)
	}
	if len(jobsBody.Jobs) != 2 || jobsBody.Jobs[0].ID != "job-done" || jobsBody.Jobs[1].Status != "failed" {
		t.Fatalf("jobs body = %#v, want filtered job items", jobsBody)
	}

	countRec := callMCP(t, h, `{"jsonrpc":"2.0","id":"count","method":"tools/call","params":{"name":"jobs_count",`+args+`}}`, "owner@example.com")
	if countRec.Code != http.StatusOK {
		t.Fatalf("jobs_count status = %d, want 200", countRec.Code)
	}
	var countBody map[string]any
	decodeToolText(t, countRec.Body.Bytes(), &countBody)
	if countBody["count"] != float64(2) {
		t.Fatalf("jobs_count body = %#v, want count 2", countBody)
	}
	if _, ok := countBody["jobs"]; ok {
		t.Fatalf("jobs_count body = %#v, want no jobs array", countBody)
	}
	if _, ok := countBody["next_cursor"]; ok {
		t.Fatalf("jobs_count body = %#v, want no cursor", countBody)
	}
	if len(wiki.jobCountFilter.Statuses) != 2 || wiki.jobCountFilter.Statuses[0] != "done" || wiki.jobCountFilter.Statuses[1] != "failed" {
		t.Fatalf("jobs_count statuses = %#v, want done+failed", wiki.jobCountFilter.Statuses)
	}
	if !wiki.jobCountFilter.Since.Equal(wiki.jobFilter.Since) || !wiki.jobCountFilter.Until.Equal(wiki.jobFilter.Until) {
		t.Fatalf("jobs_count filter = %#v, want same time bounds as jobs %#v", wiki.jobCountFilter, wiki.jobFilter)
	}
}

func TestJobsStatusSchemaPublishesEnumAndRejectsUnknownStatus(t *testing.T) {
	// R-Y4EH-RVMV
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithJobListService(&capturingWiki{}),
		WithJobsCountService(&capturingWiki{}),
	))
	rec := callMCP(t, h, `{"jsonrpc":"2.0","id":"list","method":"tools/list"}`, "owner@example.com")
	if rec.Code != http.StatusOK {
		t.Fatalf("tools/list status = %d, want 200", rec.Code)
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
	for _, name := range []string{"jobs", "jobs_count"} {
		schema := toolSchema(t, got.Result.Tools, name)
		properties := schema["properties"].(map[string]any)
		status := properties["status"].(map[string]any)
		items := status["items"].(map[string]any)
		enum := items["enum"].([]any)
		if status["type"] != "array" || len(enum) != 5 {
			t.Fatalf("%s status schema = %#v, want array enum of five states", name, status)
		}
		for _, want := range []string{"pending", "working", "done", "failed", "aborted"} {
			if !containsJSONValue(enum, want) {
				t.Fatalf("%s status enum = %#v, missing %s", name, enum, want)
			}
		}
	}

	for _, name := range []string{"jobs", "jobs_count"} {
		rec := callMCP(t, h, `{"jsonrpc":"2.0","id":"bad","method":"tools/call","params":{"name":"`+name+`","arguments":{"status":["paused"]}}}`, "owner@example.com")
		text := toolTextString(t, rec.Body.Bytes())
		if !strings.Contains(text, "status must be one of pending, working, done, failed, aborted") {
			t.Fatalf("%s error = %q, want valid status set", name, text)
		}
	}
}

func TestJobsRejectMalformedTimeAndCursorFilters(t *testing.T) {
	// R-3EZ6-MEDX
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil, WithJobListService(&capturingWiki{})))
	for _, tc := range []struct {
		name string
		body string
		want string
	}{
		{
			name: "since",
			body: `{"jsonrpc":"2.0","id":"bad-since","method":"tools/call","params":{"name":"jobs","arguments":{"since":"yesterday"}}}`,
			want: "since must be RFC3339",
		},
		{
			name: "until",
			body: `{"jsonrpc":"2.0","id":"bad-until","method":"tools/call","params":{"name":"jobs","arguments":{"until":"tomorrow"}}}`,
			want: "until must be RFC3339",
		},
		{
			name: "cursor",
			body: `{"jsonrpc":"2.0","id":"bad-cursor","method":"tools/call","params":{"name":"jobs","arguments":{"cursor":"not a cursor"}}}`,
			want: "cursor is invalid",
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			rec := callMCP(t, h, tc.body, "owner@example.com")
			text := toolTextString(t, rec.Body.Bytes())
			if !strings.Contains(text, tc.want) {
				t.Fatalf("error = %q, want %q", text, tc.want)
			}
		})
	}
}

func TestPaginatedListToolsForwardFiltersAndReturnNextCursors(t *testing.T) {
	// R-3A3L-3BF5
	// R-3BBH-H35U
	// R-3CJD-UUWJ
	started := time.Date(2026, 6, 22, 12, 30, 0, 0, time.UTC)
	jobCursor := paging.EncodeCursor("2026-06-22T00:00:00Z", "job-cursor")
	wiki := &capturingWiki{
		jobs: []job{{
			ID:         "job-123",
			Owner:      "owner@example.com",
			Title:      "Source",
			Tags:       []string{"one"},
			Status:     "done",
			ReceivedAt: started,
		}},
		jobNext: "job-next",
		subjects: []subject{{
			Name:     "Acme Robotics",
			NormName: "acme robotics",
			Type:     "entity",
			HasPage:  true,
		}},
		subjectNext: "subject-next",
		claims: []claim{{
			ID:    "claim-1",
			JobID: "job-123",
			Body:  "Acme Robotics runs a Tulsa lab.",
		}},
		claimsNext: "claim-next",
	}
	calls := &capturingCalls{
		calls: []callRecord{{
			ID:        "call-1",
			Stage:     "extract",
			JobID:     "job-123",
			Attempt:   2,
			Provider:  "test-provider",
			Model:     "test-model",
			Usage:     `{"total":3}`,
			StartedAt: started,
		}},
		next: "call-next",
	}
	h := gatedHandler(t, NewHandler("test-version", "wiki", nil,
		WithJobListService(wiki),
		WithSubjectListService(wiki),
		WithClaimListService(wiki),
		WithLLMCallListService(calls),
	))

	jobsRec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"jobs",
		"method":"tools/call",
		"params":{"name":"jobs","arguments":{"status":["done"],"since":"2026-06-22T00:00:00Z","until":"2026-06-23T00:00:00Z","limit":1,"cursor":"`+jobCursor+`"}}
	}`, "owner@example.com")
	var jobsBody struct {
		Jobs []struct {
			ID     string   `json:"id"`
			Status string   `json:"status"`
			Tags   []string `json:"tags"`
		} `json:"jobs"`
		Next string `json:"next_cursor"`
	}
	decodeToolText(t, jobsRec.Body.Bytes(), &jobsBody)
	if len(wiki.jobFilter.Statuses) != 1 || wiki.jobFilter.Statuses[0] != "done" || wiki.jobPage.Limit != 1 || wiki.jobPage.Cursor != jobCursor {
		t.Fatalf("job list args = %#v/%#v, want forwarded filter/page", wiki.jobFilter, wiki.jobPage)
	}
	if len(jobsBody.Jobs) != 1 || jobsBody.Jobs[0].ID != "job-123" || jobsBody.Jobs[0].Status != "done" || jobsBody.Next != "job-next" {
		t.Fatalf("jobs body = %#v, want paginated job result", jobsBody)
	}

	subjectsRec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"subjects",
		"method":"tools/call",
		"params":{"name":"subjects","arguments":{"type":"entity","name":"robot","limit":2,"cursor":"subject-cursor"}}
	}`, "owner@example.com")
	var subjectsBody struct {
		Subjects []struct {
			Path    string `json:"path"`
			HasPage bool   `json:"has_page"`
		} `json:"subjects"`
		Next string `json:"next_cursor"`
	}
	decodeToolText(t, subjectsRec.Body.Bytes(), &subjectsBody)
	if wiki.subjectType != "entity" || wiki.subjectName != "robot" || wiki.subjectPage.Limit != 2 || wiki.subjectPage.Cursor != "subject-cursor" {
		t.Fatalf("subject list args = %q/%q/%#v, want forwarded filter/page", wiki.subjectType, wiki.subjectName, wiki.subjectPage)
	}
	if len(subjectsBody.Subjects) != 1 || subjectsBody.Subjects[0].Path != "entity/acme-robotics" || !subjectsBody.Subjects[0].HasPage || subjectsBody.Next != "subject-next" {
		t.Fatalf("subjects body = %#v, want paginated subject result", subjectsBody)
	}

	claimsRec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"claims",
		"method":"tools/call",
		"params":{"name":"claims","arguments":{"subject":"entity/acme-robotics","limit":3,"cursor":"claim-cursor"}}
	}`, "owner@example.com")
	var claimsBody struct {
		Claims []struct {
			ID   string `json:"id"`
			Text string `json:"text"`
		} `json:"claims"`
		Next string `json:"next_cursor"`
	}
	decodeToolText(t, claimsRec.Body.Bytes(), &claimsBody)
	if wiki.claimsSubject != "entity/acme-robotics" || wiki.claimsPage.Limit != 3 || wiki.claimsPage.Cursor != "claim-cursor" {
		t.Fatalf("claims args = %q/%#v, want forwarded subject/page", wiki.claimsSubject, wiki.claimsPage)
	}
	if len(claimsBody.Claims) != 1 || claimsBody.Claims[0].ID != "claim-1" || claimsBody.Claims[0].Text == "" || claimsBody.Next != "claim-next" {
		t.Fatalf("claims body = %#v, want paginated claims result", claimsBody)
	}

	callsRec := callMCP(t, h, `{
		"jsonrpc":"2.0",
		"id":"llm_calls",
		"method":"tools/call",
		"params":{"name":"llm_calls","arguments":{"job_id":"job-123","stage":"extract","since":"2026-06-22T00:00:00Z","until":"2026-06-23T00:00:00Z","limit":4,"cursor":"call-cursor"}}
	}`, "owner@example.com")
	var callsBody struct {
		Calls []struct {
			ID       string `json:"id"`
			Stage    string `json:"stage"`
			JobID    string `json:"job_id"`
			Attempt  int64  `json:"attempt"`
			Provider string `json:"provider"`
		} `json:"llm_calls"`
		Next string `json:"next_cursor"`
	}
	decodeToolText(t, callsRec.Body.Bytes(), &callsBody)
	if calls.filter.JobID != "job-123" || calls.filter.Stage != "extract" || calls.params.Limit != 4 || calls.params.Cursor != "call-cursor" {
		t.Fatalf("call list args = %#v/%#v, want forwarded filter/page", calls.filter, calls.params)
	}
	if len(callsBody.Calls) != 1 || callsBody.Calls[0].ID != "call-1" || callsBody.Calls[0].Attempt != 2 || callsBody.Calls[0].Provider != "test-provider" || callsBody.Next != "call-next" {
		t.Fatalf("llm_calls body = %#v, want paginated footprint result", callsBody)
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
	ingestID       string
	ingestOwner    string
	ingestText     string
	ingestTitle    string
	ingestTags     []string
	statusJobID    string
	status         jobStatus
	statusErr      error
	abortJobID     string
	abortResult    abortResult
	rerunJobID     string
	rerunResult    rerunResult
	jobs           []job
	jobFilter      JobFilter
	jobPage        paging.Params
	jobNext        string
	jobCount       int
	jobCountFilter JobFilter
	claims         []claim
	claimsErr      error
	claimsSubject  string
	claimsPage     paging.Params
	claimsNext     string
	page           page
	pagePath       string
	pageErr        error
	subjects       []subject
	subjectType    string
	subjectName    string
	subjectPage    paging.Params
	subjectNext    string
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

func (w *capturingWiki) Abort(_ context.Context, jobID string) (abortResult, error) {
	w.abortJobID = jobID
	return w.abortResult, nil
}

func (w *capturingWiki) Rerun(_ context.Context, jobID string) (rerunResult, error) {
	w.rerunJobID = jobID
	return w.rerunResult, nil
}

func (w *capturingWiki) ListJobs(_ context.Context, f JobFilter, p paging.Params) ([]job, string, error) {
	w.jobFilter = f
	w.jobPage = p
	return w.jobs, w.jobNext, nil
}

func (w *capturingWiki) CountJobs(_ context.Context, f JobFilter) (int, error) {
	w.jobCountFilter = f
	return w.jobCount, nil
}

func (w *capturingWiki) Subjects(_ context.Context, _, _ string) ([]subject, error) {
	return w.subjects, nil
}

func (w *capturingWiki) List(_ context.Context, typ, name string, p paging.Params) ([]subject, string, error) {
	w.subjectType = typ
	w.subjectName = name
	w.subjectPage = p
	return w.subjects, w.subjectNext, nil
}

func (w *capturingWiki) ClaimsBySubject(_ context.Context, _ string) ([]claim, error) {
	if w.claimsErr != nil {
		return nil, w.claimsErr
	}
	return w.claims, nil
}

func (w *capturingWiki) ListBySubject(_ context.Context, subject string, p paging.Params) ([]claim, string, error) {
	w.claimsSubject = subject
	w.claimsPage = p
	if w.claimsErr != nil {
		return nil, "", w.claimsErr
	}
	return w.claims, w.claimsNext, nil
}

func (w *capturingWiki) PageByPath(_ context.Context, path string) (page, error) {
	w.pagePath = path
	if w.pageErr != nil {
		return page{}, w.pageErr
	}
	return w.page, nil
}

type abortResult struct {
	Aborted bool
	Status  string
}

type rerunResult struct {
	Requeued bool
	Status   string
}

type job struct {
	ID         string
	Owner      string
	Title      string
	Tags       []string
	Status     string
	ReceivedAt time.Time
	StartedAt  time.Time
	FinishedAt time.Time
	Error      string
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

type callRecord struct {
	ID        string
	Stage     string
	JobID     string
	Attempt   int
	Provider  string
	Model     string
	Params    string
	Request   string
	Response  string
	Usage     string
	Err       string
	StartedAt time.Time
	EndedAt   time.Time
}

type capturingCalls struct {
	calls  []callRecord
	filter LLMCallFilter
	params paging.Params
	next   string
}

func (c *capturingCalls) List(_ context.Context, f LLMCallFilter, p paging.Params) ([]callRecord, string, error) {
	c.filter = f
	c.params = p
	return c.calls, c.next, nil
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

func toolSchema(t *testing.T, tools []struct {
	Name        string         `json:"name"`
	InputSchema map[string]any `json:"inputSchema"`
}, name string) map[string]any {
	t.Helper()
	for _, tool := range tools {
		if tool.Name == name {
			return tool.InputSchema
		}
	}
	t.Fatalf("tools/list missing %s", name)
	return nil
}

func containsJSONValue(values []any, want string) bool {
	for _, value := range values {
		if value == want {
			return true
		}
	}
	return false
}
