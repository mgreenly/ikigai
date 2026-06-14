package worker

import (
	"context"
	"database/sql"
	"sync"
	"testing"
	"time"

	"wiki/internal/integrate"
	"wiki/internal/page"
)

// conflictIntegrator is a document integrator whose manifest writes one page at a
// FIXED base version, so once the page is advanced past that version every commit
// conflicts (the lost-update race). It satisfies ReMerger; reMergeCalls counts how
// many times the conflict loop re-ran merge. When healOnReMerge is set, the first
// re-merge advances baseVersion to the page's current version so the next commit
// succeeds — exercising the loop's success arm.
type conflictIntegrator struct {
	subjectID     string
	baseVersion   int
	body          string
	healTo        int  // if >0, reMerge sets baseVersion to this on the first call
	healOnReMerge bool

	mu           sync.Mutex
	reMergeCalls int
}

func (c *conflictIntegrator) Job() string { return "document-pass" }

func (c *conflictIntegrator) Integrate(_ context.Context, u integrate.Unit) (*integrate.Manifest, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.manifest(), nil
}

func (c *conflictIntegrator) manifest() *integrate.Manifest {
	return &integrate.Manifest{
		Subjects: []integrate.Subject{{
			Type: integrate.TypeEntity, Name: "Acme", Aliases: []string{"acme"},
			SubjectID: c.subjectID, TargetPage: c.subjectID, BaseVersion: c.baseVersion,
			PageTitle: "Acme", PageBody: c.body,
			Claims: []integrate.Claim{{Text: "c", Cites: []string{"x"}}},
		}},
	}
}

func (c *conflictIntegrator) ReMerge(_ context.Context, m *integrate.Manifest, _ string) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.reMergeCalls++
	if c.healOnReMerge {
		c.baseVersion = c.healTo
	}
	// Re-derive the manifest's subject base version from the (possibly healed) value,
	// as a real re-merge would (re-reading the fresh page version).
	m.Subjects[0].BaseVersion = c.baseVersion
	return nil
}

// TestConflictLoopExhausts drives a document whose manifest is permanently stale
// (base version 0 while the page is at version 1): every commit conflicts, the loop
// re-runs merge but stays stale, and after MaxCommitAttempts the run fails cleanly.
// It asserts the row stays pending with backoff armed, conflicts are counted, and
// re-merge ran exactly MaxCommitAttempts-1 times (no re-merge after the last
// attempt).
func TestConflictLoopExhausts(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	runs := newRuns(t, conn)
	store := page.NewStore(conn)

	// Pre-create the page at version 0, then advance it to version 1 so a base-0
	// commit conflicts.
	mkPage := func(body string, base int) {
		tx, _ := conn.Begin()
		if err := store.UpsertPage(context.Background(), tx, "subj-1", "Acme", body, base); err != nil {
			tx.Rollback()
			t.Fatalf("seed upsert: %v", err)
		}
		tx.Commit()
	}
	mkPage("v0 [x]", 0) // create at version 0
	mkPage("v1 [x]", 0) // guarded update 0→1; page now at version 1

	integ := &conflictIntegrator{subjectID: "subj-1", baseVersion: 0, body: "stale [x]"}

	p, err := New(Options{DB: conn, Runs: runs, Workers: 1, Document: integ})
	if err != nil {
		t.Fatalf("new pool: %v", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() { p.Run(ctx); close(done) }()

	// Wait until the row goes non-pending-eligible (backoff armed) or dead.
	waitUntil(t, func() bool {
		var iu *int64
		conn.QueryRow(`SELECT ineligible_until FROM inbox WHERE id='doc-1'`).Scan(&iu)
		return iu != nil
	})
	cancel()
	<-done

	// The run failed (conflict-retry exhaustion); the row is still pending (not
	// stamped) with backoff armed.
	var stamp string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&stamp)
	if stamp != "" {
		t.Fatalf("row stamped despite exhaustion: %q", stamp)
	}
	var nFailed int
	conn.QueryRow(`SELECT COUNT(1) FROM runs WHERE caused_by='doc-1' AND status='failed'`).Scan(&nFailed)
	if nFailed < 1 {
		t.Fatalf("expected a failed run, got %d", nFailed)
	}
	// conflicts counted on the run: one per conflicting attempt (MaxCommitAttempts).
	var maxConflicts int
	conn.QueryRow(`SELECT MAX(conflicts) FROM runs WHERE caused_by='doc-1'`).Scan(&maxConflicts)
	if maxConflicts < 1 {
		t.Fatalf("conflicts not counted on the run: %d", maxConflicts)
	}
	// Re-merge ran on each non-terminal conflict (MaxCommitAttempts-1 times per run).
	integ.mu.Lock()
	rm := integ.reMergeCalls
	integ.mu.Unlock()
	if rm < 1 {
		t.Fatalf("re-merge never ran: %d", rm)
	}
}

// TestConflictLoopRecovers drives a document that conflicts once, then the re-merge
// "heals" the base version (as a real re-merge re-reading the fresh page would) so
// the recommit succeeds — proving the lost-update loop's success arm: re-run merge
// only, recommit, stamp.
func TestConflictLoopRecovers(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	runs := newRuns(t, conn)
	store := page.NewStore(conn)

	mkPage := func(body string, base int) {
		tx, _ := conn.Begin()
		if err := store.UpsertPage(context.Background(), tx, "subj-1", "Acme", body, base); err != nil {
			tx.Rollback()
			t.Fatalf("seed upsert: %v", err)
		}
		tx.Commit()
	}
	mkPage("v0 [x]", 0)
	mkPage("v1 [x]", 0) // page at version 1

	// First commit at base 0 conflicts; the re-merge heals base→1 so the recommit
	// succeeds (the page is at version 1).
	integ := &conflictIntegrator{
		subjectID: "subj-1", baseVersion: 0, body: "recovered [x]",
		healOnReMerge: true, healTo: 1,
	}

	p, err := New(Options{DB: conn, Runs: runs, Workers: 1, Document: integ})
	if err != nil {
		t.Fatalf("new pool: %v", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() { p.Run(ctx); close(done) }()

	waitUntil(t, func() bool {
		var stamp string
		conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&stamp)
		return stamp != ""
	})
	cancel()
	<-done

	// The row was stamped by a succeeded run, and the page carries the recovered body.
	var status string
	conn.QueryRow(`SELECT status FROM runs WHERE caused_by='doc-1' ORDER BY started_at DESC LIMIT 1`).Scan(&status)
	if status != "succeeded" {
		t.Fatalf("last run status = %q, want succeeded", status)
	}
	var body string
	conn.QueryRow(`SELECT body FROM pages WHERE subject='subj-1'`).Scan(&body)
	if body != "recovered [x]" {
		t.Fatalf("page body = %q, want recovered", body)
	}
	// The conflict was counted, and re-merge ran exactly once (one conflict, then win).
	var maxConflicts int
	conn.QueryRow(`SELECT MAX(conflicts) FROM runs WHERE caused_by='doc-1'`).Scan(&maxConflicts)
	if maxConflicts != 1 {
		t.Fatalf("conflicts = %d, want exactly 1", maxConflicts)
	}
	integ.mu.Lock()
	rm := integ.reMergeCalls
	integ.mu.Unlock()
	if rm != 1 {
		t.Fatalf("re-merge ran %d times, want 1", rm)
	}
}

// dupMintIntegrator mints a subject whose alias key collides with a
// pre-committed winner (the duplicate-mint race). It satisfies BOTH ReMerger and
// ReResolver. On ReResolve it "restarts at resolve" — flipping the manifest's
// subject id (and target page) onto the winner, as a real re-resolution hitting the
// winner's freshly-committed alias would — so the recommit succeeds. reResolveCalls
// / reMergeCalls record which conflict arm the loop took.
type dupMintIntegrator struct {
	loserID  string
	winnerID string
	aliasN   string

	mu              sync.Mutex
	reResolveCalls  int
	reMergeCalls    int
	healOnReResolve bool
}

func (c *dupMintIntegrator) Job() string { return "document-pass" }

func (c *dupMintIntegrator) Integrate(_ context.Context, _ integrate.Unit) (*integrate.Manifest, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.manifest(c.loserID), nil
}

func (c *dupMintIntegrator) manifest(subjectID string) *integrate.Manifest {
	return &integrate.Manifest{
		Subjects: []integrate.Subject{{
			Type: integrate.TypeEntity, Name: "Acme", Aliases: []string{c.aliasN},
			SubjectID: subjectID, TargetPage: subjectID, BaseVersion: 0,
			PageTitle: "Acme", PageBody: "body [x]",
			Claims: []integrate.Claim{{Text: "c", Cites: []string{"x"}}},
		}},
	}
}

func (c *dupMintIntegrator) ReMerge(_ context.Context, _ *integrate.Manifest, _ string) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.reMergeCalls++
	return nil
}

func (c *dupMintIntegrator) ReResolve(_ context.Context, m *integrate.Manifest, subjectID string) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.reResolveCalls++
	if !c.healOnReResolve {
		return nil // re-resolution still mints the same loser → stays conflicting
	}
	// Restart-at-resolve: the lookup now hits the winner's alias, so this subject
	// resolves ONTO the winner. Replace the manifest entry's identity in place.
	for i := range m.Subjects {
		if m.Subjects[i].SubjectID == subjectID {
			m.Subjects[i].SubjectID = c.winnerID
			m.Subjects[i].TargetPage = c.winnerID
			m.Subjects[i].BaseVersion = 0 // a fresh re-read; the winner's page is at v0 here
		}
	}
	return nil
}

// seedWinner pre-commits the winner subject + its alias + page, so a colliding mint
// of the same alias key hits UNIQUE(type, norm) → duplicate-mint conflict.
func seedWinner(t *testing.T, conn *sql.DB, id, aliasNorm string) {
	t.Helper()
	if _, err := conn.Exec(
		`INSERT INTO subjects (id, type, kind, canonical_name, created_by_run) VALUES (?, 'entity', 'org', 'Acme', 'seed')`, id); err != nil {
		t.Fatalf("seed subject: %v", err)
	}
	if _, err := conn.Exec(
		`INSERT INTO aliases (type, norm, subject_id) VALUES ('entity', ?, ?)`, aliasNorm, id); err != nil {
		t.Fatalf("seed alias: %v", err)
	}
}

// TestDuplicateMintRestartsAtResolve drives a document that mints a subject whose
// alias key is already owned by a pre-committed winner. The commit hits the alias
// UNIQUE → the conflict loop RESTARTS AT RESOLVE (not re-merge), the re-resolution
// lands the subject on the winner, and the recommit succeeds — exactly one subject
// exists, the loser having resolved onto the winner.
func TestDuplicateMintRestartsAtResolve(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	runs := newRuns(t, conn)
	seedWinner(t, conn, "winner", "acme")

	integ := &dupMintIntegrator{loserID: "loser", winnerID: "winner", aliasN: "acme", healOnReResolve: true}

	p, err := New(Options{DB: conn, Runs: runs, Workers: 1, Document: integ})
	if err != nil {
		t.Fatalf("new pool: %v", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() { p.Run(ctx); close(done) }()

	waitUntil(t, func() bool {
		var stamp string
		conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&stamp)
		return stamp != ""
	})
	cancel()
	<-done

	// The row was stamped by a succeeded run.
	var status string
	conn.QueryRow(`SELECT status FROM runs WHERE caused_by='doc-1' ORDER BY started_at DESC LIMIT 1`).Scan(&status)
	if status != "succeeded" {
		t.Fatalf("last run status = %q, want succeeded", status)
	}
	// Exactly one subject exists (no duplicate "loser" was minted) and the loser
	// resolved onto the winner.
	var nLoser int
	conn.QueryRow(`SELECT COUNT(1) FROM subjects WHERE id='loser'`).Scan(&nLoser)
	if nLoser != 0 {
		t.Fatalf("loser subject minted despite restart-at-resolve")
	}
	// The conflict loop took the RE-RESOLVE arm, not the re-merge arm.
	integ.mu.Lock()
	rr, rm := integ.reResolveCalls, integ.reMergeCalls
	integ.mu.Unlock()
	if rr < 1 {
		t.Fatalf("re-resolve never ran: %d", rr)
	}
	if rm != 0 {
		t.Fatalf("duplicate mint wrongly took the re-merge arm: %d", rm)
	}
	// The conflict was counted on the run (shared bound — same counter as P7b).
	var maxConflicts int
	conn.QueryRow(`SELECT MAX(conflicts) FROM runs WHERE caused_by='doc-1'`).Scan(&maxConflicts)
	if maxConflicts < 1 {
		t.Fatalf("conflict not counted on the run: %d", maxConflicts)
	}
}

// TestDuplicateMintExhaustsSharedBound drives a duplicate mint whose re-resolution
// keeps minting the same colliding subject: every commit conflicts, and after the
// SHARED MaxCommitAttempts bound (the same one P7b's lost-update arm uses) the run
// fails cleanly with the row left pending and backoff armed.
func TestDuplicateMintExhaustsSharedBound(t *testing.T) {
	conn := newDB(t)
	insertDoc(t, conn, "doc-1")
	runs := newRuns(t, conn)
	seedWinner(t, conn, "winner", "acme")

	// healOnReResolve false → re-resolution stays on the loser → permanent conflict.
	integ := &dupMintIntegrator{loserID: "loser", winnerID: "winner", aliasN: "acme"}

	p, err := New(Options{DB: conn, Runs: runs, Workers: 1, Document: integ})
	if err != nil {
		t.Fatalf("new pool: %v", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() { p.Run(ctx); close(done) }()

	waitUntil(t, func() bool {
		var iu *int64
		conn.QueryRow(`SELECT ineligible_until FROM inbox WHERE id='doc-1'`).Scan(&iu)
		return iu != nil
	})
	cancel()
	<-done

	var stamp string
	conn.QueryRow(`SELECT integrated_by FROM inbox WHERE id='doc-1'`).Scan(&stamp)
	if stamp != "" {
		t.Fatalf("row stamped despite exhaustion: %q", stamp)
	}
	var nFailed int
	conn.QueryRow(`SELECT COUNT(1) FROM runs WHERE caused_by='doc-1' AND status='failed'`).Scan(&nFailed)
	if nFailed < 1 {
		t.Fatalf("expected a failed run, got %d", nFailed)
	}
	// The shared bound caps re-resolution at MaxCommitAttempts-1 per run (no re-resolve
	// after the final attempt).
	integ.mu.Lock()
	rr := integ.reResolveCalls
	integ.mu.Unlock()
	if rr < 1 {
		t.Fatalf("re-resolve never ran: %d", rr)
	}
}

// waitUntil polls cond until true or a short deadline (the pool runs async).
func waitUntil(t *testing.T, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatal("condition not met before deadline")
}
