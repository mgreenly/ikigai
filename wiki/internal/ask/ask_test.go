package ask

import (
	"context"
	"encoding/json"
	"errors"
	"strings"
	"testing"
	"time"

	"agentkit/job"
	"agentkit/provider"
	"agentkit/tools/write"

	"wiki/internal/db"
	"wiki/internal/ingest"
	"wiki/internal/search"
	"wiki/internal/store"
)

// stubProvider is a no-network provider.Client. Each Stream call replays the next
// canned event sequence, so an N-element sequence drives N agent turns. Mirrors
// the ingest/lint test stubs.
type stubProvider struct {
	sequences [][]provider.Event
	calls     int
}

func (s *stubProvider) Stream(_ context.Context, _ provider.Request) (<-chan provider.Event, error) {
	if s.calls >= len(s.sequences) {
		return nil, &provider.Error{Kind: provider.ErrUnknown, Msg: "stubProvider exhausted"}
	}
	evs := s.sequences[s.calls]
	s.calls++
	ch := make(chan provider.Event, len(evs))
	for _, ev := range evs {
		ch <- ev
	}
	close(ch)
	return ch, nil
}

func writeToolUse(t *testing.T, id, relPath, content string) provider.EventToolUse {
	t.Helper()
	raw, err := json.Marshal(map[string]any{"file_path": relPath, "content": content})
	if err != nil {
		t.Fatalf("marshal write input: %v", err)
	}
	return provider.EventToolUse{ID: id, Name: write.Name, Input: raw}
}

// newAsker builds an Asker wired to a real on-disk store, a real BM25 index, a
// real migrated SQLite DB, and the supplied stub provider. It returns the Asker,
// the store, and the index so tests can seed a fixture tree and assert on the
// post-ask state.
func newAsker(t *testing.T, stub provider.Client) (*Asker, *store.Store, search.Index) {
	t.Helper()
	st, err := store.New(t.TempDir())
	if err != nil {
		t.Fatalf("store.New: %v", err)
	}
	idx := search.NewBM25Index(st.SearchIndexPath)
	t.Cleanup(func() { idx.Close() })

	conn, err := db.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}

	newClient := func() (provider.Client, error) { return stub, nil }
	a := New(st, idx, conn, newClient, Config{Model: "claude-sonnet-4-6", MaxTokens: 4096, JobTTL: 5 * time.Second})
	return a, st, idx
}

// awaitTerminal polls JobStatus until the job is terminal, failing on timeout.
func awaitTerminal(t *testing.T, a *Asker, owner, jobID string) ingest.Status {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	for {
		st, err := a.JobStatus(context.Background(), owner, "", jobID)
		if err == nil && st.Terminal {
			return st
		}
		if time.Now().After(deadline) {
			t.Fatalf("timed out waiting for ask job %s to go terminal (last err=%v status=%q)", jobID, err, st.Status)
		}
		time.Sleep(2 * time.Millisecond)
	}
}

// seedFixtureTree lays down a small collection with a couple of curated pages and
// an index so the ask pass has something to synthesize from. The pages are written
// through the store (not the agent) — this is the pre-existing tree ask navigates.
func seedFixtureTree(t *testing.T, st *store.Store, owner string) {
	t.Helper()
	const col = "default"
	if _, err := st.EnsureLayout(owner, col); err != nil {
		t.Fatalf("EnsureLayout: %v", err)
	}
	pages := map[string]string{
		"concepts/otters.md":    "---\ntype: concept\ntitle: Otters\nsource: sources/otter-note.md\ncollection: default\n---\n# Otters\n\nPlayful semi-aquatic mammals; expert swimmers.\n",
		"sources/otter-note.md": "---\ntype: source\nkind: chat\ntitle: Otter note\ncollection: default\n---\n# Otter note\n\nFiled from a chat snippet.\n",
		"index.md":              "---\ntype: index\n---\n# Wiki index\n\n- [Otters](concepts/otters.md)\n",
	}
	for rel, body := range pages {
		if err := st.WritePage(owner, col, rel, []byte(body)); err != nil {
			t.Fatalf("seed WritePage %s: %v", rel, err)
		}
	}
}

// TestAskCorePath is the acceptance-gate stub-provider test (no network). It seeds
// a fixture tree, then drives the REAL Asker.Ask with a stub provider that "reads"
// the wiki and files a CITED synthesis page back under synthesis/, then ends with
// the answer as plain text.
//
// It asserts: the ask job spawns and reaches terminal `succeeded`; the agent's
// synthesis write landed INSIDE the owner+collection tree (confined) and is a
// `synthesis`-type page that CITES the page it used; ReindexCollection ran on
// success; and — the compounding property — a subsequent wiki_search FINDS the
// freshly-filed synthesis page.
func TestAskCorePath(t *testing.T) {
	const owner = "alice@example.com"

	// The cited synthesis page the ask agent "decides" to file. It is a synthesis
	// page that cites concepts/otters.md (the page it drew on).
	synthesisPage := "---\ntype: synthesis\ntitle: What are otters?\nsource: concepts/otters.md\ncollection: default\n---\n# What are otters?\n\nOtters are playful semi-aquatic mammals and expert swimmers (see concepts/otters.md).\n"
	answerText := "Otters are playful semi-aquatic mammals and expert swimmers (source: concepts/otters.md)."

	stub := &stubProvider{sequences: [][]provider.Event{
		// 1. file the cited synthesis page back under synthesis/.
		{writeToolUse(t, "w1", "synthesis/what-are-otters.md", synthesisPage), provider.EventDone{StopReason: "tool_use"}},
		// 2. final text turn: the answer itself.
		{
			provider.EventTextDelta{Text: answerText},
			provider.EventUsage{InputTokens: 150, OutputTokens: 30},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	a, st, idx := newAsker(t, stub)
	seedFixtureTree(t, st, owner)

	res, err := a.Ask(context.Background(), owner, "", "What are otters?")
	if err != nil {
		t.Fatalf("Ask: %v", err)
	}
	if res.JobID == "" {
		t.Fatal("Ask returned empty job id")
	}

	// The async job reached succeeded, and the status verb reports it terminal.
	st1 := awaitTerminal(t, a, owner, res.JobID)
	if st1.Status != string(job.StatusSucceeded) {
		t.Fatalf("ask job status = %q (err=%q), want succeeded", st1.Status, st1.Error)
	}
	if !st1.Terminal || st1.EndedAt == "" {
		t.Fatalf("status not terminal: %+v", st1)
	}
	if st1.UsageJSON == "" || !strings.Contains(st1.UsageJSON, "input_tokens") {
		t.Fatalf("status usage = %q, want a captured usage blob", st1.UsageJSON)
	}

	// The synthesis page landed INSIDE the owner+collection tree (confined) and is a
	// cited synthesis page.
	got, err := st.ReadPage(owner, "default", "synthesis/what-are-otters.md")
	if err != nil {
		t.Fatalf("ReadPage synthesis: %v", err)
	}
	gotStr := string(got)
	if !strings.Contains(gotStr, "type: synthesis") {
		t.Fatalf("filed page is not a synthesis page:\n%s", gotStr)
	}
	if !strings.Contains(gotStr, "concepts/otters.md") {
		t.Fatalf("synthesis answer is not cited (no source page reference):\n%s", gotStr)
	}

	// raw/ must be untouched by ask (immutable raw invariant).
	if rawPages, err := st.ListPages(owner, "default", "raw"); err == nil && len(rawPages) > 0 {
		t.Fatalf("ask touched raw/: %v", rawPages)
	}

	// COMPOUNDING: ReindexCollection ran on success, so a subsequent wiki_search
	// finds the freshly-filed synthesis page.
	results, err := idx.Search(context.Background(), owner, "default", "otters", 10)
	if err != nil {
		t.Fatalf("Search: %v", err)
	}
	found := false
	for _, h := range results.Hits {
		if h.Path == "synthesis/what-are-otters.md" {
			found = true
		}
	}
	if !found {
		t.Fatalf("search after ask missing the filed synthesis page; hits=%v", hitPaths(results))
	}
	if results.Index == nil {
		t.Fatal("search Index nil; index.md was not re-indexed after ask")
	}
}

func hitPaths(r search.Results) []string {
	out := make([]string, 0, len(r.Hits))
	for _, h := range r.Hits {
		out = append(out, h.Path)
	}
	return out
}

// TestAskSingleFlightWithIngest proves ask and ingest serialize over the SAME
// per-(owner, collection) flight key: an ask started while an ingest job is still
// running for the same collection is rejected with job.ErrFlightInUse, and once
// the ingest clears the ask succeeds. The ingest job is held running by a blocking
// stub provider so the collision is deterministic (no sleeps). Mirrors lint's
// single-flight test.
func TestAskSingleFlightWithIngest(t *testing.T) {
	const owner = "carol@example.com"

	st, err := store.New(t.TempDir())
	if err != nil {
		t.Fatalf("store.New: %v", err)
	}
	idx := search.NewBM25Index(st.SearchIndexPath)
	t.Cleanup(func() { idx.Close() })
	conn, err := db.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}

	// Share the ONE db + store + index between the ingest core and the asker, so the
	// single-flight gate (the global partial-unique running index on wiki_jobs) is
	// actually exercised across the two packages.
	release := make(chan struct{})
	entered := make(chan struct{}, 1)
	blocking := &blockingProvider{release: release, entered: entered}

	ingestCore := ingest.New(st, idx, conn,
		func() (provider.Client, error) { return blocking, nil },
		ingest.Config{Model: "claude-sonnet-4-6", MaxTokens: 4096, JobTTL: 5 * time.Second})

	asker := New(st, idx, conn,
		func() (provider.Client, error) { return &stubProvider{}, nil },
		Config{Model: "claude-sonnet-4-6", MaxTokens: 4096, JobTTL: 5 * time.Second})

	// Start an ingest; hold it running.
	ing, err := ingestCore.Ingest(context.Background(), owner, "", []byte("doc one"), store.RawMeta{})
	if err != nil {
		t.Fatalf("ingest: %v", err)
	}
	select {
	case <-entered:
	case <-time.After(2 * time.Second):
		t.Fatal("ingest job never entered the provider Stream")
	}

	// An ask into the SAME owner+collection must be rejected single-flight: it
	// shares ingest's flight key (ask is a write-pass — it files a synthesis page).
	ar, err := asker.Ask(context.Background(), owner, "", "anything")
	if !errors.Is(err, job.ErrFlightInUse) {
		t.Fatalf("ask while ingest running: err = %v, want ErrFlightInUse", err)
	}
	if ar.JobID != "" {
		t.Fatalf("rejected ask still returned a job id %q", ar.JobID)
	}

	// Let the ingest finish, then an ask must now succeed (the flight key is free).
	close(release)
	deadline := time.Now().Add(3 * time.Second)
	for {
		s, err := ingestCore.JobStatus(context.Background(), owner, "", ing.JobID)
		if err == nil && s.Terminal {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("ingest job %s never went terminal (err=%v)", ing.JobID, err)
		}
		time.Sleep(2 * time.Millisecond)
	}

	ar2, err := asker.Ask(context.Background(), owner, "", "anything")
	if err != nil {
		t.Fatalf("ask after ingest cleared: %v", err)
	}
	if ar2.JobID == "" {
		t.Fatal("post-ingest ask returned empty job id")
	}
	awaitTerminal(t, asker, owner, ar2.JobID)
}

// blockingProvider holds the first Stream call open until release is closed, then
// returns a single final text turn. It signals entered once so the test can
// synchronize on the running job before racing a second write-pass. Mirrors the
// ingest/lint test's blockingProvider.
type blockingProvider struct {
	release <-chan struct{}
	entered chan<- struct{}
	armed   bool
}

func (b *blockingProvider) Stream(ctx context.Context, _ provider.Request) (<-chan provider.Event, error) {
	if !b.armed {
		b.armed = true
		select {
		case b.entered <- struct{}{}:
		default:
		}
		select {
		case <-b.release:
		case <-ctx.Done():
			return nil, &provider.Error{Kind: provider.ErrUnknown, Msg: "cancelled"}
		}
	}
	ch := make(chan provider.Event, 2)
	ch <- provider.EventTextDelta{Text: "done"}
	ch <- provider.EventDone{StopReason: "end_turn"}
	close(ch)
	return ch, nil
}
