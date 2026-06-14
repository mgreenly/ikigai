package read

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"agentkit/provider"

	"wiki/internal/config"
	"wiki/internal/db"
	"wiki/internal/inbox"
	"wiki/internal/llm"
	"wiki/internal/page"

	_ "modernc.org/sqlite"
)

// newTestDB stands up a migrated in-temp-dir SQLite DB.
func newTestDB(t *testing.T) *sql.DB {
	t.Helper()
	dir := t.TempDir()
	conn, err := db.Open(filepath.Join(dir, "wiki.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return conn
}

func mustExec(t *testing.T, conn *sql.DB, q string, args ...any) {
	t.Helper()
	if _, err := conn.Exec(q, args...); err != nil {
		t.Fatalf("exec: %v", err)
	}
}

// seedAcme inserts the Acme Corp subject + alias + page used across the ask/search
// tests.
func seedAcme(t *testing.T, conn *sql.DB) {
	t.Helper()
	mustExec(t, conn, `INSERT INTO subjects (id, type, kind, canonical_name, created_by_run) VALUES ('01HACMECORPSUBJECTID0001','entity','company','Acme Corp','r')`)
	mustExec(t, conn, `INSERT INTO aliases (type, norm, subject_id) VALUES ('entity',?,?)`, page.Normalize("Acme Corp"), "01HACMECORPSUBJECTID0001")
	res, err := conn.Exec(`INSERT INTO pages (subject, title, body, version) VALUES (?,?,?,1)`,
		"01HACMECORPSUBJECTID0001", "Acme Corp", "Dana Lee is the CEO of Acme Corp. [01ARRIVAL]")
	if err != nil {
		t.Fatalf("insert page: %v", err)
	}
	rowid, _ := res.LastInsertId()
	mustExec(t, conn, `INSERT INTO pages_fts (rowid, title, body) VALUES (?,?,?)`, rowid, "Acme Corp", "Dana Lee is the CEO of Acme Corp. [01ARRIVAL]")
}

func newReadService(conn *sql.DB) *Service {
	store := page.NewStore(conn)
	return NewService(store, NewStoreRetriever(store), SearchLimits{Default: 3, Cap: 10})
}

// mockCaller is a stand-in inner agent: it can replay tool calls (to exercise the
// dispatch path) and then returns a fixed final answer.
type mockCaller struct {
	finalAnswer string
	// toolPlan, if set, is invoked with the dispatch so tests can drive the real
	// read tools and assert their results before the answer is returned.
	toolPlan func(ctx context.Context, dispatch llm.ToolDispatch) error
	err      error
}

func (m *mockCaller) Agent(ctx context.Context, site config.CallSite, msgs []provider.Message, tools []provider.Tool, budget llm.AgentBudget, dispatch llm.ToolDispatch) (*llm.StructuredResult, error) {
	if m.err != nil {
		return nil, m.err
	}
	if m.toolPlan != nil {
		if err := m.toolPlan(ctx, dispatch); err != nil {
			return nil, err
		}
	}
	return &llm.StructuredResult{Raw: m.finalAnswer}, nil
}

func newInboxStore(t *testing.T, conn *sql.DB) *inbox.Store {
	t.Helper()
	s, err := inbox.New(inbox.Options{DB: conn, BlobRoot: t.TempDir(), InlineMax: 4096, MaxBytes: 1 << 20})
	if err != nil {
		t.Fatalf("inbox: %v", err)
	}
	return s
}

// --- ParseAnswer ------------------------------------------------------------

func TestParseAnswerContract(t *testing.T) {
	raw, err := os.ReadFile(filepath.Join("testdata", "ask_response.json"))
	if err != nil {
		t.Fatalf("read fixture: %v", err)
	}
	a, err := ParseAnswer(string(raw))
	if err != nil {
		t.Fatalf("committed fixture must parse: %v", err)
	}
	if !a.Found || len(a.Citations) != 1 || a.Citations[0].Subject == "" || a.Citations[0].Title == "" {
		t.Errorf("fixture answer = %+v", a)
	}

	// found=true with no citations is rejected (page-level citation contract).
	if _, err := ParseAnswer(`{"answer":"x","citations":[],"found":true}`); err == nil {
		t.Error("found=true with no citations should be rejected")
	}
	// found=false with citations is rejected (the not-found shape cites nothing).
	if _, err := ParseAnswer(`{"answer":"x","citations":[{"subject":"s","title":"t"}],"found":false}`); err == nil {
		t.Error("found=false with citations should be rejected")
	}
	// empty answer is rejected.
	if _, err := ParseAnswer(`{"answer":"  ","citations":[],"found":false}`); err == nil {
		t.Error("empty answer should be rejected")
	}
	// a citation missing its subject id is rejected.
	if _, err := ParseAnswer(`{"answer":"x","citations":[{"subject":"","title":"t"}],"found":true}`); err == nil {
		t.Error("citation without subject id should be rejected")
	}
	// a fenced reply is accepted.
	if _, err := ParseAnswer("```json\n{\"answer\":\"hi\",\"citations\":[],\"found\":false}\n```"); err != nil {
		t.Errorf("fenced reply should parse: %v", err)
	}
	// prose-wrapped JSON (a real model habit) is accepted — the embedded object is
	// extracted by brace-matching, respecting string literals with braces in them.
	wrapped := `Based on the wiki: {"answer":"Dana Lee {the CEO}","citations":[{"subject":"s","title":"t"}],"found":true} — hope that helps!`
	wa, err := ParseAnswer(wrapped)
	if err != nil {
		t.Fatalf("prose-wrapped JSON should parse: %v", err)
	}
	if wa.Answer != "Dana Lee {the CEO}" || !wa.Found {
		t.Errorf("prose-wrapped parse = %+v", wa)
	}
}

// --- Ask happy path (mocked inner agent) ------------------------------------

func TestAskHappyPath(t *testing.T) {
	conn := newTestDB(t)
	seedAcme(t, conn)
	svc := newReadService(conn)
	asks := NewAskStore(conn)
	raw, _ := os.ReadFile(filepath.Join("testdata", "ask_response.json"))

	// The mock drives lookup + read_page through the REAL tools, then returns the
	// page-cited answer.
	mc := &mockCaller{
		finalAnswer: string(raw),
		toolPlan: func(ctx context.Context, dispatch llm.ToolDispatch) error {
			out, err := dispatch(ctx, "lookup", json.RawMessage(`{"name":"Acme Corp"}`))
			if err != nil {
				return err
			}
			if !strings.Contains(out, "01HACMECORPSUBJECTID0001") {
				t.Errorf("lookup tool did not resolve Acme Corp: %s", out)
			}
			out, err = dispatch(ctx, "read_page", json.RawMessage(`{"subject":"01HACMECORPSUBJECTID0001"}`))
			if err != nil {
				return err
			}
			if !strings.Contains(out, "Dana Lee") {
				t.Errorf("read_page did not return the body: %s", out)
			}
			return nil
		},
	}
	asker := NewAsker(svc, mc, newInboxStore(t, conn), asks, config.CallSite{Name: "ask", Model: "claude-sonnet-4-6"}, llm.AgentBudget{MaxTurns: 5})

	ans, err := asker.Ask(context.Background(), "owner@x.com", "Who is the CEO of Acme Corp?")
	if err != nil {
		t.Fatalf("ask: %v", err)
	}
	if !ans.Found || !strings.Contains(ans.Answer, "Dana Lee") {
		t.Errorf("answer = %+v", ans)
	}
	if len(ans.Citations) != 1 || ans.Citations[0].Subject != "01HACMECORPSUBJECTID0001" {
		t.Errorf("citation = %+v", ans.Citations)
	}

	// The asks row was finalized succeeded, with the question + answer captured.
	var status, question, usage string
	if err := conn.QueryRow(`SELECT status, question, COALESCE(usage,'') FROM asks LIMIT 1`).Scan(&status, &question, &usage); err != nil {
		t.Fatalf("asks row: %v", err)
	}
	if status != StatusSucceeded {
		t.Errorf("asks status = %q, want succeeded", status)
	}
	if question != "Who is the CEO of Acme Corp?" {
		t.Errorf("asks question = %q", question)
	}
	if !strings.Contains(usage, "Dana Lee") {
		t.Errorf("asks usage golden missing the answer: %s", usage)
	}
}

// --- Abstention over fabrication --------------------------------------------

func TestAskAbstainsOnEmptyWiki(t *testing.T) {
	conn := newTestDB(t)
	svc := newReadService(conn)
	asks := NewAskStore(conn)
	mc := &mockCaller{finalAnswer: `{"answer":"The wiki has nothing on this.","citations":[],"found":false}`}
	asker := NewAsker(svc, mc, newInboxStore(t, conn), asks, config.CallSite{Name: "ask", Model: "claude-sonnet-4-6"}, llm.AgentBudget{})

	ans, err := asker.Ask(context.Background(), "o@x.com", "What is the airspeed of an unladen swallow?")
	if err != nil {
		t.Fatalf("ask: %v", err)
	}
	if ans.Found || len(ans.Citations) != 0 {
		t.Errorf("abstention answer should be not-found, no citations: %+v", ans)
	}
}

// --- read_source follows a citation to the original arrival ------------------

func TestAskReadSourceTool(t *testing.T) {
	conn := newTestDB(t)
	box := newInboxStore(t, conn)
	rec, err := box.Accept(context.Background(), "o@x.com", "document", "mcp:test", "text/plain", "Memo", "[]", []byte("the original memo text"))
	if err != nil {
		t.Fatalf("accept: %v", err)
	}
	svc := newReadService(conn)
	asks := NewAskStore(conn)
	mc := &mockCaller{
		finalAnswer: `{"answer":"per the memo","citations":[],"found":false}`,
		toolPlan: func(ctx context.Context, dispatch llm.ToolDispatch) error {
			out, derr := dispatch(ctx, "read_source", json.RawMessage(`{"inbox_id":"`+rec.ID+`"}`))
			if derr != nil {
				return derr
			}
			if !strings.Contains(out, "the original memo text") {
				t.Errorf("read_source did not return the payload: %s", out)
			}
			// An unknown id is found=false, not an error.
			out, derr = dispatch(ctx, "read_source", json.RawMessage(`{"inbox_id":"nope"}`))
			if derr != nil {
				return derr
			}
			if !strings.Contains(out, "false") {
				t.Errorf("unknown read_source should report found=false: %s", out)
			}
			return nil
		},
	}
	asker := NewAsker(svc, mc, box, asks, config.CallSite{Name: "ask", Model: "claude-sonnet-4-6"}, llm.AgentBudget{})
	if _, err := asker.Ask(context.Background(), "o@x.com", "what does the memo say?"); err != nil {
		t.Fatalf("ask: %v", err)
	}
}

// --- Ask failure finalizes the asks row failed ------------------------------

func TestAskFailureFinalizesRow(t *testing.T) {
	conn := newTestDB(t)
	svc := newReadService(conn)
	asks := NewAskStore(conn)
	mc := &mockCaller{err: errors.New("model exploded")}
	asker := NewAsker(svc, mc, newInboxStore(t, conn), asks, config.CallSite{Name: "ask", Model: "claude-sonnet-4-6"}, llm.AgentBudget{})

	if _, err := asker.Ask(context.Background(), "o@x.com", "q"); err == nil {
		t.Fatal("expected ask error")
	}
	var status, errStr string
	if err := conn.QueryRow(`SELECT status, error FROM asks LIMIT 1`).Scan(&status, &errStr); err != nil {
		t.Fatalf("asks row: %v", err)
	}
	if status != StatusFailed || errStr == "" {
		t.Errorf("failed ask row = %q / %q", status, errStr)
	}
}

// --- asks orphan sweep ------------------------------------------------------

func TestSweepOrphans(t *testing.T) {
	conn := newTestDB(t)
	asks := NewAskStore(conn)
	// One running orphan, one already-finished row (must be untouched).
	mustExec(t, conn, `INSERT INTO asks (id, owner, question, status, started_at) VALUES ('a1','o','q','running',1)`)
	mustExec(t, conn, `INSERT INTO asks (id, owner, question, status, started_at, finished_at) VALUES ('a2','o','q','succeeded',1,2)`)

	n, err := asks.SweepOrphans(context.Background())
	if err != nil {
		t.Fatalf("sweep: %v", err)
	}
	if n != 1 {
		t.Errorf("swept %d, want 1", n)
	}
	var s1, s2 string
	conn.QueryRow(`SELECT status FROM asks WHERE id='a1'`).Scan(&s1)
	conn.QueryRow(`SELECT status FROM asks WHERE id='a2'`).Scan(&s2)
	if s1 != StatusCrashed || s2 != StatusSucceeded {
		t.Errorf("after sweep: a1=%q a2=%q", s1, s2)
	}
}

// --- search registry-first + whole-page contract ----------------------------

func TestSearchRegistryFirst(t *testing.T) {
	conn := newTestDB(t)
	seedAcme(t, conn)
	// A second page that also matches the body lane but is NOT the exact name.
	mustExec(t, conn, `INSERT INTO subjects (id, type, kind, canonical_name, created_by_run) VALUES ('s2','entity','company','Acme Rivals','r')`)
	res, _ := conn.Exec(`INSERT INTO pages (subject, title, body, version) VALUES ('s2','Acme Rivals','A competitor of Acme Corp.',1)`)
	rowid, _ := res.LastInsertId()
	mustExec(t, conn, `INSERT INTO pages_fts (rowid, title, body) VALUES (?,?,?)`, rowid, "Acme Rivals", "A competitor of Acme Corp.")

	svc := newReadService(conn)
	hits, err := svc.Search(context.Background(), "Acme Corp", 5)
	if err != nil {
		t.Fatalf("search: %v", err)
	}
	if len(hits) == 0 {
		t.Fatal("search returned nothing")
	}
	// Registry-first: the exact-name subject is pinned at rank 1.
	if hits[0].Subject != "01HACMECORPSUBJECTID0001" {
		t.Errorf("registry-first failed: rank 1 = %q", hits[0].Subject)
	}
	// A hit is the whole page.
	if hits[0].Title == "" || hits[0].Body == "" || hits[0].Type == "" {
		t.Errorf("hit not a whole page: %+v", hits[0])
	}
}

func TestSearchLimitClamp(t *testing.T) {
	l := SearchLimits{Default: 3, Cap: 10}
	if l.Resolve(0) != 3 {
		t.Error("non-positive should default to 3")
	}
	if l.Resolve(50) != 10 {
		t.Error("over-cap should clamp to 10")
	}
	if l.Resolve(5) != 5 {
		t.Error("in-range should pass through")
	}
}
