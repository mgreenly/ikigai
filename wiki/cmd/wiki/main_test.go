package main

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"regexp"
	"strings"
	"sync"
	"testing"
	"time"

	"appkit"
	appdb "appkit/db"
	"appkit/manifest"
	"appkit/server"
	appkitweb "appkit/web"
	agentkit "github.com/ikigenba/agentkit"
	"registry"

	"wiki/internal/ask"
	"wiki/internal/compile"
	wikidb "wiki/internal/db"
	"wiki/internal/extract"
	"wiki/internal/llm"
	"wiki/internal/mcp"
	paging "wiki/internal/page"
	"wiki/internal/web"
	"wiki/internal/wiki"
)

func testWWWRoot(t *testing.T) string {
	t.Helper()

	root, err := filepath.Abs(filepath.Join("..", "..", "share", "www"))
	if err != nil {
		t.Fatalf("resolve test www root: %v", err)
	}
	return root
}

// R-8DF1-W89F
func TestCommittedManifestIsPortable(t *testing.T) {
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}
	if bytes.Contains(committed, []byte("/opt/")) {
		t.Fatalf("committed manifest.env contains on-box /opt/ path:\n%s", committed)
	}
	for _, line := range bytes.Split(committed, []byte("\n")) {
		if bytes.HasPrefix(line, []byte("WIKI_DB_PATH=")) || bytes.HasPrefix(line, []byte("WIKI_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	// R-JFR5-MJW9
	spec := newSpec(wiki.NewConfig)
	got := manifest.Emit(manifest.Fields{
		App:      spec.App,
		Mount:    spec.Mount,
		Default:  spec.Default,
		Port:     spec.Port,
		MCP:      spec.MCP,
		Feed:     spec.Feed,
		Consumes: spec.Consumes,
		Extras:   manifestExtras(spec.ManifestExtras),
	})
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}

	if got != string(committed) {
		t.Fatalf("manifest.Emit output != committed etc/manifest.env\n--- emit ---\n%s\n--- committed ---\n%s", got, committed)
	}
}

func TestSpecDeclaresServedMCPService(t *testing.T) {
	// R-JDBC-V0EV
	spec := newSpec(wiki.NewConfig)
	if spec.App != "wiki" {
		t.Fatalf("App = %q, want wiki", spec.App)
	}
	if spec.Mount != "/srv/wiki/" {
		t.Fatalf("Mount = %q, want /srv/wiki/", spec.Mount)
	}
	if want := registry.MustPort("wiki"); spec.Port != want {
		t.Fatalf("Port = %d, want registry wiki port %d", spec.Port, want)
	}
	if !spec.MCP {
		t.Fatal("MCP = false, want true")
	}
	if spec.Handlers == nil {
		t.Fatal("Handlers is nil; service would not mount /mcp")
	}
	if spec.Config == nil {
		t.Fatal("Config is nil; service would not read LLM configuration")
	}
	if len(spec.Workers) != 1 {
		t.Fatalf("Workers len = %d, want 1", len(spec.Workers))
	}
}

// R-4LKF-FB23
func TestWikiBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "wiki")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	libexecDir := filepath.Join(appRoot, "libexec")
	binDir := filepath.Join(appRoot, "bin")
	etcDir := filepath.Join(appRoot, "etc")
	shareDir := filepath.Join(appRoot, "share")
	for _, dir := range []string{stateDir, cacheDir, libexecDir, binDir, etcDir, shareDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}

	versionBytes, err := os.ReadFile(filepath.Join("..", "..", "VERSION"))
	if err != nil {
		t.Fatalf("read VERSION: %v", err)
	}
	version := strings.TrimSpace(string(versionBytes))
	if !regexp.MustCompile(`^v[0-9]+\.[0-9]+\.[0-9]+$`).MatchString(version) {
		t.Fatalf("VERSION = %q, want v-prefixed SemVer", version)
	}

	committedManifest, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}
	etcVersionDir := filepath.Join(etcDir, version)
	shareVersionDir := filepath.Join(shareDir, version)
	for _, dir := range []string{etcVersionDir, shareVersionDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}
	if err := os.CopyFS(filepath.Join(shareVersionDir, "www"), os.DirFS(testWWWRoot(t))); err != nil {
		t.Fatalf("copy www fixture: %v", err)
	}
	shippedManifest := filepath.Join(etcVersionDir, "manifest.env")
	if err := os.WriteFile(shippedManifest, committedManifest, 0o644); err != nil {
		t.Fatalf("write shipped manifest.env: %v", err)
	}
	if err := os.Symlink(version, filepath.Join(etcDir, "current")); err != nil {
		t.Fatalf("symlink etc/current: %v", err)
	}
	if err := os.Symlink(version, filepath.Join(shareDir, "current")); err != nil {
		t.Fatalf("symlink share/current: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(filepath.Join(etcDir, "current")); err != nil || resolved != etcVersionDir {
		t.Fatalf("etc/current resolves to %q err=%v, want %q", resolved, err, etcVersionDir)
	}
	if resolved, err := filepath.EvalSymlinks(filepath.Join(shareDir, "current")); err != nil || resolved != shareVersionDir {
		t.Fatalf("share/current resolves to %q err=%v, want %q", resolved, err, shareVersionDir)
	}
	selectedManifest, err := os.ReadFile(filepath.Join(etcDir, "current", "manifest.env"))
	if err != nil {
		t.Fatalf("read selected manifest.env: %v", err)
	}
	if !bytes.Equal(selectedManifest, committedManifest) {
		t.Fatalf("selected manifest.env differs from committed authored file\n--- selected ---\n%s\n--- committed ---\n%s", selectedManifest, committedManifest)
	}

	binary := filepath.Join(libexecDir, "wiki-"+version)
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build wiki: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/wiki-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "wiki.db")
	generationPath := filepath.Join(cacheDir, "wiki.db.generation")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = testEnv(map[string]string{
		"IKIGENBA_DOMAIN":   "int.ikigenba.com",
		"IKIGENBA_ROOT":     root,
		"WIKI_IP":           "127.0.0.1",
		"WIKI_PORT":         fmt.Sprintf("%d", port),
		"ANTHROPIC_API_KEY": "test-anthropic-key",
		"OPENAI_API_KEY":    "test-openai-key",
	})
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start wiki: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "wiki" {
		t.Fatalf("health service = %v, want wiki; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("wiki did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("wiki did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
}

func TestServeFailsLoudWhenAnthropicKeyMissing(t *testing.T) {
	// R-6RVX-P1IG
	for _, value := range []string{"", "   "} {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()

		cmd := exec.CommandContext(ctx, "go", "run", ".", "serve")
		root := t.TempDir()
		cmd.Env = testEnv(map[string]string{
			"IKIGENBA_DOMAIN":      "int.ikigenba.com",
			"IKIGENBA_ROOT":        root,
			"WIKI_DB_PATH":         filepath.Join(root, "wiki.db"),
			"WIKI_GENERATION_PATH": filepath.Join(root, "wiki.db.generation"),
			"WIKI_WWW_PATH":        testWWWRoot(t),
			"WIKI_IP":              "127.0.0.1",
			"WIKI_PORT":            fmt.Sprintf("%d", freeTCPPort(t)),
			"OPENAI_API_KEY":       "test-openai-key",
			"ANTHROPIC_API_KEY":    value,
		})
		out, err := cmd.CombinedOutput()
		if ctx.Err() == context.DeadlineExceeded {
			t.Fatal("serve did not fail before startup timeout")
		}
		if err == nil {
			t.Fatalf("serve with ANTHROPIC_API_KEY=%q exited 0; output:\n%s", value, out)
		}
		if !strings.Contains(string(out), "ANTHROPIC_API_KEY is required") {
			t.Fatalf("serve output = %q, want missing-key error", out)
		}
	}
}

func TestBuildSpecWiresSixteenMCPTools(t *testing.T) {
	// R-MUQ4-K1JS
	// R-3G73-064M
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	spec := newSpec(staticConfig(wiki.Config{
		SearchDefault: 8,
		SearchCap:     32,
	}))
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		Register:   spec.Handlers,
		DB:         conn,
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}

	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(`{"jsonrpc":"2.0","id":"list","method":"tools/list"}`))
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-1")
	rec := httptest.NewRecorder()
	srv.Handler.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	var got struct {
		Result struct {
			Tools []struct {
				Name string `json:"name"`
			} `json:"tools"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &got); err != nil {
		t.Fatalf("decode tools/list response: %v", err)
	}
	names := make(map[string]bool, len(got.Result.Tools))
	for _, tool := range got.Result.Tools {
		names[tool.Name] = true
	}
	want := []string{"ingest", "status", "abort", "rerun", "jobs", "jobs_count", "merge", "merges", "ask", "subjects", "claims", "page", "llm_calls", "guide", "health", "reflection"}
	if len(names) != len(want) {
		t.Fatalf("tool names = %#v, want exact %v", names, want)
	}
	for _, name := range want {
		if !names[name] {
			t.Fatalf("tool names = %#v, missing %s", names, name)
		}
	}
}

func TestBuildSpecPageToolReturnsRenderedFooter(t *testing.T) {
	// R-02WN-BXPK
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	internalSubjectID := "01HZX4Q0SUBJECTULID00000001"
	subjects := wiki.NewSubjectStore(conn)
	pages := wiki.NewPageStore(conn)
	for _, subject := range []wiki.Subject{
		{ID: internalSubjectID, Name: "Acme Robotics", NormName: "acme-robotics", Type: "entity"},
		{ID: "subject-tulsa", Name: "Tulsa Launch", NormName: "tulsa-launch", Type: "event"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save subject %s: %v", subject.ID, err)
		}
	}
	for _, page := range []wiki.Page{
		{
			ID:        "page-acme",
			SubjectID: internalSubjectID,
			Title:     "Acme Robotics",
			Body:      "Acme Robotics coordinated Tulsa Launch.",
		},
		{
			ID:        "page-tulsa",
			SubjectID: "subject-tulsa",
			Title:     "Tulsa Launch",
			Body:      "Tulsa Launch was coordinated by Acme Robotics.",
		},
	} {
		if err := pages.Upsert(ctx, page); err != nil {
			t.Fatalf("Upsert page %s: %v", page.ID, err)
		}
	}

	spec := newSpec(staticConfig(wiki.Config{}))
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		Register:   spec.Handlers,
		DB:         conn,
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(`{
		"jsonrpc":"2.0",
		"id":"page",
		"method":"tools/call",
		"params":{"name":"page","arguments":{"subject":"entity/acme-robotics"}}
	}`))
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-1")
	rec := httptest.NewRecorder()
	srv.Handler.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	var body struct {
		Subject string `json:"subject"`
		Title   string `json:"title"`
		Body    string `json:"body"`
	}
	decodeMCPToolText(t, rec.Body.Bytes(), &body)
	if body.Subject != "entity/acme-robotics" || body.Title != "Acme Robotics" {
		t.Fatalf("page = %#v, want Acme page", body)
	}
	text := mcpToolText(t, rec.Body.Bytes())
	if strings.Contains(text, internalSubjectID) || strings.Contains(text, "subject_id") || strings.Contains(text, `"path"`) {
		t.Fatalf("page text = %s, want public subject field and no internal ids/path field", text)
	}
	for _, want := range []string{
		"## Links",
		"### Mentions",
		"- [Tulsa Launch](event/tulsa-launch)",
		"### Mentioned by",
		"- [Tulsa Launch](event/tulsa-launch)",
	} {
		if !strings.Contains(body.Body, want) {
			t.Fatalf("page body:\n%s\nmissing %q", body.Body, want)
		}
	}
}

func TestBuildSpecReadToolsReturnPublicPathsWithoutSubjectIDs(t *testing.T) {
	// R-03GW-PX5K
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	internalSubjectID := "01HZX4Q0SUBJECTULID00000001"

	if err := wiki.NewSubjectStore(conn).Save(ctx, wiki.Subject{
		ID:       internalSubjectID,
		Name:     "Acme Robotics",
		NormName: "acme-robotics",
		Type:     "entity",
	}); err != nil {
		t.Fatalf("Save subject: %v", err)
	}
	if err := wiki.NewJobStore(conn).InsertIngest(ctx, wiki.Job{
		ID:         "job-123",
		Owner:      "owner@example.com",
		SourceText: "source",
		Status:     wiki.JobDone,
		ReceivedAt: time.Date(2026, 6, 21, 12, 0, 0, 0, time.UTC),
	}); err != nil {
		t.Fatalf("InsertIngest: %v", err)
	}
	if err := wiki.NewClaimStore(conn).Save(ctx, wiki.Claim{
		ID:        "claim-1",
		SubjectID: internalSubjectID,
		JobID:     "job-123",
		Body:      "Acme Robotics runs a Tulsa lab.",
	}); err != nil {
		t.Fatalf("Save claim: %v", err)
	}
	if err := wiki.NewPageStore(conn).Upsert(ctx, wiki.Page{
		ID:        internalSubjectID,
		SubjectID: internalSubjectID,
		Title:     "Acme Robotics",
		Body:      "Acme Robotics overview.",
	}); err != nil {
		t.Fatalf("Upsert page: %v", err)
	}

	spec := newSpec(staticConfig(wiki.Config{}))
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		Register:   spec.Handlers,
		DB:         conn,
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}

	for _, tc := range []struct {
		name    string
		request string
	}{
		{
			name:    "status",
			request: `{"jsonrpc":"2.0","id":"status","method":"tools/call","params":{"name":"status","arguments":{"job_id":"job-123"}}}`,
		},
		{
			name:    "subjects",
			request: `{"jsonrpc":"2.0","id":"subjects","method":"tools/call","params":{"name":"subjects","arguments":{"type":"entity","name":"acme"}}}`,
		},
		{
			name:    "claims",
			request: `{"jsonrpc":"2.0","id":"claims","method":"tools/call","params":{"name":"claims","arguments":{"subject":"entity/acme-robotics"}}}`,
		},
		{
			name:    "page",
			request: `{"jsonrpc":"2.0","id":"page","method":"tools/call","params":{"name":"page","arguments":{"subject":"entity/acme-robotics"}}}`,
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			text := mcpToolCallText(t, srv.Handler, tc.request)
			if tc.name != "claims" && !strings.Contains(text, "entity/acme-robotics") {
				t.Fatalf("tool text = %s, want public path", text)
			}
			for _, forbidden := range []string{internalSubjectID, "SubjectID", "subject_id", "NormName", "norm_name"} {
				if strings.Contains(text, forbidden) {
					t.Fatalf("tool text = %s, leaked %q", text, forbidden)
				}
			}
		})
	}
}

func TestPathReadServicesResolveFoldedAndSurvivorPathsIdentically(t *testing.T) {
	// R-AL5R-PL1P
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	survivor := wiki.Subject{
		ID:   "subject-survivor",
		Name: "Winner Widget",
		Type: "entity",
	}
	if err := wiki.NewSubjectStore(conn).Save(ctx, survivor); err != nil {
		t.Fatalf("Save survivor: %v", err)
	}
	if err := wiki.NewAliasStore(conn).Insert(ctx, wiki.Alias{
		Name:      "Folded Widget",
		SubjectID: survivor.ID,
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-24T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert alias: %v", err)
	}
	if err := wiki.NewJobStore(conn).InsertIngest(ctx, wiki.Job{
		ID:         "job-1",
		Owner:      "owner@example.com",
		SourceText: "source",
		Status:     wiki.JobDone,
		ReceivedAt: time.Date(2026, 6, 24, 12, 0, 0, 0, time.UTC),
	}); err != nil {
		t.Fatalf("InsertIngest: %v", err)
	}
	if err := wiki.NewClaimStore(conn).Save(ctx, wiki.Claim{
		ID:        "claim-1",
		SubjectID: survivor.ID,
		JobID:     "job-1",
		Body:      "Winner Widget shipped the release.",
	}); err != nil {
		t.Fatalf("Save claim: %v", err)
	}
	if err := wiki.NewPageStore(conn).Upsert(ctx, wiki.Page{
		ID:        "page-survivor",
		SubjectID: survivor.ID,
		Title:     "Winner Widget",
		Body:      "Winner Widget overview.",
	}); err != nil {
		t.Fatalf("Upsert page: %v", err)
	}

	resolver := wiki.NewResolver(conn)
	folded, err := resolver.ResolveByPath(ctx, "entity/folded-widget")
	if err != nil {
		t.Fatalf("ResolveByPath folded: %v", err)
	}
	current, err := resolver.ResolveByPath(ctx, "entity/winner-widget")
	if err != nil {
		t.Fatalf("ResolveByPath survivor: %v", err)
	}
	if folded.ID != survivor.ID || current.ID != survivor.ID {
		t.Fatalf("resolved folded=%+v survivor=%+v, want same survivor", folded, current)
	}

	pageService := pathPageService{
		resolver: resolver,
		service:  wiki.NewService(conn, nil, nil, time.Now),
	}
	foldedPage, err := pageService.PageByPath(ctx, "entity/folded-widget")
	if err != nil {
		t.Fatalf("PageByPath folded: %v", err)
	}
	currentPage, err := pageService.PageByPath(ctx, "entity/winner-widget")
	if err != nil {
		t.Fatalf("PageByPath survivor: %v", err)
	}
	if !reflect.DeepEqual(foldedPage, currentPage) {
		t.Fatalf("folded page = %#v, survivor page = %#v; want byte-identical projection", foldedPage, currentPage)
	}
	if foldedPage.Path != "entity/winner-widget" || foldedPage.Title != "Winner Widget" || !strings.Contains(foldedPage.Body, "Winner Widget overview.") {
		t.Fatalf("folded page = %#v, want survivor public page shape", foldedPage)
	}

	claimService := pathClaimService{
		resolver: resolver,
		claims:   wiki.NewClaimStore(conn),
	}
	foldedClaims, foldedNext, err := claimService.ListBySubject(ctx, "entity/folded-widget", paging.Params{})
	if err != nil {
		t.Fatalf("ListBySubject folded: %v", err)
	}
	currentClaims, currentNext, err := claimService.ListBySubject(ctx, "entity/winner-widget", paging.Params{})
	if err != nil {
		t.Fatalf("ListBySubject survivor: %v", err)
	}
	if foldedNext != currentNext || !reflect.DeepEqual(foldedClaims, currentClaims) {
		t.Fatalf("folded claims = %#v/%q, survivor claims = %#v/%q; want same survivor claims", foldedClaims, foldedNext, currentClaims, currentNext)
	}
	if len(foldedClaims) != 1 || foldedClaims[0].ID != "claim-1" || foldedClaims[0].Text != "Winner Widget shipped the release." {
		t.Fatalf("folded claims = %#v, want survivor claim projection", foldedClaims)
	}
}

func TestSubjectHandlerWithRealPathPageServiceResolvesAliasInboundLinks(t *testing.T) {
	// R-PODT-EU1H
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := wiki.NewSubjectStore(conn)
	for _, subject := range []wiki.Subject{
		{ID: "subject-w", Name: "Giorgio Vasari", Type: "entity"},
		{ID: "subject-f", Name: "Florence Workshop", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save subject %s: %v", subject.ID, err)
		}
	}
	if err := wiki.NewAliasStore(conn).Insert(ctx, wiki.Alias{
		Name:      "Vasari",
		SubjectID: "subject-w",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-24T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert alias: %v", err)
	}
	pages := wiki.NewPageStore(conn)
	for _, page := range []wiki.Page{
		{ID: "page-w", SubjectID: "subject-w", Title: "Giorgio Vasari", Body: "Giorgio Vasari documented Renaissance artists."},
		{ID: "page-f", SubjectID: "subject-f", Title: "Florence Workshop", Body: "The workshop cited Vasari in its notes."},
	} {
		if err := pages.Upsert(ctx, page); err != nil {
			t.Fatalf("Upsert page %s: %v", page.ID, err)
		}
	}

	pageService := pathPageService{
		resolver: wiki.NewResolver(conn),
		service:  wiki.NewService(conn, nil, nil, time.Now),
		webBase:  "https://int.ikigenba.com/srv/wiki/subject/",
	}
	site, err := appkitweb.Load(testWWWRoot(t))
	if err != nil {
		t.Fatalf("load test site: %v", err)
	}
	handler := web.NewHandler("wiki", "v-test", "/srv/wiki/", site, web.WithPageFinder(pageService))
	folded := httptest.NewRecorder()
	handler.ServeHTTP(folded, httptest.NewRequest(http.MethodGet, "/subject/entity/vasari", nil))
	current := httptest.NewRecorder()
	handler.ServeHTTP(current, httptest.NewRequest(http.MethodGet, "/subject/entity/giorgio-vasari", nil))

	if folded.Code != http.StatusOK {
		t.Fatalf("folded status = %d, want 200; body=%s", folded.Code, folded.Body.String())
	}
	if current.Code != http.StatusOK {
		t.Fatalf("current status = %d, want 200; body=%s", current.Code, current.Body.String())
	}
	if folded.Body.String() != current.Body.String() {
		t.Fatalf("folded body differs from current body:\nfolded=%s\ncurrent=%s", folded.Body.String(), current.Body.String())
	}
	body := folded.Body.String()
	for _, want := range []string{
		"<h1>Giorgio Vasari</h1>",
		"<p>Giorgio Vasari documented Renaissance artists.</p>",
		`<nav aria-label="Mentioned by">`,
		`<a href="https://int.ikigenba.com/srv/wiki/subject/entity/florence-workshop">Florence Workshop</a>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("folded subject page missing %q: %s", want, body)
		}
	}
}

func TestWebSubjectLinksUseAbsoluteAuthServerBase(t *testing.T) {
	// R-8I6N-DL0I
	for _, tc := range []struct {
		name       string
		authServer string
	}{
		{name: "production", authServer: "https://acct.ikigenba.com"},
		{name: "local", authServer: "http://localhost:8080"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			ctx := context.Background()
			conn := migratedDB(t, ctx)
			defer conn.Close()
			if err := wiki.NewSubjectStore(conn).Save(ctx, wiki.Subject{
				ID:   "subject-tsr",
				Name: "TSR",
				Type: "entity",
			}); err != nil {
				t.Fatalf("Save subject: %v", err)
			}
			site, err := appkitweb.Load(testWWWRoot(t))
			if err != nil {
				t.Fatalf("load test site: %v", err)
			}
			spec := newSpec(staticConfig(wiki.Config{}))
			srv, err := server.New(server.Options{
				Addr:       "127.0.0.1:0",
				Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
				ResourceID: tc.authServer + "/srv/wiki/mcp",
				AuthServer: tc.authServer,
				Version:    "test-version",
				Service:    "wiki",
				Register:   spec.Handlers,
				WWW:        site,
				DB:         conn,
			})
			if err != nil {
				t.Fatalf("server.New: %v", err)
			}

			rec := httptest.NewRecorder()
			srv.Handler.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
			}
			want := `<a href="` + tc.authServer + `/srv/wiki/subject/entity/tsr">TSR</a>`
			if !strings.Contains(rec.Body.String(), want) {
				t.Fatalf("home page missing absolute subject link %q: %s", want, rec.Body.String())
			}
			if strings.Contains(rec.Body.String(), `href="subject/entity/tsr"`) || strings.Contains(rec.Body.String(), `//srv/wiki/subject/`) {
				t.Fatalf("home page rendered malformed subject base: %s", rec.Body.String())
			}
		})
	}
}

func TestBuildSpecMatchesDirectMCPToolSurface(t *testing.T) {
	// R-04HB-QM7T
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	spec := newSpec(staticConfig(wiki.Config{}))
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		Register:   spec.Handlers,
		DB:         conn,
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}

	var direct http.Handler
	_, err = server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		Register: func(rt *appkit.Router) error {
			var err error
			direct, err = mcp.NewHandler(rt,
				mcp.WithIngestService(surfaceWiki{}),
				mcp.WithJobStatusService(surfaceWiki{}),
				mcp.WithJobAbortService(surfaceWiki{}),
				mcp.WithJobRerunService(surfaceWiki{}),
				mcp.WithJobListService(surfaceWiki{}),
				mcp.WithJobsCountService(surfaceWiki{}),
				mcp.WithMergeService(surfaceWiki{}, surfaceWiki{}),
				mcp.WithMergeListService(surfaceWiki{}),
				mcp.WithAskFunc(surfaceAsk),
				mcp.WithSubjectListService(surfaceWiki{}),
				mcp.WithClaimListService(surfaceWiki{}),
				mcp.WithPagePathService(surfaceWiki{}),
				mcp.WithLLMCallListService(surfaceCalls{}),
			)
			return err
		},
	})
	if err != nil {
		t.Fatalf("direct mcp.NewHandler: %v", err)
	}
	if direct == nil {
		t.Fatal("direct mcp.NewHandler returned nil")
	}

	composedTools := mcpToolSurface(t, srv.Handler, true)
	directTools := mcpToolSurface(t, direct, false)
	if !reflect.DeepEqual(composedTools, directTools) {
		t.Fatalf("tool surface mismatch\ncomposed=%#v\ndirect=%#v", composedTools, directTools)
	}
}

func TestBuildSpecRecordsPageEmbeddingCalls(t *testing.T) {
	// R-LFOY-L6TZ
	// R-SIFE-Z88I
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	prov := &capturingProvider{responses: []string{
		`{"subjects":[{
			"type":"entity",
			"kind":"company",
			"name":"Acme Robotics",
			"occurred_at":"",
			"claims":["Acme Robotics opened a recorded embedding lab."]
		}]}`,
		`{"title":"Acme Robotics","body":"Acme Robotics opened a recorded embedding lab."}`,
	}}
	embeds := &capturingEmbeddingProvider{vectors: [][]float32{{0.6, 0.8}}, usage: agentkit.EmbeddingUsage{InputTokens: 5, Total: 5}}
	extractSite := extract.DefaultCallSite()
	extractSite.Model = "extract-model"
	compileSite := compile.DefaultCallSite()
	compileSite.Model = "compile-model"
	spec := newSpec(staticConfig(wiki.Config{
		CallSites: wiki.CallSites{
			Extract: extractSite,
			Compile: compileSite,
		},
		EmbedSite: wiki.EmbedSite{
			Model:    "recorded-page-embed-model",
			Dims:     2,
			Provider: embeds,
		},
		LLM: llm.New(prov, nil),
	}))
	h := buildSpecTestHandler(t, conn, spec)
	stopWorker, workerErr := startBuildSpecWorker(t, ctx, spec.Workers[0])
	defer stopWorker()

	var ingest struct {
		JobID string `json:"job_id"`
	}
	if err := json.Unmarshal([]byte(mcpToolCallText(t, h, `{
		"jsonrpc":"2.0",
		"id":"ingest",
		"method":"tools/call",
		"params":{"name":"ingest","arguments":{"text":"Acme Robotics opened a recorded embedding lab.","title":"Recorded lab"}}
	}`)), &ingest); err != nil {
		t.Fatalf("decode ingest response: %v", err)
	}
	status := waitBuildSpecJob(t, ctx, conn, ingest.JobID, wiki.JobDone, workerErr)
	if len(status.Subjects) != 1 {
		t.Fatalf("job subjects = %#v, want one embedded subject", status.Subjects)
	}

	calls, _, err := wiki.NewLLMCallStore(conn).List(ctx, wiki.LLMCallFilter{Stage: "embed-page"}, paging.Params{Limit: 10})
	if err != nil {
		t.Fatalf("List embed-page calls: %v", err)
	}
	if len(calls) != 1 {
		t.Fatalf("embed-page calls = %+v, want one page embedding record", calls)
	}
	call := calls[0]
	if call.JobID != ingest.JobID || call.Provider != "capturing-embed" || call.Model != "recorded-page-embed-model" || call.Err != "" {
		t.Fatalf("embed-page call = %+v, want recorded production page embedding footprint", call)
	}
	assertCallJSONField(t, call.Params, "dimensions", float64(2))
	assertCallJSONField(t, call.Request, "role", "document")
	assertCallJSONField(t, call.Response, "vectors", float64(1))
	assertCallJSONField(t, call.Response, "dims", float64(2))
	assertCallJSONField(t, call.Usage, "InputTokens", float64(5))

	requests := embeds.Requests()
	if len(requests) != 1 || requests[0].Role != agentkit.InputDocument || requests[0].Model != "recorded-page-embed-model" || requests[0].Dimensions != 2 {
		t.Fatalf("embedding provider requests = %#v, want one document request from buildSpec page embedder", requests)
	}
}

func TestBuildSpecMergeRemovesLoserVectorFromLiveCache(t *testing.T) {
	// R-WS3C-J4QB
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	saveBuildSpecMergeFixture(t, ctx, conn)

	if err := wiki.NewEmbeddingStore(conn).Upsert(ctx, wiki.Embedding{
		SubjectID:   "subject-loser",
		Model:       "merge-cache-model",
		Dims:        2,
		Vec:         []float32{1, 0},
		ContentHash: "old-loser",
		UpdatedAt:   42,
	}); err != nil {
		t.Fatalf("seed loser embedding: %v", err)
	}

	prov := &capturingProvider{responses: []string{
		`{"title":"Winner Subject","body":"Loser fact.\nWinner fact."}`,
		`{"sub_queries":["meaning lane"],"keywords":[],"aliases":[]}`,
		`{"found":true,"text":"The winner retained the merged page.","citations":[{"path":"entity/winner-subject","title":"Winner Subject"}]}`,
	}}
	embeds := &capturingEmbeddingProvider{vectors: [][]float32{{1, 0}}, usage: agentkit.EmbeddingUsage{InputTokens: 2, Total: 2}}
	compileSite := compile.DefaultCallSite()
	compileSite.Model = "merge-compile-model"
	askSubjectSite := ask.DefaultSubjectCallSite()
	askSubjectSite.Model = "merge-ask-subject-model"
	askSynthesisSite := ask.DefaultSynthesisCallSite()
	askSynthesisSite.Model = "merge-ask-synthesis-model"
	spec := newSpec(staticConfig(wiki.Config{
		CallSites: wiki.CallSites{
			Compile:      compileSite,
			AskSubject:   askSubjectSite,
			AskSynthesis: askSynthesisSite,
		},
		EmbedSite: wiki.EmbedSite{
			Model:    "merge-cache-model",
			Dims:     2,
			Provider: embeds,
		},
		SearchDefault: 1,
		LLM:           llm.New(prov, nil),
	}))
	h := buildSpecTestHandler(t, conn, spec)
	stopWorker, workerErr := startBuildSpecWorker(t, ctx, spec.Workers[0])
	defer stopWorker()

	var merge struct {
		JobID string `json:"job_id"`
	}
	if err := json.Unmarshal([]byte(mcpToolCallText(t, h, `{
		"jsonrpc":"2.0",
		"id":"merge",
		"method":"tools/call",
		"params":{"name":"merge","arguments":{"from":"entity/loser-subject","to":"entity/winner-subject"}}
	}`)), &merge); err != nil {
		t.Fatalf("decode merge response: %v", err)
	}
	waitBuildSpecJob(t, ctx, conn, merge.JobID, wiki.JobDone, workerErr)

	var answer struct {
		Found bool `json:"found"`
	}
	if err := json.Unmarshal([]byte(mcpToolCallText(t, h, `{
		"jsonrpc":"2.0",
		"id":"ask",
		"method":"tools/call",
		"params":{"name":"ask","arguments":{"question":"meaning lane"}}
	}`)), &answer); err != nil {
		t.Fatalf("decode ask response: %v", err)
	}
	if !answer.Found {
		t.Fatalf("ask response = %+v, want winner page found after loser vector eviction", answer)
	}

	calls, _, err := wiki.NewLLMCallStore(conn).List(ctx, wiki.LLMCallFilter{Stage: "embed-page"}, paging.Params{Limit: 10})
	if err != nil {
		t.Fatalf("List embed-page calls: %v", err)
	}
	if len(calls) != 1 || calls[0].JobID != merge.JobID {
		t.Fatalf("embed-page calls = %+v, want one merge job page embedding", calls)
	}
}

func TestBuildSpecRecordsQueryEmbeddingCalls(t *testing.T) {
	// R-JEC4-3M6O
	// R-NWE2-CUPE
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subject := wiki.Subject{ID: "subject-acme", Name: "Acme Robotics", Type: "entity"}
	if err := wiki.NewSubjectStore(conn).Save(ctx, subject); err != nil {
		t.Fatalf("Save subject: %v", err)
	}
	if err := wiki.NewPageStore(conn).Upsert(ctx, wiki.Page{
		ID:        "page-acme",
		SubjectID: subject.ID,
		Title:     "Acme Robotics",
		Body:      "Acme Robotics owns the scheduler.",
	}); err != nil {
		t.Fatalf("Upsert page: %v", err)
	}
	if err := wiki.NewEmbeddingStore(conn).Upsert(ctx, wiki.Embedding{
		SubjectID:   subject.ID,
		Model:       "recorded-query-embed-model",
		Dims:        2,
		Vec:         []float32{1, 0},
		ContentHash: "hash-acme",
		UpdatedAt:   42,
	}); err != nil {
		t.Fatalf("Upsert embedding: %v", err)
	}

	prov := &capturingProvider{responses: []string{
		`{"sub_queries":["scheduler owner"],"keywords":["scheduler"],"aliases":[]}`,
		`{"found":true,"text":"Acme Robotics owns the scheduler.","citations":[{"path":"entity/acme-robotics","title":"Acme Robotics"}]}`,
	}}
	embeds := &capturingEmbeddingProvider{vectors: [][]float32{{1, 0}}, usage: agentkit.EmbeddingUsage{InputTokens: 3, Total: 3}}
	askSubjectSite := ask.DefaultSubjectCallSite()
	askSubjectSite.Model = "ask-subject-model"
	askSynthesisSite := ask.DefaultSynthesisCallSite()
	askSynthesisSite.Model = "ask-synthesis-model"
	spec := newSpec(staticConfig(wiki.Config{
		CallSites: wiki.CallSites{
			AskSubject:   askSubjectSite,
			AskSynthesis: askSynthesisSite,
		},
		EmbedSite: wiki.EmbedSite{
			Model:    "recorded-query-embed-model",
			Dims:     2,
			Provider: embeds,
		},
		SearchDefault: 8,
		LLM:           llm.New(prov, nil),
	}))
	h := buildSpecTestHandler(t, conn, spec)

	var answer struct {
		Found  bool   `json:"found"`
		Answer string `json:"answer"`
	}
	if err := json.Unmarshal([]byte(mcpToolCallText(t, h, `{
		"jsonrpc":"2.0",
		"id":"ask",
		"method":"tools/call",
		"params":{"name":"ask","arguments":{"question":"Who owns the scheduler?"}}
	}`)), &answer); err != nil {
		t.Fatalf("decode ask response: %v", err)
	}
	if !answer.Found || answer.Answer != "[Acme Robotics](https://int.ikigenba.com/srv/wiki/subject/entity/acme-robotics) owns the scheduler." {
		t.Fatalf("ask response = %#v, want answer produced through composed retriever", answer)
	}

	calls, _, err := wiki.NewLLMCallStore(conn).List(ctx, wiki.LLMCallFilter{Stage: "embed-query"}, paging.Params{Limit: 10})
	if err != nil {
		t.Fatalf("List embed-query calls: %v", err)
	}
	if len(calls) != 1 {
		t.Fatalf("embed-query calls = %+v, want one query embedding record", calls)
	}
	call := calls[0]
	if call.JobID != "" || call.Provider != "capturing-embed" || call.Model != "recorded-query-embed-model" || call.Err != "" {
		t.Fatalf("embed-query call = %+v, want recorded production query embedding footprint", call)
	}
	assertCallJSONField(t, call.Params, "dimensions", float64(2))
	assertCallJSONField(t, call.Request, "role", "query")
	assertCallJSONField(t, call.Response, "vectors", float64(1))
	assertCallJSONField(t, call.Response, "dims", float64(2))
	assertCallJSONField(t, call.Usage, "InputTokens", float64(3))

	requests := embeds.Requests()
	if len(requests) != 1 || requests[0].Role != agentkit.InputQuery || requests[0].Model != "recorded-query-embed-model" || requests[0].Dimensions != 2 {
		t.Fatalf("embedding provider requests = %#v, want one query request from buildSpec retriever", requests)
	}
}

func TestBuildCompilerUsesDefaultCompileCallSite(t *testing.T) {
	// R-4DS4-RXYX
	prov := &capturingProvider{responses: []string{`{"title":"Acme Robotics","body":"Acme Robotics runs a Tulsa lab."}`}}
	wantSite := compile.DefaultCallSite()
	wantSite.Model = "compile-model"
	cfg := wiki.Config{
		CallSites: wiki.CallSites{Compile: wantSite},
		LLM:       llm.New(prov, nil),
	}
	compiler := buildCompiler(cfg, cfg.LLM)

	title, body, err := compiler.Compile(context.Background(), wiki.Subject{
		ID:       "subject-acme",
		Name:     "Acme Robotics",
		NormName: "acme-robotics",
		Type:     "entity",
	}, []wiki.Claim{
		{ID: "claim-001", SubjectID: "subject-acme", Body: "Acme Robotics runs a Tulsa lab."},
	})
	if err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if title != "Acme Robotics" || body != "Acme Robotics runs a Tulsa lab." {
		t.Fatalf("Compile result = %q/%q, want mocked page", title, body)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != wantSite.Model {
		t.Fatalf("request model = %q, want %q from compile.DefaultCallSite", req.Model, wantSite.Model)
	}
	if req.System != wantSite.System {
		t.Fatalf("request system = %q, want %q from compile.DefaultCallSite", req.System, wantSite.System)
	}
	if len(req.Tools) != 0 {
		t.Fatalf("request tools len = %d, want tool-less compile call site", len(req.Tools))
	}
	if wantSite.Temperature == nil {
		t.Fatal("compile.DefaultCallSite temperature is nil, want deterministic temperature")
	}
	if *wantSite.Temperature != 0 {
		t.Fatalf("compile.DefaultCallSite temperature = %v, want 0", *wantSite.Temperature)
	}
	if !reflect.DeepEqual(wantSite.Reasoning, llm.DisableReasoning()) {
		t.Fatalf("compile.DefaultCallSite reasoning = %#v, want llm.DisableReasoning()", wantSite.Reasoning)
	}
	if req.Gen.Temperature == nil || *req.Gen.Temperature != *wantSite.Temperature || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want compile.DefaultCallSite temperature %v and disabled reasoning", req.Gen, *wantSite.Temperature)
	}
}

func decodeMCPToolText(t *testing.T, raw []byte, dst any) {
	t.Helper()
	if err := json.Unmarshal([]byte(mcpToolText(t, raw)), dst); err != nil {
		t.Fatalf("decode MCP tool text: %v", err)
	}
}

func mcpToolCallText(t *testing.T, h http.Handler, body string) string {
	t.Helper()
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-1")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("tools/call status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	return mcpToolText(t, rec.Body.Bytes())
}

func mcpToolText(t *testing.T, raw []byte) string {
	t.Helper()
	var got struct {
		Result struct {
			Content []struct {
				Text string `json:"text"`
			} `json:"content"`
		} `json:"result"`
	}
	if err := json.Unmarshal(raw, &got); err != nil {
		t.Fatalf("decode MCP response: %v", err)
	}
	if len(got.Result.Content) != 1 {
		t.Fatalf("content len = %d, want 1", len(got.Result.Content))
	}
	return got.Result.Content[0].Text
}

func mcpToolSurface(t *testing.T, h http.Handler, authenticated bool) []struct {
	Name        string         `json:"name"`
	InputSchema map[string]any `json:"inputSchema"`
} {
	t.Helper()
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(`{"jsonrpc":"2.0","id":"list","method":"tools/list"}`))
	if authenticated {
		req.Header.Set("X-Owner-Email", "owner@example.com")
		req.Header.Set("X-Client-Id", "client-1")
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("tools/list status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	var got struct {
		Result struct {
			Tools []struct {
				Name        string         `json:"name"`
				InputSchema map[string]any `json:"inputSchema"`
			} `json:"tools"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &got); err != nil {
		t.Fatalf("decode tools/list response: %v", err)
	}
	return got.Result.Tools
}

func withoutAnthropicKey(env []string) []string {
	out := env[:0]
	for _, kv := range env {
		if strings.HasPrefix(kv, "ANTHROPIC_API_KEY=") {
			continue
		}
		out = append(out, kv)
	}
	return out
}

func staticConfig(cfg wiki.Config) configLoader {
	return func(func(string) string) (wiki.Config, error) {
		return cfg, nil
	}
}

func migratedDB(t *testing.T, ctx context.Context) *sql.DB {
	t.Helper()
	conn, err := appdb.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	migs, err := appdb.LoadMigrations(wikidb.FS, "migrations")
	if err != nil {
		conn.Close()
		t.Fatalf("LoadMigrations: %v", err)
	}
	if err := appdb.Migrate(ctx, conn, migs); err != nil {
		conn.Close()
		t.Fatalf("Migrate: %v", err)
	}
	return conn
}

func saveBuildSpecMergeFixture(t *testing.T, ctx context.Context, conn *sql.DB) {
	t.Helper()
	subjects := wiki.NewSubjectStore(conn)
	for _, subject := range []wiki.Subject{
		{ID: "subject-loser", Name: "Loser Subject", Type: "entity"},
		{ID: "subject-winner", Name: "Winner Subject", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save subject %s: %v", subject.ID, err)
		}
	}
	claims := wiki.NewClaimStore(conn)
	for _, claim := range []wiki.Claim{
		{ID: "claim-loser", SubjectID: "subject-loser", JobID: "job-existing", Body: "Loser fact."},
		{ID: "claim-winner", SubjectID: "subject-winner", JobID: "job-existing", Body: "Winner fact."},
	} {
		if err := claims.Save(ctx, claim); err != nil {
			t.Fatalf("Save claim %s: %v", claim.ID, err)
		}
	}
	pages := wiki.NewPageStore(conn)
	for _, page := range []wiki.Page{
		{ID: "subject-loser", SubjectID: "subject-loser", Title: "Loser Subject", Body: "old loser body"},
		{ID: "subject-winner", SubjectID: "subject-winner", Title: "Winner Subject", Body: "old winner body"},
	} {
		if err := pages.Upsert(ctx, page); err != nil {
			t.Fatalf("Upsert page %s: %v", page.ID, err)
		}
	}
}

func buildSpecTestHandler(t *testing.T, conn *sql.DB, spec appkit.Spec) http.Handler {
	t.Helper()
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		Register:   spec.Handlers,
		DB:         conn,
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}
	return srv.Handler
}

func startBuildSpecWorker(t *testing.T, parent context.Context, run func(context.Context) error) (func(), <-chan error) {
	t.Helper()
	if run == nil {
		t.Fatal("buildSpec worker is nil")
	}
	ctx, cancel := context.WithCancel(parent)
	done := make(chan error, 1)
	go func() {
		done <- run(ctx)
	}()
	stop := func() {
		cancel()
		select {
		case err := <-done:
			if err != nil {
				t.Fatalf("buildSpec worker returned error: %v", err)
			}
		case <-time.After(3 * time.Second):
			t.Fatal("buildSpec worker did not stop")
		}
	}
	return stop, done
}

func waitBuildSpecJob(t *testing.T, ctx context.Context, conn *sql.DB, jobID, want string, workerErr <-chan error) wiki.JobStatus {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	jobs := wiki.NewJobStore(conn)
	var last wiki.JobStatus
	for time.Now().Before(deadline) {
		select {
		case err := <-workerErr:
			if err != nil {
				t.Fatalf("buildSpec worker returned before job completed: %v", err)
			}
			t.Fatal("buildSpec worker stopped before job completed")
		default:
		}
		status, err := jobs.Status(ctx, jobID)
		if err == nil {
			last = status
			if status.Status == want {
				return status
			}
			if status.Status == wiki.JobFailed {
				t.Fatalf("job %s failed: %s", jobID, status.Error)
			}
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatalf("job %s status = %+v, want %s", jobID, last, want)
	return wiki.JobStatus{}
}

func assertCallJSONField(t *testing.T, raw, key string, want any) {
	t.Helper()
	var fields map[string]any
	if err := json.Unmarshal([]byte(raw), &fields); err != nil {
		t.Fatalf("decode %s as JSON object: %v", raw, err)
	}
	if got := fields[key]; got != want {
		t.Fatalf("JSON field %s in %s = %#v, want %#v", key, raw, got, want)
	}
}

func manifestExtras(in []appkit.ManifestKV) []manifest.KV {
	out := make([]manifest.KV, 0, len(in))
	for _, kv := range in {
		out = append(out, manifest.KV{Key: kv.Key, Value: kv.Value})
	}
	return out
}

func testEnv(overrides map[string]string) []string {
	env := os.Environ()
	out := make([]string, 0, len(env)+len(overrides))
	for _, kv := range env {
		key, _, _ := strings.Cut(kv, "=")
		if _, ok := overrides[key]; ok {
			continue
		}
		out = append(out, kv)
	}
	for key, value := range overrides {
		out = append(out, key+"="+value)
	}
	return out
}

func freeTCPPort(t *testing.T) int {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen for free port: %v", err)
	}
	defer ln.Close()
	return ln.Addr().(*net.TCPAddr).Port
}

func waitForHealth(t *testing.T, port int, done <-chan error, stdout, stderr *bytes.Buffer) map[string]any {
	t.Helper()
	url := fmt.Sprintf("http://127.0.0.1:%d/health", port)
	client := http.Client{Timeout: 250 * time.Millisecond}
	deadline := time.Now().Add(5 * time.Second)
	var last string
	for time.Now().Before(deadline) {
		select {
		case err := <-done:
			t.Fatalf("wiki exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		default:
		}

		resp, err := client.Get(url)
		if err == nil {
			body, readErr := io.ReadAll(resp.Body)
			closeErr := resp.Body.Close()
			if resp.StatusCode == http.StatusOK && readErr == nil && closeErr == nil {
				var doc map[string]any
				if err := json.Unmarshal(body, &doc); err != nil {
					t.Fatalf("decode health JSON: %v\nbody:\n%s", err, body)
				}
				return doc
			}
			last = fmt.Sprintf("status=%d read=%v close=%v body=%s", resp.StatusCode, readErr, closeErr, body)
		} else {
			last = err.Error()
		}
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatalf("wiki never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
	}
}

type capturingProvider struct {
	responses []string
	requests  []agentkit.Request
}

type capturingEmbeddingProvider struct {
	mu       sync.Mutex
	vectors  [][]float32
	usage    agentkit.EmbeddingUsage
	requests []agentkit.EmbedRequest
}

func (p *capturingEmbeddingProvider) Embed(_ context.Context, req *agentkit.EmbedRequest) *agentkit.EmbedRoundTrip {
	p.mu.Lock()
	defer p.mu.Unlock()
	if req != nil {
		p.requests = append(p.requests, *req)
	}
	return agentkit.NewEmbedRoundTrip(cloneVectors(p.vectors), p.usage, nil, nil)
}

func (p *capturingEmbeddingProvider) Name() string {
	return "capturing-embed"
}

func (p *capturingEmbeddingProvider) Pricing(string) (agentkit.EmbeddingPricing, bool) {
	return agentkit.EmbeddingPricing{InputToken: 1}, true
}

func (p *capturingEmbeddingProvider) Requests() []agentkit.EmbedRequest {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]agentkit.EmbedRequest(nil), p.requests...)
}

func cloneVectors(in [][]float32) [][]float32 {
	out := make([][]float32, len(in))
	for i := range in {
		out[i] = append([]float32(nil), in[i]...)
	}
	return out
}

type surfaceWiki struct{}

func (surfaceWiki) Ingest(context.Context, string, string, string, []string) (string, error) {
	return "", nil
}

func (surfaceWiki) JobStatus(context.Context, string) (publicJobStatus, error) {
	return publicJobStatus{}, nil
}

func (surfaceWiki) Abort(context.Context, string) (wiki.AbortResult, error) {
	return wiki.AbortResult{}, nil
}

func (surfaceWiki) Rerun(context.Context, string) (wiki.RerunResult, error) {
	return wiki.RerunResult{}, nil
}

func (surfaceWiki) ListJobs(context.Context, mcp.JobFilter, paging.Params) ([]wiki.Job, string, error) {
	return []wiki.Job{{ID: "job-1", Status: wiki.JobDone}}, "", nil
}

func (surfaceWiki) CountJobs(context.Context, mcp.JobFilter) (int, error) {
	return 1, nil
}

func (surfaceWiki) GetByPath(context.Context, string) (wiki.Subject, error) {
	return wiki.Subject{ID: "subject-1", Name: "Acme", NormName: "acme", Type: "entity"}, nil
}

func (surfaceWiki) MergeSubjects(context.Context, string, string) (string, error) {
	return "job-merge", nil
}

func (surfaceWiki) ListMerges(context.Context, paging.Params) ([]wiki.Alias, string, error) {
	return []wiki.Alias{{NormName: "old acme", SubjectID: "subject-1", Name: "Old Acme", CreatedBy: "owner@example.com", CreatedAt: "2026-06-24T12:00:00Z"}}, "", nil
}

func (surfaceWiki) Subjects(context.Context, string, string) ([]publicSubject, error) {
	return []publicSubject{{Path: "entity/acme", Type: "entity", Name: "Acme", HasPage: true}}, nil
}

func (surfaceWiki) List(context.Context, string, string, paging.Params) ([]publicSubject, string, error) {
	return []publicSubject{{Path: "entity/acme", Type: "entity", Name: "Acme", HasPage: true}}, "", nil
}

func (surfaceWiki) ClaimsBySubject(context.Context, string) ([]publicClaim, error) {
	return []publicClaim{{ID: "claim-1", Text: "Claim text.", Job: "job-1"}}, nil
}

func (surfaceWiki) ListBySubject(context.Context, string, paging.Params) ([]publicClaim, string, error) {
	return []publicClaim{{ID: "claim-1", Text: "Claim text.", Job: "job-1"}}, "", nil
}

func (surfaceWiki) PageByPath(context.Context, string) (publicPage, error) {
	return publicPage{}, nil
}

type surfaceCalls struct{}

func (surfaceCalls) List(context.Context, mcp.LLMCallFilter, paging.Params) ([]wiki.CallRecord, string, error) {
	return []wiki.CallRecord{{ID: "call-1", Stage: "extract", JobID: "job-1"}}, "", nil
}

func surfaceAsk(context.Context, string, string) (askSurfaceAnswer, error) {
	return askSurfaceAnswer{}, nil
}

type askSurfaceAnswer struct {
	Found     bool
	Text      string
	Citations []askSurfaceCitation
}

type askSurfaceCitation struct {
	Path  string
	Title string
}

func (p *capturingProvider) RoundTrip(_ context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, cloneRequest(req))
	text := `{"title":"Untitled","body":"Empty."}`
	if len(p.responses) > 0 {
		text = p.responses[0]
		p.responses = p.responses[1:]
	}
	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: []agentkit.Block{agentkit.TextBlock{Text: text}}},
		agentkit.FinishStop,
		agentkit.Usage{InputUncached: 1, Output: 1, Total: 2},
		nil,
		nil,
	)
}

func (p *capturingProvider) Name() string {
	return "capturing"
}

func (p *capturingProvider) Pricing(string) (agentkit.Pricing, bool) {
	return agentkit.Pricing{Tiers: []agentkit.RateTier{{MinInputTokens: 0}}}, true
}

func cloneRequest(req *agentkit.Request) agentkit.Request {
	if req == nil {
		return agentkit.Request{}
	}
	return agentkit.Request{
		Model:    req.Model,
		System:   req.System,
		Messages: append([]agentkit.Message(nil), req.Messages...),
		Tools:    append([]agentkit.Tool(nil), req.Tools...),
		Gen:      req.Gen,
	}
}
