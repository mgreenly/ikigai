package run

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"wiki/internal/db"

	_ "modernc.org/sqlite"
)

// fakeOutbox records appended events (on whatever tx) and Ring calls, so the
// dead-letter atomicity + emission can be asserted without a real eventplane.
type fakeOutbox struct {
	mu      sync.Mutex
	events  []outboxEvent
	rings   int
	onTx    func(*sql.Tx) // optional hook to observe the tx
	failTx  bool
}

func (f *fakeOutbox) Append(tx *sql.Tx, ev outboxEvent) error {
	if f.onTx != nil {
		f.onTx(tx)
	}
	if f.failTx {
		return errors.New("outbox boom")
	}
	f.mu.Lock()
	f.events = append(f.events, ev)
	f.mu.Unlock()
	return nil
}

func (f *fakeOutbox) Ring() {
	f.mu.Lock()
	f.rings++
	f.mu.Unlock()
}

// policyStore builds a Store with an injected clock, zero-jitter randFloat, a low
// attempts threshold, and the given outbox, over a migrated temp DB.
func policyStore(t *testing.T, ob Outbox, attemptsMax int, clock func() time.Time) (*Store, *sql.DB) {
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
	var n int
	s, err := New(Options{
		DB:          conn,
		NewID:       func() string { n++; return fmt.Sprintf("run-%02d", n) },
		Now:         clock,
		RandFloat:   func() float64 { return 0 }, // jitter factor = 2.0, deterministic
		AttemptsMax: attemptsMax,
		Outbox:      ob,
	})
	if err != nil {
		t.Fatalf("new store: %v", err)
	}
	return s, conn
}

func ineligOf(t *testing.T, conn *sql.DB, id string) sql.NullInt64 {
	t.Helper()
	var v sql.NullInt64
	if err := conn.QueryRow(`SELECT ineligible_until FROM inbox WHERE id=?`, id).Scan(&v); err != nil {
		t.Fatalf("ineligible_until: %v", err)
	}
	return v
}

func deadOf(t *testing.T, conn *sql.DB, id string) sql.NullInt64 {
	t.Helper()
	var v sql.NullInt64
	if err := conn.QueryRow(`SELECT dead_at FROM inbox WHERE id=?`, id).Scan(&v); err != nil {
		t.Fatalf("dead_at: %v", err)
	}
	return v
}

// TestFailArmsBackoff — a single failure (below threshold) sets ineligible_until
// and leaves the row not dead, not stamped (design §7 backoff on every failure).
func TestFailArmsBackoff(t *testing.T) {
	clock := func() time.Time { return time.Unix(1000, 0).UTC() }
	s, conn := policyStore(t, nil, 5, clock)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")

	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), id, "doc-1", errors.New("boom")); err != nil {
		t.Fatalf("fail: %v", err)
	}

	inelig := ineligOf(t, conn, "doc-1")
	if !inelig.Valid {
		t.Fatal("backoff not armed")
	}
	// failures=1 ⇒ exp 2^0=1; jitter 2.0; avg floored at 60s ⇒ now + 120s.
	wantMs := clock().Add(120 * time.Second).UnixMilli()
	if inelig.Int64 != wantMs {
		t.Fatalf("ineligible_until = %d, want %d (now + 2×60s)", inelig.Int64, wantMs)
	}
	if deadOf(t, conn, "doc-1").Valid {
		t.Fatal("row dead-lettered below threshold")
	}
	if stampOf(t, conn, "doc-1") != "" {
		t.Fatal("failed row was stamped")
	}
}

// TestBackoffGrowsExponentially — the second failure's delay doubles (2^(n−1)).
func TestBackoffGrowsExponentially(t *testing.T) {
	clock := func() time.Time { return time.Unix(1000, 0).UTC() }
	s, conn := policyStore(t, nil, 5, clock)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")

	r1, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), r1, "doc-1", errors.New("e1")); err != nil {
		t.Fatalf("fail 1: %v", err)
	}
	r2, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), r2, "doc-1", errors.New("e2")); err != nil {
		t.Fatalf("fail 2: %v", err)
	}

	// failures=2 ⇒ exp 2^1=2; jitter 2.0; avg 60s ⇒ now + 240s.
	want := clock().Add(240 * time.Second).UnixMilli()
	if got := ineligOf(t, conn, "doc-1").Int64; got != want {
		t.Fatalf("second backoff = %d, want %d (doubled)", got, want)
	}
}

// TestDeadLetterAtThreshold — at WIKI_RUN_ATTEMPTS_MAX the row dead-letters: dead_at
// set, ineligible_until cleared in the same UPDATE, and wiki.row_dead_lettered
// emitted with the §12.3 payload + the doorbell rung.
func TestDeadLetterAtThreshold(t *testing.T) {
	clock := func() time.Time { return time.Unix(2000, 0).UTC() }
	ob := &fakeOutbox{}
	s, conn := policyStore(t, ob, 3, clock)
	// title + source carried into the payload.
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by, title)
		 VALUES ('doc-1','u@x','document','mcp:thing','sha',1,1,'','My Doc')`)
	if err != nil {
		t.Fatalf("insert: %v", err)
	}

	var lastErr string
	for i := 0; i < 3; i++ {
		id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
		lastErr = fmt.Sprintf("boom-%d", i)
		if err := s.Fail(context.Background(), id, "doc-1", errors.New(lastErr)); err != nil {
			t.Fatalf("fail %d: %v", i, err)
		}
	}

	if !deadOf(t, conn, "doc-1").Valid {
		t.Fatal("row not dead-lettered at threshold")
	}
	if ineligOf(t, conn, "doc-1").Valid {
		t.Fatal("ineligible_until not cleared on dead-letter")
	}
	if len(ob.events) != 1 {
		t.Fatalf("emitted %d events, want exactly 1 wiki.row_dead_lettered", len(ob.events))
	}
	if ob.events[0].Type != EventRowDeadLettered {
		t.Fatalf("event type = %q, want %q", ob.events[0].Type, EventRowDeadLettered)
	}
	var p rowDeadLetteredPayload
	if err := json.Unmarshal(ob.events[0].Payload, &p); err != nil {
		t.Fatalf("payload: %v", err)
	}
	if p.InboxID != "doc-1" || p.Source != "mcp:thing" || p.Title != "My Doc" || p.LastError != lastErr {
		t.Fatalf("payload = %+v, want inbox_id/source/title/last_error populated", p)
	}
	if ob.rings != 1 {
		t.Fatalf("rings = %d, want 1 (doorbell after dead-letter commit)", ob.rings)
	}
}

// TestDeadLetterEventIsAtomic — if the outbox Append fails, the whole dead-letter
// transaction rolls back: dead_at is NOT set (event + mark commit together — §8).
func TestDeadLetterEventIsAtomic(t *testing.T) {
	clock := func() time.Time { return time.Unix(2000, 0).UTC() }
	ob := &fakeOutbox{failTx: true}
	s, conn := policyStore(t, ob, 1, clock)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")

	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), id, "doc-1", errors.New("boom")); err == nil {
		t.Fatal("expected dead-letter to fail when the outbox append fails")
	}
	if deadOf(t, conn, "doc-1").Valid {
		t.Fatal("dead_at set despite the event failing — not atomic")
	}
}

// TestDeadLetterIdempotent — a second policy pass on an already-dead row neither
// re-marks nor re-emits.
func TestDeadLetterIdempotent(t *testing.T) {
	clock := func() time.Time { return time.Unix(2000, 0).UTC() }
	ob := &fakeOutbox{}
	s, conn := policyStore(t, ob, 1, clock)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")

	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), id, "doc-1", errors.New("boom")); err != nil {
		t.Fatalf("fail: %v", err)
	}
	// Another failed attempt against the already-dead row.
	id2, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), id2, "doc-1", errors.New("boom2")); err != nil {
		t.Fatalf("fail 2: %v", err)
	}
	if len(ob.events) != 1 {
		t.Fatalf("emitted %d events, want 1 (no double-emit on already-dead row)", len(ob.events))
	}
}

// TestRequeueGrantsFreshBudget — re-queue clears dead_at + backoff, stamps
// requeued_at, and the retry counter then ignores the pre-requeue attempts, so a
// freshly-requeued row backs off again instead of immediately re-dead-lettering.
func TestRequeueGrantsFreshBudget(t *testing.T) {
	clock := func() time.Time { return time.Unix(3000, 0).UTC() }
	ob := &fakeOutbox{}
	s, conn := policyStore(t, ob, 2, clock)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")

	// Two failures → dead-lettered (threshold 2).
	for i := 0; i < 2; i++ {
		id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
		if err := s.Fail(context.Background(), id, "doc-1", errors.New("boom")); err != nil {
			t.Fatalf("fail %d: %v", i, err)
		}
	}
	if !deadOf(t, conn, "doc-1").Valid {
		t.Fatal("not dead-lettered after threshold")
	}

	// Human re-queues: fresh budget.
	if err := s.Requeue(context.Background(), "doc-1"); err != nil {
		t.Fatalf("requeue: %v", err)
	}
	if deadOf(t, conn, "doc-1").Valid {
		t.Fatal("dead_at not cleared by requeue")
	}
	var rq sql.NullInt64
	conn.QueryRow(`SELECT requeued_at FROM inbox WHERE id='doc-1'`).Scan(&rq)
	if !rq.Valid {
		t.Fatal("requeued_at not stamped")
	}

	// One more failure after requeue: counter is 1 (< threshold 2) → backoff, not dead.
	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), id, "doc-1", errors.New("boom")); err != nil {
		t.Fatalf("post-requeue fail: %v", err)
	}
	if deadOf(t, conn, "doc-1").Valid {
		t.Fatal("re-dead-lettered immediately — requeue did not grant a fresh budget")
	}
	if !ineligOf(t, conn, "doc-1").Valid {
		t.Fatal("post-requeue failure did not arm backoff")
	}
}

// TestCrashedCountsAsAttempt — the boot sweep marks an orphan crashed AND applies
// the policy, so a crash counts one attempt toward the threshold (design §7: the
// sweep does the identical thing Fail does).
func TestCrashedCountsAsAttempt(t *testing.T) {
	clock := func() time.Time { return time.Unix(4000, 0).UTC() }
	s, conn := policyStore(t, nil, 5, clock)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")

	// An orphaned running run (a crashed attempt).
	if _, err := conn.Exec(
		`INSERT INTO runs (id, job, caused_by, status, started_at)
		 VALUES ('orphan-1','document-pass','doc-1','running', 1)`); err != nil {
		t.Fatalf("insert orphan: %v", err)
	}
	n, err := s.SweepOrphans(context.Background())
	if err != nil {
		t.Fatalf("sweep: %v", err)
	}
	if n != 1 {
		t.Fatalf("swept %d, want 1", n)
	}
	if statusOf(t, conn, "orphan-1") != StatusCrashed {
		t.Fatal("orphan not crashed")
	}
	// The crash applied backoff (one attempt counted).
	if !ineligOf(t, conn, "doc-1").Valid {
		t.Fatal("boot sweep did not apply the failure policy to the crashed row")
	}
}

// TestAllFailuresCountIncludingCrashed — failed and crashed runs both count toward
// the threshold (no exempt type — design §7).
func TestAllFailuresCountIncludingCrashed(t *testing.T) {
	clock := func() time.Time { return time.Unix(5000, 0).UTC() }
	ob := &fakeOutbox{}
	s, conn := policyStore(t, ob, 2, clock)
	insertInbox(t, conn, "doc-1", "document", "mcp:x")

	// Attempt 1: a crash (boot sweep).
	if _, err := conn.Exec(
		`INSERT INTO runs (id, job, caused_by, status, started_at)
		 VALUES ('orphan-1','document-pass','doc-1','running', 1)`); err != nil {
		t.Fatalf("insert orphan: %v", err)
	}
	if _, err := s.SweepOrphans(context.Background()); err != nil {
		t.Fatalf("sweep: %v", err)
	}
	if deadOf(t, conn, "doc-1").Valid {
		t.Fatal("dead after a single crash with threshold 2")
	}

	// Attempt 2: a clean failure → reaches threshold 2 (crash + fail) → dead-lettered.
	id, _ := s.Begin(context.Background(), "document-pass", "doc-1")
	if err := s.Fail(context.Background(), id, "doc-1", errors.New("boom")); err != nil {
		t.Fatalf("fail: %v", err)
	}
	if !deadOf(t, conn, "doc-1").Valid {
		t.Fatal("crash + fail did not reach threshold 2 — a failure type was exempt")
	}
}
