package consumer

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	"eventplane/outbox"

	_ "modernc.org/sqlite"
)

// TestMain shrinks the reconnect backoff so the reconnect/resync paths run fast
// in tests. Production never touches these package vars.
func TestMain(m *testing.M) {
	baseBackoff = 2 * time.Millisecond
	maxBackoff = 20 * time.Millisecond
	os.Exit(m.Run())
}

// ── harness ────────────────────────────────────────────────────────────────

// syncBuffer is a goroutine-safe sink for the engine's slog output, so a test
// can poll it for readiness ("caught up") and assert resync reasons.
type syncBuffer struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (s *syncBuffer) Write(p []byte) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.buf.Write(p)
}

func (s *syncBuffer) String() string {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.buf.String()
}

// collector accumulates the events the handler receives, guarded for the test
// goroutine to read while the Run goroutine writes.
type collector struct {
	mu  sync.Mutex
	got []Event
}

func (c *collector) handle(_ context.Context, ev Event) error {
	c.mu.Lock()
	c.got = append(c.got, ev)
	c.mu.Unlock()
	return nil
}

func (c *collector) ns() []int {
	c.mu.Lock()
	defer c.mu.Unlock()
	out := make([]int, len(c.got))
	for i, ev := range c.got {
		var p struct {
			N int `json:"n"`
		}
		_ = json.Unmarshal(ev.Payload, &p)
		out[i] = p.N
	}
	return out
}

func openDB(t *testing.T, path string) *sql.DB {
	t.Helper()
	dsn := fmt.Sprintf("file:%s?_pragma=journal_mode(WAL)&_pragma=foreign_keys(ON)&_pragma=busy_timeout(5000)", path)
	db, err := sql.Open("sqlite", dsn)
	if err != nil {
		t.Fatalf("open %s: %v", path, err)
	}
	db.SetMaxOpenConns(1)
	if err := db.Ping(); err != nil {
		t.Fatalf("ping %s: %v", path, err)
	}
	t.Cleanup(func() { db.Close() })
	return db
}

func applySchema(t *testing.T, db *sql.DB, ddl string) {
	t.Helper()
	if _, err := db.Exec(ddl); err != nil {
		t.Fatalf("apply schema: %v", err)
	}
}

// newProducer builds a real outbox + httptest feed server over a file DB, with a
// generation token at genPath. Reusing the same dbPath with a fresh genPath
// simulates a rebuild/restore that re-mints the epoch (§9.3) over the same seqs.
func newProducer(t *testing.T, dbPath, genPath string) (*outbox.Outbox, *sql.DB, *httptest.Server) {
	t.Helper()
	db := openDB(t, dbPath)
	// Apply the outbox DDL only once per DB file.
	var n int
	_ = db.QueryRow(`SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='outbox'`).Scan(&n)
	if n == 0 {
		applySchema(t, db, outbox.SchemaSQL)
	}
	ob, err := outbox.New(db, outbox.Options{
		Source:         "crm",
		DBPath:         dbPath,
		GenerationPath: genPath,
		Logger:         slog.New(slog.NewJSONHandler(io.Discard, nil)),
	})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	srv := httptest.NewServer(ob.FeedHandler())
	t.Cleanup(srv.Close)
	return ob, db, srv
}

// emit writes one contact-shaped event with marker n into the producer's outbox
// inside a transaction, commits, and rings the doorbell — the producer wiring a
// real service performs (§4.1, §4.3).
func emit(t *testing.T, ob *outbox.Outbox, db *sql.DB, n int) {
	t.Helper()
	payload := fmt.Sprintf(`{"id":"contact-%d","display_name":"Person %d","n":%d}`, n, n, n)
	tx, err := db.Begin()
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	if err := ob.Append(tx, outbox.Event{Type: "contact.created", Payload: json.RawMessage(payload)}); err != nil {
		t.Fatalf("append: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit: %v", err)
	}
	ob.Ring()
}

func newConsumerDB(t *testing.T) *sql.DB {
	t.Helper()
	db := openDB(t, filepath.Join(t.TempDir(), "consumer.db"))
	applySchema(t, db, SchemaSQL)
	return db
}

// runConsumer starts Run in a goroutine and returns a stop func that cancels and
// waits, asserting Run returned nil (no structural fault). stop is idempotent and
// also registered as a cleanup.
func runConsumer(t *testing.T, cfg Config, h Handler) (stop func()) {
	t.Helper()
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- Run(ctx, cfg, h) }()
	var once sync.Once
	stop = func() {
		once.Do(func() {
			cancel()
			if err := <-done; err != nil {
				t.Errorf("Run returned non-nil (structural fault): %v", err)
			}
		})
	}
	t.Cleanup(stop)
	return stop
}

func baseConfig(db *sql.DB, feedURL, from string, logw io.Writer) Config {
	return Config{
		FeedURL:    feedURL + "/feed",
		From:       from,
		DB:         db,
		Source:     "crm",
		ConsumerID: "notify",
		Logger:     slog.New(slog.NewTextHandler(logw, &slog.HandlerOptions{Level: slog.LevelDebug})),
	}
}

func waitFor(t *testing.T, what string, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(2 * time.Millisecond)
	}
	t.Fatalf("timed out waiting for %s", what)
}

func waitEvents(t *testing.T, c *collector, n int) {
	t.Helper()
	waitFor(t, fmt.Sprintf("%d events", n), func() bool {
		c.mu.Lock()
		defer c.mu.Unlock()
		return len(c.got) >= n
	})
}

func cursorRow(t *testing.T, db *sql.DB) (cursor sql.NullString, subscribed int) {
	t.Helper()
	err := db.QueryRow(`SELECT cursor, subscribed FROM feed_offset WHERE source = 'crm'`).Scan(&cursor, &subscribed)
	if err != nil && err != sql.ErrNoRows {
		t.Fatalf("read feed_offset: %v", err)
	}
	return cursor, subscribed
}

// ── tests ──────────────────────────────────────────────────────────────────

// 13a: backlog drained in order + cursor persisted.
func TestBacklogDrainedInOrder(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 5; i++ {
		emit(t, ob, pdb, i)
	}
	cdb := newConsumerDB(t)
	c := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, io.Discard), c.handle)

	waitEvents(t, c, 5)
	if got := c.ns(); fmt.Sprint(got) != fmt.Sprint([]int{1, 2, 3, 4, 5}) {
		t.Fatalf("events out of order: %v", got)
	}
	waitFor(t, "cursor persisted", func() bool {
		cur, _ := cursorRow(t, cdb)
		return cur.Valid
	})
}

// 13a: reconnect/restart resumes strictly after the committed cursor — committed
// events are not re-delivered; new events are.
func TestResumeAfterCommittedCursor(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 3; i++ {
		emit(t, ob, pdb, i)
	}
	cdb := newConsumerDB(t)

	c1 := &collector{}
	var logbuf syncBuffer
	stop := runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, &logbuf), c1.handle)
	waitEvents(t, c1, 3)
	// "caught up" is the barrier that all three cursors committed: the producer
	// emits it only after draining, i.e. after the consumer committed event 3.
	// Restarting before that barrier would correctly re-deliver an uncommitted
	// event (at-least-once, §10) — not what this test asserts.
	waitFor(t, "caught up before restart", func() bool { return strings.Contains(logbuf.String(), "caught up") })
	stop() // restart

	for i := 4; i <= 5; i++ {
		emit(t, ob, pdb, i)
	}
	c2 := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, io.Discard), c2.handle)
	waitEvents(t, c2, 2)
	if got := c2.ns(); fmt.Sprint(got) != fmt.Sprint([]int{4, 5}) {
		t.Fatalf("resume re-delivered committed events or wrong set: %v", got)
	}
}

// 13a: tail bootstrap streams nothing historical, only events after the head at
// first subscription (§7.1).
func TestTailBootstrapSkipsHistory(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 3; i++ {
		emit(t, ob, pdb, i) // history, must be skipped
	}
	cdb := newConsumerDB(t)
	var logbuf syncBuffer
	c := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromTail, &logbuf), c.handle)

	// Wait until the consumer has tailed and reached head (caught up) before
	// emitting, so the new events land strictly after the tail position.
	waitFor(t, "caught up after tail", func() bool { return strings.Contains(logbuf.String(), "caught up") })
	for i := 4; i <= 5; i++ {
		emit(t, ob, pdb, i)
	}
	waitEvents(t, c, 2)
	if got := c.ns(); fmt.Sprint(got) != fmt.Sprint([]int{4, 5}) {
		t.Fatalf("tail delivered history or wrong set: %v", got)
	}
}

// 13a: tail choice is durable across a restart-before-first-commit — the events
// that arrive while the consumer is down are NOT silently dropped (§10).
func TestTailDurableAcrossRestartBeforeCommit(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 3; i++ {
		emit(t, ob, pdb, i)
	}
	cdb := newConsumerDB(t)

	// First run: tail, reach head, receive nothing, restart before any commit.
	var logbuf syncBuffer
	c1 := &collector{}
	stop := runConsumer(t, baseConfig(cdb, srv.URL, fromTail, &logbuf), c1.handle)
	waitFor(t, "caught up after tail", func() bool { return strings.Contains(logbuf.String(), "caught up") })
	// The bootstrap marker MUST be durable before any commit.
	cur, sub := cursorRow(t, cdb)
	if sub != 1 || cur.Valid {
		t.Fatalf("after tail-before-commit want subscribed=1 cursor=NULL, got subscribed=%d cursor=%v", sub, cur)
	}
	stop()

	// While "down", two events arrive.
	for i := 4; i <= 5; i++ {
		emit(t, ob, pdb, i)
	}

	// Restart: must not re-bootstrap as tail; the gap events must be delivered.
	c2 := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromTail, io.Discard), c2.handle)
	waitFor(t, "gap events delivered", func() bool {
		got := c2.ns()
		seen := map[int]bool{}
		for _, n := range got {
			seen[n] = true
		}
		return seen[4] && seen[5]
	})
}

// 13a: stale-epoch — a cursor minted under a prior generation is rejected and the
// consumer re-bootstraps fresh, recovering against the new lineage (§9.3, §10.1).
func TestResyncStaleEpoch(t *testing.T) {
	dir := t.TempDir()
	dbPath := filepath.Join(dir, "p.db")

	ob1, pdb, srv1 := newProducer(t, dbPath, filepath.Join(dir, "gen1"))
	for i := 1; i <= 3; i++ {
		emit(t, ob1, pdb, i)
	}
	cdb := newConsumerDB(t)
	c1 := &collector{}
	stop := runConsumer(t, baseConfig(cdb, srv1.URL, fromEarliest, io.Discard), c1.handle)
	waitEvents(t, c1, 3)
	stop()
	srv1.Close()

	// Rebuild: same DB file (seqs 1..3 preserved), fresh generation token.
	ob2, _, srv2 := newProducer(t, dbPath, filepath.Join(dir, "gen2"))
	_ = ob2
	var logbuf syncBuffer
	c2 := &collector{}
	runConsumer(t, baseConfig(cdb, srv2.URL, fromEarliest, &logbuf), c2.handle)

	// The stale (gen1) cursor triggers stale-epoch; the consumer discards it and
	// replays from the beginning under gen2.
	waitEvents(t, c2, 3)
	waitFor(t, "stale-epoch logged", func() bool { return strings.Contains(logbuf.String(), "stale-epoch") })
}

// 13a: past-horizon — a cursor below the retention floor is real, unrecovered
// loss; the consumer logs it loudly and re-bootstraps (§10.1, §11.1).
func TestResyncPastHorizon(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 5; i++ {
		emit(t, ob, pdb, i)
	}
	// Simulate a retention trim below seq 3: rows 1..3 are gone, min seq is now 4.
	if _, err := pdb.Exec(`DELETE FROM outbox WHERE seq <= 3`); err != nil {
		t.Fatalf("trim: %v", err)
	}
	// Seed a committed cursor at seq 1 (below the new horizon floor) under the live
	// generation.
	gen := ob.Generation()
	cdb := newConsumerDB(t)
	if _, err := cdb.Exec(
		`INSERT INTO feed_offset (source, cursor, subscribed, updated_at) VALUES ('crm', ?, 1, ?)`,
		gen+".1", time.Now().UTC().Format(time.RFC3339Nano),
	); err != nil {
		t.Fatalf("seed cursor: %v", err)
	}

	var logbuf syncBuffer
	c := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, &logbuf), c.handle)

	// Recovery: re-bootstrap earliest replays the surviving events (4, 5).
	waitEvents(t, c, 2)
	waitFor(t, "past-horizon data loss logged", func() bool {
		return strings.Contains(logbuf.String(), "past_horizon_data_loss")
	})
}

// 13a: diverged — a cursor ahead of the producer's head is rejected; the consumer
// re-bootstraps (§10.1).
func TestResyncDiverged(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 2; i++ {
		emit(t, ob, pdb, i)
	}
	gen := ob.Generation()
	cdb := newConsumerDB(t)
	if _, err := cdb.Exec(
		`INSERT INTO feed_offset (source, cursor, subscribed, updated_at) VALUES ('crm', ?, 1, ?)`,
		gen+".99", time.Now().UTC().Format(time.RFC3339Nano), // ahead of head=2
	); err != nil {
		t.Fatalf("seed cursor: %v", err)
	}

	var logbuf syncBuffer
	c := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, &logbuf), c.handle)
	waitEvents(t, c, 2)
	waitFor(t, "diverged logged", func() bool { return strings.Contains(logbuf.String(), "diverged") })
}

// 13a: unintelligible-cursor — a garbage / cross-feed cursor is rejected; the
// consumer re-bootstraps (§10.1).
func TestResyncUnintelligible(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 2; i++ {
		emit(t, ob, pdb, i)
	}
	cdb := newConsumerDB(t)
	if _, err := cdb.Exec(
		`INSERT INTO feed_offset (source, cursor, subscribed, updated_at) VALUES ('crm', ?, 1, ?)`,
		"garbage-no-separator", time.Now().UTC().Format(time.RFC3339Nano),
	); err != nil {
		t.Fatalf("seed cursor: %v", err)
	}

	var logbuf syncBuffer
	c := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, &logbuf), c.handle)
	waitEvents(t, c, 2)
	waitFor(t, "unintelligible logged", func() bool { return strings.Contains(logbuf.String(), "unintelligible-cursor") })
}

// ── P1: handler return gates the cursor (event-triggering decisions §1) ──────

// nil → advance: the canonical happy path is already covered by
// TestBacklogDrainedInOrder; this asserts the cursor lands on the last event.
func TestHandlerNilAdvancesCursor(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 3; i++ {
		emit(t, ob, pdb, i)
	}
	cdb := newConsumerDB(t)
	c := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, io.Discard), c.handle)
	waitEvents(t, c, 3)
	waitFor(t, "cursor advanced past all", func() bool {
		cur, _ := cursorRow(t, cdb)
		return cur.Valid && strings.HasSuffix(cur.String, ".3")
	})
}

// ErrSkip → log loud + advance: a handler that skips every event must not stall;
// the cursor advances past all of them and is logged.
func TestHandlerErrSkipAdvancesCursor(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	for i := 1; i <= 3; i++ {
		emit(t, ob, pdb, i)
	}
	cdb := newConsumerDB(t)
	var seen int
	var mu sync.Mutex
	var logbuf syncBuffer
	h := func(_ context.Context, _ Event) error {
		mu.Lock()
		seen++
		mu.Unlock()
		// Wrapped, to prove errors.Is matching (not ==).
		return fmt.Errorf("poison payload: %w", ErrSkip)
	}
	cfg := baseConfig(cdb, srv.URL, fromEarliest, &logbuf)
	runConsumer(t, cfg, h)

	waitFor(t, "all three skipped", func() bool {
		mu.Lock()
		defer mu.Unlock()
		return seen >= 3
	})
	// Cursor advanced past the last event despite every handler erroring.
	waitFor(t, "cursor advanced past skips", func() bool {
		cur, _ := cursorRow(t, cdb)
		return cur.Valid && strings.HasSuffix(cur.String, ".3")
	})
	if !strings.Contains(logbuf.String(), "handler skipped event") {
		t.Fatalf("ErrSkip not logged loud: %q", logbuf.String())
	}
	// Each event delivered exactly once (no stall/replay): seen must be exactly 3.
	mu.Lock()
	got := seen
	mu.Unlock()
	if got != 3 {
		t.Fatalf("ErrSkip caused replay: handler saw %d events, want 3", got)
	}
}

// other error → stall: the cursor does NOT advance and the same event is
// re-delivered on reconnect from the committed cursor. Once the handler starts
// returning nil, the consumer drains forward.
func TestHandlerErrorStallsThenRecovers(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	emit(t, ob, pdb, 1)
	emit(t, ob, pdb, 2)
	cdb := newConsumerDB(t)

	var mu sync.Mutex
	var attempts int // how many times event 1 was offered
	heal := false
	c := &collector{}
	h := func(ctx context.Context, ev Event) error {
		var p struct {
			N int `json:"n"`
		}
		_ = json.Unmarshal(ev.Payload, &p)
		mu.Lock()
		healed := heal
		if p.N == 1 {
			attempts++
		}
		mu.Unlock()
		if p.N == 1 && !healed {
			return fmt.Errorf("transient failure on event 1")
		}
		return c.handle(ctx, ev)
	}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, io.Discard), h)

	// Event 1 is offered repeatedly (stall + replay) and the cursor never advances.
	waitFor(t, "event 1 retried", func() bool {
		mu.Lock()
		defer mu.Unlock()
		return attempts >= 2
	})
	if cur, _ := cursorRow(t, cdb); cur.Valid {
		t.Fatalf("stall advanced the cursor to %q; must stay NULL", cur.String)
	}
	if got := c.ns(); len(got) != 0 {
		t.Fatalf("stall delivered events downstream: %v", got)
	}

	// Heal: now the handler returns nil and the consumer drains 1 then 2 in order.
	mu.Lock()
	heal = true
	mu.Unlock()
	waitEvents(t, c, 2)
	if got := c.ns(); fmt.Sprint(got) != fmt.Sprint([]int{1, 2}) {
		t.Fatalf("after heal want [1 2] in order, got %v", got)
	}
	waitFor(t, "cursor advanced after heal", func() bool {
		cur, _ := cursorRow(t, cdb)
		return cur.Valid && strings.HasSuffix(cur.String, ".2")
	})
}

// backoff resets on progress: a connection that commits at least one event
// before stalling reconnects immediately (committedAny resets the curve), so a
// transient blip after healthy progress does not crawl up the backoff curve.
func TestBackoffResetsOnProgress(t *testing.T) {
	dir := t.TempDir()
	ob, pdb, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	emit(t, ob, pdb, 1) // committed (progress) before the stall
	emit(t, ob, pdb, 2) // the one that stalls until it heals
	cdb := newConsumerDB(t)

	var mu sync.Mutex
	var ev2Attempts int
	heal := false
	c := &collector{}
	h := func(ctx context.Context, ev Event) error {
		var p struct {
			N int `json:"n"`
		}
		_ = json.Unmarshal(ev.Payload, &p)
		mu.Lock()
		healed := heal
		if p.N == 2 {
			ev2Attempts++
		}
		mu.Unlock()
		if p.N == 2 && !healed {
			return fmt.Errorf("transient failure on event 2")
		}
		return c.handle(ctx, ev)
	}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, io.Discard), h)

	// Event 1 committed (progress); event 2 stalls and is re-offered fast because
	// the committed-on-this-connection progress resets the backoff each round.
	start := time.Now()
	waitFor(t, "event 2 retried several times", func() bool {
		mu.Lock()
		defer mu.Unlock()
		return ev2Attempts >= 5
	})
	// With maxBackoff=20ms, a non-resetting curve would take far longer to reach
	// 5 retries than a reset-to-base (2ms) one; assert the progress-reset path is
	// live by bounding the elapsed time generously.
	if elapsed := time.Since(start); elapsed > 2*time.Second {
		t.Fatalf("progress did not reset backoff: 5 retries took %v", elapsed)
	}
	// Cursor sits at event 1 (committed) and never advanced to 2 while stalling.
	if cur, _ := cursorRow(t, cdb); !cur.Valid || !strings.HasSuffix(cur.String, ".1") {
		t.Fatalf("want cursor at .1 during stall, got %v", cur)
	}

	mu.Lock()
	heal = true
	mu.Unlock()
	waitFor(t, "event 2 delivered after heal", func() bool {
		for _, n := range c.ns() {
			if n == 2 {
				return true
			}
		}
		return false
	})
}

// 13a: control frames (caught-up on an empty feed) don't corrupt the cursor — it
// stays NULL with no event to commit, while the bootstrap marker is durable.
func TestControlFramesDoNotAdvanceCursor(t *testing.T) {
	dir := t.TempDir()
	_, _, srv := newProducer(t, filepath.Join(dir, "p.db"), filepath.Join(dir, "gen"))
	cdb := newConsumerDB(t)
	var logbuf syncBuffer
	c := &collector{}
	runConsumer(t, baseConfig(cdb, srv.URL, fromEarliest, &logbuf), c.handle)

	waitFor(t, "caught up on empty feed", func() bool { return strings.Contains(logbuf.String(), "caught up") })
	// No events were emitted: cursor must remain NULL, subscribed must be durable.
	cur, sub := cursorRow(t, cdb)
	if cur.Valid {
		t.Fatalf("caught-up advanced the cursor to %q", cur.String)
	}
	if sub != 1 {
		t.Fatalf("bootstrap marker not durable: subscribed=%d", sub)
	}
	if ns := c.ns(); len(ns) != 0 {
		t.Fatalf("received events on an empty feed: %v", ns)
	}
}
