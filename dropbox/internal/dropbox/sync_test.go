package dropbox

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"io"
	"testing"
	"time"
)

// sync_test.go is the engine gate's backbone (PLAN.md §10): a recording
// EventSink + a fake dropboxAPI returning canned deltas, over a REAL temp store
// + mirror, so the load-bearing rules 1–6 are exercised deterministically
// WITHOUT live Dropbox.

// ── recording EventSink ──────────────────────────────────────────────────────

// capturingSink is a real EventSink that appends NOTHING to the DB (so no outbox
// table is required) but records each emitted FileEvent and counts post-commit
// Ring calls. It lets a test assert on the exact emitted event stream.
type capturingSink struct {
	events []FileEvent
	rings  int
}

func (c *capturingSink) AppendFileEvent(tx *sql.Tx, ev FileEvent) error {
	c.events = append(c.events, ev)
	return nil
}

func (c *capturingSink) Ring() { c.rings++ }

func (c *capturingSink) eventTypes() []string {
	out := make([]string, 0, len(c.events))
	for _, e := range c.events {
		out = append(out, e.Type)
	}
	return out
}

// ── fake Dropbox client ──────────────────────────────────────────────────────

type fakeClient struct {
	listResult ListResult
	pages      []ListResult
	longpolls  []LongpollResult

	downloads     map[string]downloadResp
	downloadErr   map[string]error
	downloadCalls map[string]int

	listCalls     int
	continueCalls int
}

type downloadResp struct {
	data []byte
	meta FileMeta
}

func newFakeClient() *fakeClient {
	return &fakeClient{
		downloads:     map[string]downloadResp{},
		downloadErr:   map[string]error{},
		downloadCalls: map[string]int{},
	}
}

func (f *fakeClient) ListFolder(ctx context.Context) (ListResult, error) {
	f.listCalls++
	return f.listResult, nil
}

func (f *fakeClient) ListFolderContinue(ctx context.Context, cursor string) (ListResult, error) {
	f.continueCalls++
	if len(f.pages) == 0 {
		return ListResult{Cursor: cursor, HasMore: false}, nil
	}
	p := f.pages[0]
	f.pages = f.pages[1:]
	return p, nil
}

func (f *fakeClient) Longpoll(ctx context.Context, cursor string) (LongpollResult, error) {
	if len(f.longpolls) == 0 {
		return LongpollResult{Changes: false}, nil
	}
	lr := f.longpolls[0]
	f.longpolls = f.longpolls[1:]
	return lr, nil
}

func (f *fakeClient) Download(ctx context.Context, path, rev string) ([]byte, FileMeta, error) {
	key := foldPath(path)
	f.downloadCalls[key]++
	if err, ok := f.downloadErr[key]; ok {
		return nil, FileMeta{}, err
	}
	resp, ok := f.downloads[key]
	if !ok {
		return nil, FileMeta{}, fmt.Errorf("fakeClient: no canned download for %q", path)
	}
	return resp.data, resp.meta, nil
}

// addFile registers canned download bytes for path and returns a file DeltaEntry
// referencing them.
func (f *fakeClient) addFile(path, rev, hash string, data []byte) DeltaEntry {
	f.downloads[foldPath(path)] = downloadResp{
		data: data,
		meta: FileMeta{PathDisplay: path, PathLower: foldPath(path), Rev: rev, ContentHash: hash, Size: uint64(len(data))},
	}
	return DeltaEntry{Tag: TagFile, PathDisplay: path, PathLower: foldPath(path), Rev: rev, ContentHash: hash, Size: uint64(len(data))}
}

func deletedEntry(path string) DeltaEntry {
	return DeltaEntry{Tag: TagDeleted, PathDisplay: path, PathLower: foldPath(path)}
}

// ── harness ──────────────────────────────────────────────────────────────────

func newEngineHarness(t *testing.T, fc *fakeClient) (*Engine, *Service, *capturingSink) {
	t.Helper()
	conn := openStoreDB(t)
	mirror, err := NewMirror(t.TempDir() + "/mirror")
	if err != nil {
		t.Fatalf("mirror: %v", err)
	}
	sink := &capturingSink{}
	svc := &Service{DB: conn, Store: NewStore(), Mirror: mirror, Outbox: sink, Now: time.Now}
	eng := NewEngine(svc, EngineOptions{Client: fc, MaxEntryRetries: 3, Backoff: time.Millisecond})
	return eng, svc, sink
}

// applyEntries runs applyPage over a single page of entries with a fresh retry
// map, returning any error. It is the unit under test for the apply logic.
func applyEntries(t *testing.T, eng *Engine, entries ...DeltaEntry) error {
	t.Helper()
	return eng.applyPage(context.Background(), ListResult{Entries: entries, Cursor: "cur-after"}, map[string]int{})
}

func ctx() context.Context { return context.Background() }

func readContent(svc *Service, path string, rev *string) ([]byte, FileRow, error) {
	row, err := svc.Content(path, rev)
	if err != nil {
		return nil, row, err
	}
	f, _, err := svc.Mirror.Open(row.Path)
	if err != nil {
		return nil, row, err
	}
	defer f.Close()
	data, err := io.ReadAll(f)
	return data, row, err
}

// ── Rule 5: create / modify / rev-dedup ──────────────────────────────────────

func TestRule5_CreateModifyRevDedup(t *testing.T) {
	fc := newFakeClient()
	eng, svc, sink := newEngineHarness(t, fc)

	// Create.
	e1 := fc.addFile("/inbox/report.pdf", "rev1", ContentHash([]byte("hello")), []byte("hello"))
	if err := applyEntries(t, eng, e1); err != nil {
		t.Fatalf("create: %v", err)
	}
	if got := sink.eventTypes(); len(got) != 1 || got[0] != EventFileCreated {
		t.Fatalf("want [file.created], got %v", got)
	}
	if data, _, err := readContent(svc, "/inbox/report.pdf", nil); err != nil || string(data) != "hello" {
		t.Fatalf("content after create: data=%q err=%v", data, err)
	}

	// Re-apply same rev → no-op (no event, no re-download).
	sink.events = nil
	if err := applyEntries(t, eng, e1); err != nil {
		t.Fatalf("rev-dedup re-apply: %v", err)
	}
	if len(sink.events) != 0 {
		t.Fatalf("rev-dedup should emit nothing, got %v", sink.eventTypes())
	}
	if fc.downloadCalls[foldPath("/inbox/report.pdf")] != 1 {
		t.Fatalf("rev-dedup should not re-download, calls=%d", fc.downloadCalls[foldPath("/inbox/report.pdf")])
	}

	// Modify (rev change) → file.modified.
	sink.events = nil
	e2 := fc.addFile("/inbox/report.pdf", "rev2", ContentHash([]byte("world")), []byte("world"))
	if err := applyEntries(t, eng, e2); err != nil {
		t.Fatalf("modify: %v", err)
	}
	if got := sink.eventTypes(); len(got) != 1 || got[0] != EventFileModified {
		t.Fatalf("want [file.modified], got %v", got)
	}
	if data, _, err := readContent(svc, "/inbox/report.pdf", nil); err != nil || string(data) != "world" {
		t.Fatalf("content after modify: data=%q err=%v", data, err)
	}
}

// ── Rule 1: folder delete fans out to one file.deleted per row ────────────────

func TestRule1_FolderDeleteSubtreeFanout(t *testing.T) {
	fc := newFakeClient()
	eng, svc, sink := newEngineHarness(t, fc)

	a := fc.addFile("/proj/a.txt", "r1", ContentHash([]byte("a")), []byte("a"))
	b := fc.addFile("/proj/sub/b.txt", "r2", ContentHash([]byte("b")), []byte("b"))
	c := fc.addFile("/other/c.txt", "r3", ContentHash([]byte("c")), []byte("c"))
	if err := applyEntries(t, eng, a, b, c); err != nil {
		t.Fatalf("seed: %v", err)
	}
	sink.events = nil

	// One DeletedMetadata for the folder /proj → fan out over /proj/a.txt and
	// /proj/sub/b.txt (NOT /other/c.txt).
	if err := applyEntries(t, eng, deletedEntry("/proj")); err != nil {
		t.Fatalf("folder delete: %v", err)
	}
	if got := sink.eventTypes(); len(got) != 2 {
		t.Fatalf("folder delete should emit 2 file.deleted, got %v", got)
	}
	for _, e := range sink.events {
		if e.Type != EventFileDeleted {
			t.Fatalf("non-delete event in fanout: %v", e.Type)
		}
	}
	// /other/c.txt survives; /proj files gone from index and mirror.
	if _, _, err := readContent(svc, "/proj/a.txt", nil); !errors.Is(err, ErrNotFound) {
		t.Fatalf("a.txt should be gone, err=%v", err)
	}
	if _, _, err := readContent(svc, "/proj/sub/b.txt", nil); !errors.Is(err, ErrNotFound) {
		t.Fatalf("b.txt should be gone, err=%v", err)
	}
	if data, _, err := readContent(svc, "/other/c.txt", nil); err != nil || string(data) != "c" {
		t.Fatalf("c.txt should survive: data=%q err=%v", data, err)
	}
	_ = c
}

func TestFolderMkdirIndexesEmptyDirectory(t *testing.T) {
	// R-JZVV-Q0C3
	fc := newFakeClient()
	eng, svc, sink := newEngineHarness(t, fc)
	if err := applyEntries(t, eng, DeltaEntry{Tag: TagFolder, PathDisplay: "/empty", PathLower: "/empty"}); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	entry, err := svc.Stat("/EMPTY")
	if err != nil || entry.Kind != KindDir || entry.Path != "/empty" {
		t.Fatalf("Stat empty directory = %+v, %v", entry, err)
	}
	entries, err := svc.List("", "", 10)
	if err != nil || len(entries) != 1 || entries[0].Kind != KindDir || entries[0].Path != "/empty" {
		t.Fatalf("List empty directory = %+v, %v", entries, err)
	}
	if len(sink.events) != 0 || sink.rings != 0 {
		t.Fatalf("mkdir emitted lifecycle activity: events=%v rings=%d", sink.events, sink.rings)
	}
}

func TestFolderDeleteRemovesDirectoryRowsAndFansOutFiles(t *testing.T) {
	// R-K13S-3S2S
	fc := newFakeClient()
	eng, svc, sink := newEngineHarness(t, fc)
	entries := []DeltaEntry{
		{Tag: TagFolder, PathDisplay: "/a", PathLower: "/a"},
		{Tag: TagFolder, PathDisplay: "/a/sub", PathLower: "/a/sub"},
		fc.addFile("/a/x.md", "r1", ContentHash([]byte("x")), []byte("x")),
		fc.addFile("/a/sub/y.md", "r2", ContentHash([]byte("y")), []byte("y")),
	}
	if err := applyEntries(t, eng, entries...); err != nil {
		t.Fatalf("seed: %v", err)
	}
	sink.events, sink.rings = nil, 0
	if err := applyEntries(t, eng, deletedEntry("/a")); err != nil {
		t.Fatalf("delete: %v", err)
	}
	if len(sink.events) != 2 || sink.rings != 1 {
		t.Fatalf("events/rings = %d/%d, want 2/1", len(sink.events), sink.rings)
	}
	for _, ev := range sink.events {
		if ev.Type != EventFileDeleted {
			t.Fatalf("event = %+v, want file.deleted", ev)
		}
	}
	for _, p := range []string{"/a", "/a/sub", "/a/x.md", "/a/sub/y.md"} {
		if _, err := svc.Stat(p); !errors.Is(err, ErrNotFound) {
			t.Fatalf("%s remains after delete: %v", p, err)
		}
	}
}

// ── Rule 2: delete on an already-absent path emits nothing ────────────────────

func TestRule2_AbsentPathDeleteEmitsNothing(t *testing.T) {
	fc := newFakeClient()
	eng, _, sink := newEngineHarness(t, fc)

	// First delete with no indexed row → idempotent unlink, no event.
	if err := applyEntries(t, eng, deletedEntry("/ghost.txt")); err != nil {
		t.Fatalf("absent delete: %v", err)
	}
	if len(sink.events) != 0 {
		t.Fatalf("absent-path delete must emit nothing, got %v", sink.eventTypes())
	}
	if sink.rings != 0 {
		t.Fatalf("absent-path delete must not ring, rings=%d", sink.rings)
	}

	// Now seed a real file, delete it (1 event), then REPLAY the delete delta:
	// the second delete sees the row already gone → zero new events.
	e := fc.addFile("/real.txt", "r1", ContentHash([]byte("x")), []byte("x"))
	if err := applyEntries(t, eng, e); err != nil {
		t.Fatalf("seed real: %v", err)
	}
	sink.events = nil
	if err := applyEntries(t, eng, deletedEntry("/real.txt")); err != nil {
		t.Fatalf("delete real: %v", err)
	}
	if got := sink.eventTypes(); len(got) != 1 || got[0] != EventFileDeleted {
		t.Fatalf("first delete should emit one file.deleted, got %v", got)
	}
	sink.events = nil
	if err := applyEntries(t, eng, deletedEntry("/real.txt")); err != nil {
		t.Fatalf("replay delete: %v", err)
	}
	if len(sink.events) != 0 {
		t.Fatalf("replayed delete on absent path must emit nothing, got %v", sink.eventTypes())
	}
}

// ── Rule 3: per-continue-page cursor advance ──────────────────────────────────

func TestRule3_PerPageCursorAdvance(t *testing.T) {
	fc := newFakeClient()
	eng, svc, _ := newEngineHarness(t, fc)

	// drain over two pages: page 1 (HasMore) then page 2 (final). After the FULL
	// drain the cursor is page 2's. But the key property is per-page: we assert
	// the cursor is set after page 1 by draining only page 1 first.
	p1 := fc.addFile("/p1.txt", "r1", ContentHash([]byte("1")), []byte("1"))
	p2 := fc.addFile("/p2.txt", "r2", ContentHash([]byte("2")), []byte("2"))

	// First continue page applied in isolation via applyPage advances the cursor.
	if err := eng.applyPage(ctx(), ListResult{Entries: []DeltaEntry{p1}, Cursor: "cursor-page-1", HasMore: true}, map[string]int{}); err != nil {
		t.Fatalf("apply page 1: %v", err)
	}
	if cur := readCursorT(t, svc); cur != "cursor-page-1" {
		t.Fatalf("cursor after page 1 should be 'cursor-page-1', got %q", cur)
	}
	// Second page advances it again.
	if err := eng.applyPage(ctx(), ListResult{Entries: []DeltaEntry{p2}, Cursor: "cursor-page-2", HasMore: false}, map[string]int{}); err != nil {
		t.Fatalf("apply page 2: %v", err)
	}
	if cur := readCursorT(t, svc); cur != "cursor-page-2" {
		t.Fatalf("cursor after page 2 should be 'cursor-page-2', got %q", cur)
	}
}

func readCursorT(t *testing.T, svc *Service) string {
	t.Helper()
	tx, err := svc.DB.Begin()
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	defer tx.Rollback()
	cur, _, err := svc.Store.GetCursor(tx)
	if err != nil {
		t.Fatalf("get cursor: %v", err)
	}
	return cur
}

// ── Rule 4: poison-entry bound marks error and advances ───────────────────────

func TestRule4_PoisonEntryBoundMarksErrorAndAdvances(t *testing.T) {
	fc := newFakeClient()
	eng, svc, _ := newEngineHarness(t, fc) // MaxEntryRetries = 3

	// A file entry whose download always fails (e.g. persistent integrity error).
	bad := DeltaEntry{Tag: TagFile, PathDisplay: "/poison.bin", PathLower: foldPath("/poison.bin"), Rev: "rx", ContentHash: "deadbeef", Size: 5}
	fc.downloadErr[foldPath("/poison.bin")] = errors.New("boom")
	good := fc.addFile("/ok.txt", "r1", ContentHash([]byte("ok")), []byte("ok"))

	page := ListResult{Entries: []DeltaEntry{bad, good}, Cursor: "cur-poison", HasMore: false}
	retries := map[string]int{}

	// First (MaxEntryRetries-1) passes return an error (cursor held).
	var lastErr error
	for i := 0; i < 2; i++ {
		lastErr = eng.applyPage(ctx(), page, retries)
		if lastErr == nil {
			t.Fatalf("pass %d should error (cursor held), got nil", i)
		}
	}
	// The bound-th pass marks the row errored and advances past it.
	if err := eng.applyPage(ctx(), page, retries); err != nil {
		t.Fatalf("bound pass should advance past poison, got %v", err)
	}
	// Cursor advanced.
	if cur := readCursorT(t, svc); cur != "cur-poison" {
		t.Fatalf("cursor should advance past poison to 'cur-poison', got %q", cur)
	}
	// failed_files surfaces it.
	tx, _ := svc.DB.Begin()
	n, _ := svc.Store.FailedFiles(tx)
	tx.Rollback()
	if n != 1 {
		t.Fatalf("failed_files should be 1, got %d", n)
	}
	h, _ := svc.Health("o@x", "c")
	if h.FailedFiles != 1 {
		t.Fatalf("health failed_files should be 1, got %d", h.FailedFiles)
	}
	// The good entry on the same page still applied.
	if data, _, err := readContent(svc, "/ok.txt", nil); err != nil || string(data) != "ok" {
		t.Fatalf("good entry should apply despite poison: data=%q err=%v", data, err)
	}
	_ = good
}

// ── Rule 6: case-only rename → file.modified (not delete+create) ──────────────

func TestRule6_CaseOnlyRenameModified(t *testing.T) {
	fc := newFakeClient()
	eng, svc, sink := newEngineHarness(t, fc)

	// Seed /report.pdf.
	e1 := fc.addFile("/report.pdf", "rev1", ContentHash([]byte("data")), []byte("data"))
	if err := applyEntries(t, eng, e1); err != nil {
		t.Fatalf("seed: %v", err)
	}
	sink.events = nil
	downloadsBefore := fc.downloadCalls[foldPath("/report.pdf")]

	// Dropbox surfaces a case-only rename as a file metadata entry: same rev,
	// path_lower matches, display path differs (/Report.pdf).
	rename := DeltaEntry{Tag: TagFile, PathDisplay: "/Report.pdf", PathLower: foldPath("/report.pdf"), Rev: "rev1", ContentHash: ContentHash([]byte("data")), Size: 4}
	if err := applyEntries(t, eng, rename); err != nil {
		t.Fatalf("case rename: %v", err)
	}
	if got := sink.eventTypes(); len(got) != 1 || got[0] != EventFileModified {
		t.Fatalf("case-only rename must emit exactly [file.modified], got %v", got)
	}
	// No re-download (it was a rename, not a fetch).
	if fc.downloadCalls[foldPath("/report.pdf")] != downloadsBefore {
		t.Fatalf("case-only rename must not re-download")
	}
	// Index now carries the new display path; content resolves via either case.
	if data, row, err := readContent(svc, "/report.pdf", nil); err != nil || string(data) != "data" || row.Path != "/Report.pdf" {
		t.Fatalf("after rename: data=%q display=%q err=%v", data, row.Path, err)
	}
}

// ── bootstrap: first boot emits file.created for every existing file ──────────

func TestBootstrap_FirstBootEmitsCreatedForAll(t *testing.T) {
	fc := newFakeClient()
	eng, svc, sink := newEngineHarness(t, fc)

	a := fc.addFile("/a.txt", "r1", ContentHash([]byte("a")), []byte("a"))
	b := fc.addFile("/b.txt", "r2", ContentHash([]byte("b")), []byte("b"))
	fc.listResult = ListResult{Entries: []DeltaEntry{a, b}, Cursor: "boot-cursor", HasMore: false}

	if err := eng.bootstrap(ctx()); err != nil {
		t.Fatalf("bootstrap: %v", err)
	}
	got := sink.eventTypes()
	if len(got) != 2 || got[0] != EventFileCreated || got[1] != EventFileCreated {
		t.Fatalf("first boot should emit file.created for every file, got %v", got)
	}
	if cur := readCursorT(t, svc); cur != "boot-cursor" {
		t.Fatalf("bootstrap should persist cursor, got %q", cur)
	}
	// Second bootstrap is a no-op (cursor present).
	sink.events = nil
	if err := eng.bootstrap(ctx()); err != nil {
		t.Fatalf("second bootstrap: %v", err)
	}
	if len(sink.events) != 0 {
		t.Fatalf("second bootstrap should be a no-op, got %v", sink.eventTypes())
	}
}

// ── Content rev-mismatch (exact-bytes contract → 409 in Phase 5) ──────────────

func TestContentRevMismatch(t *testing.T) {
	fc := newFakeClient()
	eng, svc, _ := newEngineHarness(t, fc)
	e := fc.addFile("/f.txt", "rev1", ContentHash([]byte("y")), []byte("y"))
	if err := applyEntries(t, eng, e); err != nil {
		t.Fatalf("seed: %v", err)
	}
	stale := "rev0"
	if _, _, err := readContent(svc, "/f.txt", &stale); !errors.Is(err, ErrRevMismatch) {
		t.Fatalf("stale rev should yield ErrRevMismatch, got %v", err)
	}
	cur := "rev1"
	if data, _, err := readContent(svc, "/f.txt", &cur); err != nil || string(data) != "y" {
		t.Fatalf("matching rev should serve bytes: data=%q err=%v", data, err)
	}
}
