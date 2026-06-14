package worker

import (
	"context"
	"database/sql"
	"fmt"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"wiki/internal/db"
	"wiki/internal/integrate"
	"wiki/internal/run"

	_ "modernc.org/sqlite"
)

// newDB stands up a migrated in-temp-dir SQLite DB.
func newDB(t *testing.T) *sql.DB {
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

func newRuns(t *testing.T, conn *sql.DB) *run.Store {
	t.Helper()
	var n int64
	var mu sync.Mutex
	s, err := run.New(run.Options{
		DB: conn,
		NewID: func() string {
			mu.Lock()
			defer mu.Unlock()
			n++
			return fmt.Sprintf("run-%04d", n)
		},
	})
	if err != nil {
		t.Fatalf("run.New: %v", err)
	}
	return s
}

func insertDoc(t *testing.T, conn *sql.DB, id string) {
	t.Helper()
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by)
		 VALUES (?, 'u@x', 'document', 'mcp:x', 'sha', 1, 1, '')`, id)
	if err != nil {
		t.Fatalf("insert doc: %v", err)
	}
}

func insertCron(t *testing.T, conn *sql.DB, id, schedule string) {
	t.Helper()
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by)
		 VALUES (?, 'system@ikigenba', 'event', ?, 'sha', 1, 1, '')`, id, "cron:"+schedule)
	if err != nil {
		t.Fatalf("insert cron: %v", err)
	}
}

func insertEvent(t *testing.T, conn *sql.DB, id string) {
	t.Helper()
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by)
		 VALUES (?, 'system@ikigenba', 'event', 'crm:contact.created', 'sha', 1, 1, '')`, id)
	if err != nil {
		t.Fatalf("insert event: %v", err)
	}
}

func pendingCount(t *testing.T, conn *sql.DB) int {
	t.Helper()
	var n int
	if err := conn.QueryRow(`SELECT COUNT(1) FROM inbox WHERE integrated_by=''`).Scan(&n); err != nil {
		t.Fatalf("pending count: %v", err)
	}
	return n
}

// recordingIntegrator wraps a stub and records every Integrate call so claim-once
// and ordering can be asserted. order, if set, records the units in call order.
type recordingIntegrator struct {
	integrate.Integrator
	mu    sync.Mutex
	calls map[string]int
	order *[]string
	gate  chan struct{} // if non-nil, blocks each Integrate until closed
}

func newRecording(inner integrate.Integrator) *recordingIntegrator {
	return &recordingIntegrator{Integrator: inner, calls: map[string]int{}}
}

func (r *recordingIntegrator) Integrate(ctx context.Context, u integrate.Unit) (*integrate.Manifest, error) {
	r.mu.Lock()
	key := u.CausedBy
	if u.Entry != "" {
		key += "/" + u.Entry
	}
	r.calls[key]++
	if r.order != nil {
		*r.order = append(*r.order, key)
	}
	gate := r.gate
	r.mu.Unlock()
	if gate != nil {
		<-gate
	}
	return r.Integrator.Integrate(ctx, u)
}

func (r *recordingIntegrator) maxCalls() int {
	r.mu.Lock()
	defer r.mu.Unlock()
	m := 0
	for _, c := range r.calls {
		if c > m {
			m = c
		}
	}
	return m
}

// runUntilDrained runs the pool until every inbox row is integrated (or timeout),
// then cancels and waits.
func runUntilDrained(t *testing.T, conn *sql.DB, p *Pool) {
	t.Helper()
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- p.Run(ctx) }()
	deadline := time.After(5 * time.Second)
	for pendingCount(t, conn) > 0 {
		p.Nudge()
		select {
		case <-deadline:
			cancel()
			<-done
			t.Fatalf("timed out with %d pending", pendingCount(t, conn))
		case <-time.After(5 * time.Millisecond):
		}
	}
	cancel()
	if err := <-done; err != nil {
		t.Fatalf("pool run: %v", err)
	}
}

// TestClaimOnceUnderNWorkers — many document rows, N workers, every row claimed
// EXACTLY once (no double-claim).
func TestClaimOnceUnderNWorkers(t *testing.T) {
	conn := newDB(t)
	for i := 0; i < 50; i++ {
		insertDoc(t, conn, fmt.Sprintf("doc-%03d", i))
	}
	rec := newRecording(integrate.NewDocumentStub())
	p, err := New(Options{DB: conn, Runs: newRuns(t, conn), Workers: 8, Document: rec})
	if err != nil {
		t.Fatalf("new pool: %v", err)
	}
	runUntilDrained(t, conn, p)

	if pendingCount(t, conn) != 0 {
		t.Fatalf("pending rows remain: %d", pendingCount(t, conn))
	}
	if got := rec.maxCalls(); got != 1 {
		t.Fatalf("a row was integrated %d times; want exactly once", got)
	}
	// One succeeded run per row.
	var succeeded int
	conn.QueryRow(`SELECT COUNT(1) FROM runs WHERE status='succeeded'`).Scan(&succeeded)
	if succeeded != 50 {
		t.Fatalf("succeeded runs = %d, want 50", succeeded)
	}
}

// TestCronBeforeDocument — with both a cron row and a document pending and a
// single worker gated so only one unit is in flight at a time, the cron entry is
// selected first.
func TestCronBeforeDocument(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	insertCron(t, conn, "cron-1", "daily")

	var order []string
	docRec := newRecording(integrate.NewDocumentStub())
	docRec.order = &order
	cronRec := newRecording(integrate.NewCronStub("crm-digest"))
	cronRec.order = &order

	p, err := New(Options{
		DB: conn, Runs: newRuns(t, conn), Workers: 1, Document: docRec,
		Cron:     map[string]integrate.Integrator{"crm-digest": cronRec},
		Bindings: map[string][]string{"daily": {"crm-digest"}},
	})
	if err != nil {
		t.Fatalf("new pool: %v", err)
	}
	runUntilDrained(t, conn, p)

	if len(order) != 2 {
		t.Fatalf("calls = %v, want 2", order)
	}
	if order[0] != "cron-1/crm-digest" {
		t.Fatalf("first selected %q, want the cron entry first", order[0])
	}
}

// TestEventRowsInvisible — a plain (non-cron) event row is never selected; it
// stays pending after the pool drains the documents.
func TestEventRowsInvisible(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	insertEvent(t, conn, "evt-1")

	rec := newRecording(integrate.NewDocumentStub())
	p, err := New(Options{DB: conn, Runs: newRuns(t, conn), Workers: 2, Document: rec})
	if err != nil {
		t.Fatalf("new pool: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- p.Run(ctx) }()
	// Drain the one document.
	deadline := time.After(5 * time.Second)
	for {
		var by string
		conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&by)
		if by != "" {
			break
		}
		p.Nudge()
		select {
		case <-deadline:
			cancel()
			<-done
			t.Fatal("document never integrated")
		case <-time.After(5 * time.Millisecond):
		}
	}
	cancel()
	<-done

	// The event row remains pending (invisible to selection).
	var by string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='evt-1'`).Scan(&by)
	if by != "" {
		t.Fatalf("event row was integrated (%q); it must be invisible to selection", by)
	}
}

// TestFailureLeavesRowPending — a stub told to fail leaves its row pending, and a
// restart with a non-failing integrator re-selects and completes it. This is the
// crash/fail-resume property: a crash between claim and commit leaves the row
// pending and restart re-selects it.
func TestFailureLeavesRowPending(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	runs := newRuns(t, conn)

	// First pool: integrator always fails.
	failing := integrate.NewDocumentStub().FailWith(integrate.ErrStubFailure)
	p1, _ := New(Options{DB: conn, Runs: runs, Workers: 1, Document: failing})
	ctx1, cancel1 := context.WithCancel(context.Background())
	done1 := make(chan error, 1)
	go func() { done1 <- p1.Run(ctx1) }()
	// Let it attempt + fail a few times.
	for i := 0; i < 5; i++ {
		p1.Nudge()
		time.Sleep(5 * time.Millisecond)
	}
	cancel1()
	<-done1

	// Row is still pending; a failed run was recorded.
	var by string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&by)
	if by != "" {
		t.Fatalf("failed run stamped the row (%q); it must stay pending", by)
	}
	var failed int
	conn.QueryRow(`SELECT COUNT(1) FROM runs WHERE status='failed'`).Scan(&failed)
	if failed == 0 {
		t.Fatal("no failed run recorded")
	}

	// The failed row is now backed off (ineligible_until set — P5 §7), so it is no
	// longer immediately pending; that is the bounded-retry policy working.
	var inelig sql.NullInt64
	conn.QueryRow(`SELECT ineligible_until FROM inbox WHERE id='doc-1'`).Scan(&inelig)
	if !inelig.Valid {
		t.Fatal("failed row has no ineligible_until backoff set")
	}

	// Second pool with a clock far past the backoff: the healthy integrator
	// re-selects the now-eligible pending row and completes it.
	future := func() time.Time { return time.UnixMilli(inelig.Int64).Add(time.Hour) }
	p2, _ := New(Options{DB: conn, Runs: runs, Workers: 1, Document: integrate.NewDocumentStub(), Now: future})
	runUntilDrained(t, conn, p2)
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&by)
	if by == "" {
		t.Fatal("row not integrated after restart with healthy integrator")
	}
}

// TestBootSweepMarksOrphans — a `running` row left from a "crashed" process is
// flipped to crashed by the boot sweep when the pool starts.
func TestBootSweepMarksOrphans(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	// Simulate a crash: a leftover `running` run row, in-flight set gone.
	_, err := conn.Exec(
		`INSERT INTO runs (id, job, caused_by, status, started_at)
		 VALUES ('orphan-1', 'document-pass', 'doc-1', 'running', 1)`)
	if err != nil {
		t.Fatalf("insert orphan: %v", err)
	}

	// A clock far in the future so the boot sweep's backoff on the crashed orphan's
	// row (P5 §7 — the sweep applies the policy when marking crashed) has expired and
	// the healthy integrator can re-select and drain the still-pending row.
	future := func() time.Time { return time.Date(3000, 1, 1, 0, 0, 0, 0, time.UTC) }
	p, _ := New(Options{DB: conn, Runs: newRuns(t, conn), Workers: 1, Document: integrate.NewDocumentStub(), Now: future})
	runUntilDrained(t, conn, p)

	var st string
	conn.QueryRow(`SELECT status FROM runs WHERE id='orphan-1'`).Scan(&st)
	if st != run.StatusCrashed {
		t.Fatalf("orphan status = %q, want crashed", st)
	}
}

// TestOldestFirst — with a single gated worker, documents are selected oldest-id
// first (ULIDs sort lexicographically by time, so id order is arrival order).
func TestOldestFirst(t *testing.T) {
	conn := newDB(t)
	// Insert out of order; selection must still take them ascending by id.
	insertDoc(t, conn, "doc-300")
	insertDoc(t, conn, "doc-100")
	insertDoc(t, conn, "doc-200")

	var order []string
	rec := newRecording(integrate.NewDocumentStub())
	rec.order = &order
	p, _ := New(Options{DB: conn, Runs: newRuns(t, conn), Workers: 1, Document: rec})
	runUntilDrained(t, conn, p)

	want := []string{"doc-100", "doc-200", "doc-300"}
	if len(order) != 3 {
		t.Fatalf("order = %v, want 3", order)
	}
	for i := range want {
		if order[i] != want[i] {
			t.Fatalf("order = %v, want oldest-first %v", order, want)
		}
	}
}

// TestCronNoBindingStampedAsNoop — a cron row whose schedule no job binds is
// stamped immediately as a no-op (design §3), without running an integrator.
func TestCronNoBindingStampedAsNoop(t *testing.T) {
	conn := newDB(t)
	insertCron(t, conn, "cron-1", "orphan-schedule")

	p, _ := New(Options{
		DB: conn, Runs: newRuns(t, conn), Workers: 1,
		Document: integrate.NewDocumentStub(),
		// no bindings for "orphan-schedule"
	})
	runUntilDrained(t, conn, p)

	var by string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='cron-1'`).Scan(&by)
	if by == "" {
		t.Fatal("no-binding cron row not stamped as no-op")
	}
}

// TestBackedOffRowNotSelected — a pending row whose ineligible_until is in the
// future is invisible to selection (the pending predicate, design §7), so a pool
// whose clock predates the backoff never integrates it.
func TestBackedOffRowNotSelected(t *testing.T) {
	conn := newDB(t)
	// A pending row backed off until epoch-ms 10_000_000.
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by, ineligible_until)
		 VALUES ('doc-1','u@x','document','mcp:x','sha',1,1,'', 10000000)`)
	if err != nil {
		t.Fatalf("insert: %v", err)
	}
	now := func() time.Time { return time.UnixMilli(5_000_000).UTC() } // before the backoff
	p, _ := New(Options{DB: conn, Runs: newRuns(t, conn), Workers: 1, Document: integrate.NewDocumentStub(), Now: now})

	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- p.Run(ctx) }()
	for i := 0; i < 10; i++ {
		p.Nudge()
		time.Sleep(2 * time.Millisecond)
	}
	cancel()
	<-done

	var by string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&by)
	if by != "" {
		t.Fatalf("backed-off row was selected (stamped %q) before its ineligible_until", by)
	}
}

// TestDeadRowNotSelected — a dead-lettered row (dead_at set) is never selected.
func TestDeadRowNotSelected(t *testing.T) {
	conn := newDB(t)
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by, dead_at)
		 VALUES ('doc-1','u@x','document','mcp:x','sha',1,1,'', 123)`)
	if err != nil {
		t.Fatalf("insert: %v", err)
	}
	p, _ := New(Options{DB: conn, Runs: newRuns(t, conn), Workers: 1, Document: integrate.NewDocumentStub()})

	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- p.Run(ctx) }()
	for i := 0; i < 10; i++ {
		p.Nudge()
		time.Sleep(2 * time.Millisecond)
	}
	cancel()
	<-done

	var by string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&by)
	if by != "" {
		t.Fatalf("dead-lettered row was selected (stamped %q)", by)
	}
}

// TestTimerWakeSelectsExpiredBackoff — the backoff timer wake source re-selects a
// row once the clock advances past its ineligible_until, with no nudge. We move
// the pool clock forward; the timer (armed to the earliest future
// ineligible_until) wakes selection and the row drains.
func TestTimerWakeSelectsExpiredBackoff(t *testing.T) {
	conn := newDB(t)
	_, err := conn.Exec(
		`INSERT INTO inbox (id, owner, kind, source, sha256, size, received_at, integrated_by, ineligible_until)
		 VALUES ('doc-1','u@x','document','mcp:x','sha',1,1,'', 1000)`)
	if err != nil {
		t.Fatalf("insert: %v", err)
	}
	// Clock starts before the backoff, then advances past it.
	var mu sync.Mutex
	cur := time.UnixMilli(0).UTC()
	now := func() time.Time { mu.Lock(); defer mu.Unlock(); return cur }
	advance := func(to time.Time) { mu.Lock(); cur = to; mu.Unlock() }

	p, _ := New(Options{DB: conn, Runs: newRuns(t, conn), Workers: 1, Document: integrate.NewDocumentStub(), Now: now})
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := make(chan error, 1)
	go func() { done <- p.Run(ctx) }()

	// Advance past the backoff; the row becomes eligible. Nudge (a legitimate wake
	// source) drives re-selection deterministically without sleeping for the timer.
	advance(time.UnixMilli(2000).UTC())
	deadline := time.After(3 * time.Second)
	for {
		p.Nudge()
		var by string
		conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&by)
		if by != "" {
			break
		}
		select {
		case <-deadline:
			cancel()
			<-done
			t.Fatal("expired-backoff row never re-selected after the clock advanced")
		case <-time.After(5 * time.Millisecond):
		}
	}
	cancel()
	<-done
}
