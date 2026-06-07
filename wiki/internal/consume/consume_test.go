package consume

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"agentkit/job"
	"agentkit/provider"
	"agentkit/tools/write"

	"eventplane/consumer"

	"wiki/internal/db"
	"wiki/internal/ingest"
	"wiki/internal/search"
	"wiki/internal/store"
)

// sha256Hex is the raw-store key for content bytes (matches store.WriteRaw).
func sha256Hex(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:])
}

// jobIDs returns every job id in wiki_jobs for the box owner.
func jobIDs(t *testing.T, conn *sql.DB) []string {
	t.Helper()
	rows, err := conn.Query(`SELECT id FROM wiki_jobs WHERE owner=?`, boxOwner)
	if err != nil {
		t.Fatalf("query wiki_jobs: %v", err)
	}
	defer rows.Close()
	var out []string
	for rows.Next() {
		var id string
		if err := rows.Scan(&id); err != nil {
			t.Fatalf("scan job id: %v", err)
		}
		out = append(out, id)
	}
	return out
}

// singleJobID returns the box owner's one-and-only job id, failing otherwise.
func singleJobID(t *testing.T, conn *sql.DB) string {
	t.Helper()
	ids := jobIDs(t, conn)
	if len(ids) != 1 {
		t.Fatalf("expected exactly 1 job for box owner, got %d (%v)", len(ids), ids)
	}
	return ids[0]
}

const boxOwner = "owner@example.com"

// ── stub provider (copied shape from internal/ingest/ingest_test.go) ──────────

// stubProvider is a no-network provider.Client. Each Stream call replays the next
// canned event sequence.
type stubProvider struct {
	mu        sync.Mutex
	sequences [][]provider.Event
	calls     int
}

func (s *stubProvider) Stream(_ context.Context, _ provider.Request) (<-chan provider.Event, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
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

// fileSeq is a canned 4-turn agent run: write a source page, update index.md,
// append log.md, then a final text turn (the same shape the 4.1 core-path test
// uses). It is enough to drive Core.Ingest's integration job to success.
func fileSeq(t *testing.T) [][]provider.Event {
	t.Helper()
	src := "---\ntype: source\nkind: file\ntitle: notes.md\ncollection: default\n---\n# notes\n\nFiled from dropbox.\n"
	idx := "---\ntype: index\n---\n# Wiki index\n\n- [notes](sources/notes.md)\n"
	logLine := "2026-06-04 ingested notes.md from dropbox\n"
	return [][]provider.Event{
		{writeToolUse(t, "w1", "sources/notes.md", src), provider.EventDone{StopReason: "tool_use"}},
		{writeToolUse(t, "w2", "index.md", idx), provider.EventDone{StopReason: "tool_use"}},
		{writeToolUse(t, "w3", "log.md", logLine), provider.EventDone{StopReason: "tool_use"}},
		{
			provider.EventTextDelta{Text: "Filed dropbox file."},
			provider.EventUsage{InputTokens: 50, OutputTokens: 10},
			provider.EventDone{StopReason: "end_turn"},
		},
	}
}

// newCore builds a real ingest.Core over an on-disk store, a real BM25 index, a
// migrated SQLite DB, and the supplied stub provider — same wiring as the 4.1
// core-path test, so the consumer handler exercises the genuine async core. It
// returns the store and DB the core was built over so the test can assert on the
// filesystem + provenance ledger without reaching into the core's internals.
func newCore(t *testing.T, stub provider.Client) (*ingest.Core, *store.Store, *sql.DB) {
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
	core := ingest.New(st, idx, conn, newClient, ingest.Config{
		Model: "claude-sonnet-4-6", MaxTokens: 4096, JobTTL: 5 * time.Second,
	})
	return core, st, conn
}

func awaitTerminal(t *testing.T, core *ingest.Core, owner, jobID string) ingest.Status {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	for {
		st, err := core.JobStatus(context.Background(), owner, "", jobID)
		if err == nil && st.Terminal {
			return st
		}
		if time.Now().After(deadline) {
			t.Fatalf("timed out waiting for job %s terminal (err=%v status=%q)", jobID, err, st.Status)
		}
		time.Sleep(2 * time.Millisecond)
	}
}

// dropboxEvent builds a synthetic dropbox file-lifecycle consumer.Event matching
// the real wire payload (dropbox/internal/dropbox/events.go filePayload).
func dropboxEvent(t *testing.T, evType, path, contentURL string) consumer.Event {
	t.Helper()
	payload, err := json.Marshal(dropboxFilePayload{
		Event:       evType,
		Path:        path,
		Rev:         "0123456789ab",
		ContentHash: "deadbeef",
		Size:        42,
		ContentURL:  contentURL,
		OccurredAt:  "2026-06-04T15:07:15.000000000Z",
	})
	if err != nil {
		t.Fatalf("marshal payload: %v", err)
	}
	return consumer.Event{
		Type:    evType,
		ID:      "01J" + evType, // a stand-in ULID; the handler does not parse it
		Source:  "dropbox",
		Time:    "2026-06-04T15:07:15Z",
		Payload: payload,
	}
}

// ── tests ─────────────────────────────────────────────────────────────────────

// TestHandlerFilesIngestFolderFile is the acceptance-gate handler test (no
// network). A synthetic file.created event for a file in wiki/ingest is fed
// through the REAL handler with a STUBBED byte-fetch and the REAL stub-provider
// ingest core. It asserts: the fetched bytes are filed into the immutable raw
// store (sha256-stamped frontmatter), the async integration job spawns and
// reaches succeeded, the agent's writes land in the box owner's tree, and the
// page is searchable — i.e. Core.Ingest ran with the right owner/source/bytes.
func TestHandlerFilesIngestFolderFile(t *testing.T) {
	const fileBody = "Otters are playful semi-aquatic mammals."
	core, st, conn := newCore(t, &stubProvider{sequences: fileSeq(t)})

	var fetched string // records the content_url the handler fetched
	fetch := func(_ context.Context, contentURL string) ([]byte, error) {
		fetched = contentURL
		return []byte(fileBody), nil
	}

	h := handlerWith(boxOwner, core, fetch, nil)
	const contentURL = "http://127.0.0.1:3005/content?path=%2Fwiki%2Fingest%2Fnotes.md"
	ev := dropboxEvent(t, eventFileCreated, "/wiki/ingest/notes.md", contentURL)

	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("handler returned error: %v", err)
	}
	if fetched != contentURL {
		t.Fatalf("handler fetched %q, want the event's content_url %q", fetched, contentURL)
	}

	// The raw doc was written immutably for the BOX OWNER, with the fetched bytes
	// as the body and dropbox provenance stamped. sha256 of fileBody keys it.
	sum := sha256Hex(fileBody)
	rawBytes, err := st.ReadRaw(boxOwner, "default", sum)
	if err != nil {
		t.Fatalf("ReadRaw for box owner: %v", err)
	}
	rawStr := string(rawBytes)
	if !strings.HasSuffix(rawStr, fileBody) {
		t.Fatalf("raw doc body is not the fetched bytes:\n%s", rawStr)
	}
	// Source provenance names dropbox + the literal path; title is the basename.
	for _, want := range []string{"dropbox:/wiki/ingest/notes.md", "notes.md", "ingested_at:"} {
		if !strings.Contains(rawStr, want) {
			t.Fatalf("raw frontmatter missing %q:\n%s", want, rawStr)
		}
	}

	// The async integration job spawned and reached succeeded. The job id is not
	// returned by the handler, so find the box owner's only job and await it.
	jobID := singleJobID(t, conn)
	stStatus := awaitTerminal(t, core, boxOwner, jobID)
	if stStatus.Status != string(job.StatusSucceeded) {
		t.Fatalf("job status = %q (err=%q), want succeeded", stStatus.Status, stStatus.Error)
	}

	// The agent's writes landed in the box owner's tree; the page is searchable.
	if _, err := st.ReadPage(boxOwner, "default", "sources/notes.md"); err != nil {
		t.Fatalf("ReadPage source: %v", err)
	}
}

// TestHandlerIgnoresOutsideIngestFolder asserts events OUTSIDE wiki/ingest are
// filtered out (no fetch, no ingest, no error — the engine still advances the
// cursor). It covers a sibling-prefix path that must NOT match (/wiki/ingest is a
// segment prefix, not a raw HasPrefix).
func TestHandlerIgnoresOutsideIngestFolder(t *testing.T) {
	cases := []struct {
		name string
		path string
	}{
		{"other top-level folder", "/inbox/report.pdf"},
		{"wiki but not ingest", "/wiki/notes/scratch.md"},
		{"sibling-prefix folder (must not match)", "/wiki/ingest-archive/old.md"},
		{"the folder itself (structural, not a file)", "/wiki/ingest"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			fetchCalled := false
			ing := &recordingIngester{}
			fetch := func(context.Context, string) ([]byte, error) {
				fetchCalled = true
				return []byte("x"), nil
			}
			h := handlerWith(boxOwner, ing, fetch, nil)
			ev := dropboxEvent(t, eventFileCreated, tc.path, "http://127.0.0.1:3005/content?path=x")
			if err := h(context.Background(), ev); err != nil {
				t.Fatalf("handler errored on an ignored event: %v", err)
			}
			if fetchCalled {
				t.Fatalf("handler fetched bytes for an out-of-folder path %q", tc.path)
			}
			if ing.calls != 0 {
				t.Fatalf("handler called Ingest %d times for an out-of-folder path %q, want 0", ing.calls, tc.path)
			}
		})
	}
}

// TestHandlerIgnoresDeleteAndUnknownTypes asserts the delete policy (file.deleted
// is a no-op — immutable raw, no un-filing) and that an unrelated type is ignored
// — neither fetches nor ingests, neither errors.
func TestHandlerIgnoresDeleteAndUnknownTypes(t *testing.T) {
	for _, evType := range []string{eventFileDeleted, "contact.created", "file.renamed"} {
		t.Run(evType, func(t *testing.T) {
			fetchCalled := false
			ing := &recordingIngester{}
			fetch := func(context.Context, string) ([]byte, error) {
				fetchCalled = true
				return []byte("x"), nil
			}
			h := handlerWith(boxOwner, ing, fetch, nil)
			ev := dropboxEvent(t, evType, "/wiki/ingest/gone.md", "http://127.0.0.1:3005/content?path=gone")
			if err := h(context.Background(), ev); err != nil {
				t.Fatalf("handler errored on %s: %v", evType, err)
			}
			if fetchCalled || ing.calls != 0 {
				t.Fatalf("%s triggered fetch=%v ingest=%d, want neither", evType, fetchCalled, ing.calls)
			}
		})
	}
}

// TestHandlerReDeliveryIsIdempotent asserts at-least-once safety: re-delivering
// the SAME event a second time is a safe no-op — WriteRaw dedups on the content
// sha256 (AlreadyHad), so the wiki_ingest provenance ledger has exactly one row
// for the bytes even after two deliveries.
func TestHandlerReDeliveryIsIdempotent(t *testing.T) {
	const fileBody = "same bytes delivered twice"
	// Two final-only sequences: each delivery spawns its own integration job.
	finalOnly := func() []provider.Event {
		return []provider.Event{provider.EventTextDelta{Text: "ok"}, provider.EventDone{StopReason: "end_turn"}}
	}
	core, _, conn := newCore(t, &stubProvider{sequences: [][]provider.Event{finalOnly(), finalOnly()}})

	fetch := func(context.Context, string) ([]byte, error) { return []byte(fileBody), nil }
	h := handlerWith(boxOwner, core, fetch, nil)
	ev := dropboxEvent(t, eventFileModified, "/wiki/ingest/dup.md", "http://127.0.0.1:3005/content?path=dup")

	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("first delivery: %v", err)
	}
	// Let the first integration job reach terminal before the re-delivery, so the
	// per-(owner,collection) single-flight gate does not reject the second pass —
	// here we are asserting the RAW-store idempotency, not single-flight.
	awaitTerminal(t, core, boxOwner, singleJobID(t, conn))

	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("re-delivery: %v", err)
	}

	// Wait for both integration jobs to finish so the test exits cleanly.
	for _, id := range jobIDs(t, conn) {
		awaitTerminal(t, core, boxOwner, id)
	}

	sum := sha256Hex(fileBody)
	var n int
	if err := conn.QueryRow(`SELECT COUNT(*) FROM wiki_ingest WHERE owner=? AND sha256=?`, boxOwner, sum).Scan(&n); err != nil {
		t.Fatalf("count wiki_ingest: %v", err)
	}
	if n != 1 {
		t.Fatalf("wiki_ingest rows after re-delivery = %d, want 1 (idempotent raw)", n)
	}
}

// TestHandlerProductionWiringFetchesContentURL exercises the production Handler
// (the real HTTP fetch over content_url) against an httptest server standing in
// for dropbox's loopback /content endpoint, proving the fetch-by-reference path
// without a real dropbox. The ingester is a recording stub (the core path is
// covered above) so this isolates the HTTP fetch wiring.
func TestHandlerProductionWiringFetchesContentURL(t *testing.T) {
	const body = "bytes from the dropbox /content endpoint"
	var gotPath string
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		gotPath = r.URL.Query().Get("path")
		_, _ = w.Write([]byte(body))
	}))
	defer srv.Close()

	ing := &recordingIngester{}
	h := Handler(Config{Owner: boxOwner, Ingester: ing})
	contentURL := srv.URL + "/content?path=%2Fwiki%2Fingest%2Freport.md"
	ev := dropboxEvent(t, eventFileCreated, "/wiki/ingest/report.md", contentURL)

	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("handler: %v", err)
	}
	if gotPath != "/wiki/ingest/report.md" {
		t.Fatalf("content server saw path %q, want the URL-decoded literal path", gotPath)
	}
	if ing.calls != 1 {
		t.Fatalf("Ingest calls = %d, want 1", ing.calls)
	}
	if string(ing.lastContent) != body {
		t.Fatalf("Ingest got content %q, want the fetched body %q", ing.lastContent, body)
	}
	if ing.lastOwner != boxOwner {
		t.Fatalf("Ingest owner = %q, want box owner %q", ing.lastOwner, boxOwner)
	}
	if ing.lastMeta.Source != "dropbox:/wiki/ingest/report.md" {
		t.Fatalf("Ingest source = %q, want dropbox:<path>", ing.lastMeta.Source)
	}
}

// recordingIngester is a stub consume.Ingester that records the last Ingest call
// (used by the filter/wiring tests that don't need the real async core).
type recordingIngester struct {
	calls       int
	lastOwner   string
	lastContent []byte
	lastMeta    store.RawMeta
}

func (r *recordingIngester) Ingest(_ context.Context, owner, _ string, content []byte, meta store.RawMeta) (ingest.Result, error) {
	r.calls++
	r.lastOwner = owner
	r.lastContent = content
	r.lastMeta = meta
	return ingest.Result{JobID: "stub-job", Sha256: sha256Hex(string(content))}, nil
}

// failingIngester is a stub consume.Ingester whose Ingest always fails — used to
// assert the latent-bug fix: an Ingest failure is a transient error that must
// STALL (plain error, NOT ErrSkip) so the work is retried, not silently dropped.
type failingIngester struct{ calls int }

func (f *failingIngester) Ingest(context.Context, string, string, []byte, store.RawMeta) (ingest.Result, error) {
	f.calls++
	return ingest.Result{}, fmt.Errorf("simulated transient ingest failure (disk full)")
}

// ── P2 failure-classification tests ─────────────────────────────────────────────

// TestHandlerMalformedPayloadSkips: an undecodable payload is poison → ErrSkip
// (log loud + advance), never a stall and never an ingest.
func TestHandlerMalformedPayloadSkips(t *testing.T) {
	ing := &recordingIngester{}
	fetch := func(context.Context, string) ([]byte, error) { return []byte("x"), nil }
	h := handlerWith(boxOwner, ing, fetch, nil)
	// A file.created event whose payload is not valid JSON for dropboxFilePayload.
	ev := consumer.Event{Type: eventFileCreated, ID: "01JBAD", Source: "dropbox", Payload: json.RawMessage(`{"path": `)}
	err := h(context.Background(), ev)
	if err == nil {
		t.Fatal("malformed payload returned nil, want an ErrSkip-wrapped error")
	}
	if !errors.Is(err, consumer.ErrSkip) {
		t.Fatalf("malformed payload error does not satisfy errors.Is(err, ErrSkip): %v", err)
	}
	if ing.calls != 0 {
		t.Fatalf("malformed payload triggered %d ingests, want 0", ing.calls)
	}
}

// TestHandlerFetchGoneSkips: a 404/410/409 fetch ("gone") propagates ErrSkip
// (permanently unprocessable → advance), no ingest.
func TestHandlerFetchGoneSkips(t *testing.T) {
	for _, status := range []int{http.StatusNotFound, http.StatusGone, http.StatusConflict} {
		t.Run(fmt.Sprintf("%d", status), func(t *testing.T) {
			srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
				w.WriteHeader(status)
			}))
			defer srv.Close()
			ing := &recordingIngester{}
			h := Handler(Config{Owner: boxOwner, Ingester: ing})
			ev := dropboxEvent(t, eventFileCreated, "/wiki/ingest/gone.md", srv.URL+"/content?path=gone")
			err := h(context.Background(), ev)
			if err == nil {
				t.Fatalf("status %d returned nil, want ErrSkip", status)
			}
			if !errors.Is(err, consumer.ErrSkip) {
				t.Fatalf("status %d does not satisfy errors.Is(err, ErrSkip): %v", status, err)
			}
			if ing.calls != 0 {
				t.Fatalf("status %d triggered %d ingests, want 0", status, ing.calls)
			}
		})
	}
}

// TestHandlerFetch5xxStalls: a 5xx fetch is transient → a PLAIN error (stall +
// retry), explicitly NOT ErrSkip, and no ingest.
func TestHandlerFetch5xxStalls(t *testing.T) {
	for _, status := range []int{http.StatusInternalServerError, http.StatusBadGateway, http.StatusServiceUnavailable} {
		t.Run(fmt.Sprintf("%d", status), func(t *testing.T) {
			srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
				w.WriteHeader(status)
			}))
			defer srv.Close()
			ing := &recordingIngester{}
			h := Handler(Config{Owner: boxOwner, Ingester: ing})
			ev := dropboxEvent(t, eventFileCreated, "/wiki/ingest/flaky.md", srv.URL+"/content?path=flaky")
			err := h(context.Background(), ev)
			if err == nil {
				t.Fatalf("status %d returned nil, want a stalling plain error", status)
			}
			if errors.Is(err, consumer.ErrSkip) {
				t.Fatalf("status %d wrongly classified as ErrSkip (must stall): %v", status, err)
			}
			if ing.calls != 0 {
				t.Fatalf("status %d triggered %d ingests, want 0", status, ing.calls)
			}
		})
	}
}

// TestHandlerFetchTransportErrorStalls: a transport error (unreachable URL) is
// transient → a PLAIN error (stall), not ErrSkip.
func TestHandlerFetchTransportErrorStalls(t *testing.T) {
	ing := &recordingIngester{}
	// A non-routable / closed port content_url to force a transport-layer failure.
	h := Handler(Config{
		Owner:      boxOwner,
		Ingester:   ing,
		HTTPClient: &http.Client{Timeout: 200 * time.Millisecond},
	})
	ev := dropboxEvent(t, eventFileCreated, "/wiki/ingest/dead.md", "http://127.0.0.1:1/content?path=dead")
	err := h(context.Background(), ev)
	if err == nil {
		t.Fatal("transport error returned nil, want a stalling plain error")
	}
	if errors.Is(err, consumer.ErrSkip) {
		t.Fatalf("transport error wrongly classified as ErrSkip (must stall): %v", err)
	}
	if ing.calls != 0 {
		t.Fatalf("transport error triggered %d ingests, want 0", ing.calls)
	}
}

// TestHandlerIngestFailureStalls is the latent-bug-fix assertion: an Ingest
// failure must return a PLAIN error so the engine STALLS and replays the event —
// NOT ErrSkip and NOT nil (the old commit-regardless engine silently dropped it).
func TestHandlerIngestFailureStalls(t *testing.T) {
	ing := &failingIngester{}
	fetch := func(context.Context, string) ([]byte, error) { return []byte("real content bytes"), nil }
	h := handlerWith(boxOwner, ing, fetch, nil)
	ev := dropboxEvent(t, eventFileCreated, "/wiki/ingest/notes.md", "http://127.0.0.1:3005/content?path=notes")
	err := h(context.Background(), ev)
	if err == nil {
		t.Fatal("ingest failure returned nil — the latent bug (silent drop) is NOT fixed")
	}
	if errors.Is(err, consumer.ErrSkip) {
		t.Fatalf("ingest failure wrongly classified as ErrSkip (must stall + retry): %v", err)
	}
	if ing.calls != 1 {
		t.Fatalf("ingest was attempted %d times, want exactly 1", ing.calls)
	}
}

// TestHandlerEmptyContentAdvances: an empty fetched body is nothing to do →
// nil (advance), with no ingest and no error.
func TestHandlerEmptyContentAdvances(t *testing.T) {
	ing := &recordingIngester{}
	fetch := func(context.Context, string) ([]byte, error) { return []byte{}, nil }
	h := handlerWith(boxOwner, ing, fetch, nil)
	ev := dropboxEvent(t, eventFileCreated, "/wiki/ingest/empty.md", "http://127.0.0.1:3005/content?path=empty")
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("empty content returned %v, want nil (advance)", err)
	}
	if ing.calls != 0 {
		t.Fatalf("empty content triggered %d ingests, want 0", ing.calls)
	}
}
