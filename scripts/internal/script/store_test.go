package script

import (
	"context"
	"database/sql"
	"errors"
	"path/filepath"
	"testing"
	"time"

	"eventplane/outbox"

	"scripts/internal/db"
	"scripts/internal/ids"
)

const (
	ownerA = "a@example.com"
	ownerB = "b@example.com"
)

func nowStr() string { return time.Now().UTC().Format(time.RFC3339Nano) }

func newTestStore(t *testing.T) *Store {
	t.Helper()
	ctx := context.Background()
	conn, err := db.Open(filepath.Join(t.TempDir(), "scripts.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(ctx, conn); err != nil {
		t.Fatalf("db.Migrate: %v", err)
	}
	return NewStore(conn)
}

// withOutbox attaches a real producer outbox to the store (ephemeral generation,
// no DB-path probe — the migrations already created the outbox table).
func withOutbox(t *testing.T, s *Store) {
	t.Helper()
	ob, err := outbox.New(s.db, outbox.Options{Source: "scripts", Registry: Events})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	s.Outbox = ob
}

func seedScript(t *testing.T, s *Store, owner string) Script {
	t.Helper()
	now := nowStr()
	sc := Script{
		ID:         ids.NewULID(),
		OwnerEmail: owner,
		Name:       "nightly",
		Body:       "print('hi')",
		Config:     Config{Interpreter: "python3", TimeoutSecs: 30},
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := s.InsertScript(context.Background(), sc); err != nil {
		t.Fatalf("InsertScript: %v", err)
	}
	return sc
}

func seedRun(t *testing.T, s *Store, scriptID, status string) Run {
	t.Helper()
	r := Run{
		ID:         ids.NewULID(),
		ScriptID:   scriptID,
		Status:     status,
		StartedAt:  nowStr(),
		StdoutPath: "runs/x/stdout.log",
		StderrPath: "runs/x/stderr.log",
	}
	if err := s.InsertRun(context.Background(), r); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	return r
}

func outboxCount(t *testing.T, s *Store) int {
	t.Helper()
	var n int
	if err := s.db.QueryRow(`SELECT COUNT(*) FROM outbox`).Scan(&n); err != nil {
		t.Fatalf("outbox count: %v", err)
	}
	return n
}

func TestScriptCRUDAndOwnerScope(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)

	// foreign owner reads as absent
	if _, err := s.GetScript(ctx, ownerB, sc.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign owner: want ErrNotFound, got %v", err)
	}
	got, err := s.GetScript(ctx, ownerA, sc.ID)
	if err != nil {
		t.Fatalf("GetScript: %v", err)
	}
	if got.Config.TimeoutSecs != 30 || got.Name != "nightly" {
		t.Fatalf("round-trip: %+v", got)
	}

	// update owner-scoped
	sc.Body = "print('bye')"
	sc.UpdatedAt = nowStr()
	if err := s.UpdateScript(ctx, ownerB, sc); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign update: want ErrNotFound, got %v", err)
	}
	if err := s.UpdateScript(ctx, ownerA, sc); err != nil {
		t.Fatalf("UpdateScript: %v", err)
	}
	got, _ = s.GetScript(ctx, ownerA, sc.ID)
	if got.Body != "print('bye')" {
		t.Fatalf("body not updated: %q", got.Body)
	}

	// list owner-scoped
	seedScript(t, s, ownerB)
	list, err := s.ListScripts(ctx, ownerA)
	if err != nil || len(list) != 1 {
		t.Fatalf("ListScripts ownerA: len=%d err=%v", len(list), err)
	}
}

func TestScriptForRunUnscoped(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)

	got, err := s.ScriptForRun(ctx, sc.ID)
	if err != nil {
		t.Fatalf("ScriptForRun: %v", err)
	}
	if got.ID != sc.ID || got.OwnerEmail != ownerA || got.Name != "nightly" ||
		got.Body != sc.Body || got.Config.TimeoutSecs != 30 {
		t.Fatalf("ScriptForRun round-trip: %+v", got)
	}

	if _, err := s.ScriptForRun(ctx, "nonexistent"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("unknown id: want ErrNotFound, got %v", err)
	}
}

func TestDeleteScriptTombstone(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)
	run := seedRun(t, s, sc.ID, RunSucceeded)
	if err := s.SetTrigger(ctx, ownerA, Trigger{ScriptID: sc.ID, Source: "crm", EventFilter: "contact.created", CreatedAt: nowStr()}); err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}

	// foreign delete is a no-op ErrNotFound
	if err := s.DeleteScript(ctx, ownerB, sc.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign delete: want ErrNotFound, got %v", err)
	}
	if err := s.DeleteScript(ctx, ownerA, sc.ID); err != nil {
		t.Fatalf("DeleteScript: %v", err)
	}

	// script gone
	if _, err := s.GetScript(ctx, ownerA, sc.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("script should be gone: %v", err)
	}
	// run survives (raw row still present)
	var n int
	if err := s.db.QueryRow(`SELECT COUNT(*) FROM runs WHERE id = ?`, run.ID).Scan(&n); err != nil {
		t.Fatalf("count run: %v", err)
	}
	if n != 1 {
		t.Fatalf("run should survive tombstone, got %d", n)
	}
	// triggers cascade away
	if err := s.db.QueryRow(`SELECT COUNT(*) FROM script_triggers WHERE script_id = ?`, sc.ID).Scan(&n); err != nil {
		t.Fatalf("count triggers: %v", err)
	}
	if n != 0 {
		t.Fatalf("triggers should cascade-delete, got %d", n)
	}
}

func TestRunsInsertGetListOwnerScope(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	scA := seedScript(t, s, ownerA)
	scB := seedScript(t, s, ownerB)
	rA := seedRun(t, s, scA.ID, RunRunning)
	seedRun(t, s, scB.ID, RunRunning)

	// GetRun owner-scoped
	if _, err := s.GetRun(ctx, ownerB, rA.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign GetRun: want ErrNotFound, got %v", err)
	}
	got, err := s.GetRun(ctx, ownerA, rA.ID)
	if err != nil || got.ScriptID != scA.ID {
		t.Fatalf("GetRun: %+v err=%v", got, err)
	}

	// ListRuns owner-scoped + status filter
	all, err := s.ListRuns(ctx, ownerA, "", "")
	if err != nil || len(all) != 1 {
		t.Fatalf("ListRuns ownerA: len=%d err=%v", len(all), err)
	}
	none, _ := s.ListRuns(ctx, ownerA, scA.ID, RunSucceeded)
	if len(none) != 0 {
		t.Fatalf("status filter: want 0, got %d", len(none))
	}
}

func TestRunningCountAndLastRun(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)

	if last, err := s.LastRun(ctx, sc.ID); err != nil || last != nil {
		t.Fatalf("LastRun none: want nil, got %+v err=%v", last, err)
	}

	seedRun(t, s, sc.ID, RunRunning)
	time.Sleep(2 * time.Millisecond)
	r2 := seedRun(t, s, sc.ID, RunRunning)
	seedRun(t, s, sc.ID, RunSucceeded)

	n, err := s.RunningCount(ctx, sc.ID)
	if err != nil || n != 2 {
		t.Fatalf("RunningCount: want 2, got %d err=%v", n, err)
	}
	last, err := s.LastRun(ctx, sc.ID)
	if err != nil || last == nil {
		t.Fatalf("LastRun: %+v err=%v", last, err)
	}
	_ = r2 // ordering by started_at; the succeeded one was inserted last
}

func TestSweepRunning(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)
	r1 := seedRun(t, s, sc.ID, RunRunning)
	r2 := seedRun(t, s, sc.ID, RunRunning)
	seedRun(t, s, sc.ID, RunSucceeded)

	ids, err := s.SweepRunning(ctx)
	if err != nil {
		t.Fatalf("SweepRunning: %v", err)
	}
	if len(ids) != 2 {
		t.Fatalf("swept ids: want 2, got %v", ids)
	}
	set := map[string]bool{ids[0]: true, ids[1]: true}
	if !set[r1.ID] || !set[r2.ID] {
		t.Fatalf("swept wrong ids: %v", ids)
	}
	n, _ := s.RunningCount(ctx, sc.ID)
	if n != 0 {
		t.Fatalf("after sweep RunningCount: want 0, got %d", n)
	}
	got, _ := s.GetRun(ctx, ownerA, r1.ID)
	if got.Status != RunFailed || got.Error == "" {
		t.Fatalf("swept run not failed: %+v", got)
	}
}

func TestFinishRunSucceededEmits(t *testing.T) {
	s := newTestStore(t)
	withOutbox(t, s)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)
	run := seedRun(t, s, sc.ID, RunRunning)

	zero := 0
	in := FinishRunInput{
		RunID: run.ID, ScriptID: sc.ID, ScriptName: sc.Name,
		Status: RunSucceeded, ExitCode: &zero, EndedAt: nowStr(),
		TriggerSource: "cron", TriggerType: "cron.nightly", TriggerEventID: "evt1",
		StdoutTail: "ok\n",
	}
	if err := s.FinishRun(ctx, in); err != nil {
		t.Fatalf("FinishRun: %v", err)
	}
	got, _ := s.GetRun(ctx, ownerA, run.ID)
	if got.Status != RunSucceeded || got.ExitCode == nil || *got.ExitCode != 0 {
		t.Fatalf("row not updated: %+v", got)
	}
	if c := outboxCount(t, s); c != 1 {
		t.Fatalf("outbox rows: want 1, got %d", c)
	}
	var typ string
	if err := s.db.QueryRow(`SELECT type FROM outbox LIMIT 1`).Scan(&typ); err != nil {
		t.Fatalf("read outbox: %v", err)
	}
	if typ != EventSucceeded {
		t.Fatalf("event type: want %q, got %q", EventSucceeded, typ)
	}
}

func TestFinishRunFailedEmitsWithError(t *testing.T) {
	s := newTestStore(t)
	withOutbox(t, s)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)
	run := seedRun(t, s, sc.ID, RunRunning)

	in := FinishRunInput{
		RunID: run.ID, ScriptID: sc.ID, ScriptName: sc.Name,
		Status: RunFailed, EndedAt: nowStr(), ErrMsg: "run TTL exceeded",
		StderrTail: "boom\n",
	}
	if err := s.FinishRun(ctx, in); err != nil {
		t.Fatalf("FinishRun: %v", err)
	}
	got, _ := s.GetRun(ctx, ownerA, run.ID)
	if got.Status != RunFailed || got.Error != "run TTL exceeded" {
		t.Fatalf("error not stored: %+v", got)
	}
	if c := outboxCount(t, s); c != 1 {
		t.Fatalf("outbox rows: want 1, got %d", c)
	}
	var typ string
	_ = s.db.QueryRow(`SELECT type FROM outbox LIMIT 1`).Scan(&typ)
	if typ != EventFailed {
		t.Fatalf("event type: want %q, got %q", EventFailed, typ)
	}
}

func TestFinishRunCancelledNoEmit(t *testing.T) {
	s := newTestStore(t)
	withOutbox(t, s)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)
	run := seedRun(t, s, sc.ID, RunRunning)

	in := FinishRunInput{
		RunID: run.ID, ScriptID: sc.ID, ScriptName: sc.Name,
		Status: RunCancelled, EndedAt: nowStr(),
	}
	if err := s.FinishRun(ctx, in); err != nil {
		t.Fatalf("FinishRun: %v", err)
	}
	got, _ := s.GetRun(ctx, ownerA, run.ID)
	if got.Status != RunCancelled {
		t.Fatalf("row not updated: %+v", got)
	}
	if c := outboxCount(t, s); c != 0 {
		t.Fatalf("cancelled must NOT emit, got %d outbox rows", c)
	}
}

func TestFinishRunNilOutbox(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	sc := seedScript(t, s, ownerA)
	run := seedRun(t, s, sc.ID, RunRunning)
	zero := 0
	in := FinishRunInput{RunID: run.ID, ScriptID: sc.ID, Status: RunSucceeded, ExitCode: &zero, EndedAt: nowStr()}
	if err := s.FinishRun(ctx, in); err != nil {
		t.Fatalf("FinishRun nil outbox: %v", err)
	}
	got, _ := s.GetRun(ctx, ownerA, run.ID)
	if got.Status != RunSucceeded {
		t.Fatalf("row not updated: %+v", got)
	}
}

func TestTriggersAndScriptsForEvent(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	sc1 := seedScript(t, s, ownerA)
	sc2 := seedScript(t, s, ownerA)

	// foreign owner cannot set a trigger
	if err := s.SetTrigger(ctx, ownerB, Trigger{ScriptID: sc1.ID, Source: "crm", EventFilter: "contact.*", CreatedAt: nowStr()}); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign SetTrigger: want ErrNotFound, got %v", err)
	}
	if err := s.SetTrigger(ctx, ownerA, Trigger{ScriptID: sc1.ID, Source: "crm", EventFilter: "contact.*", CreatedAt: nowStr()}); err != nil {
		t.Fatalf("SetTrigger sc1: %v", err)
	}
	if err := s.SetTrigger(ctx, ownerA, Trigger{ScriptID: sc2.ID, Source: "crm", EventFilter: "contact.created", CreatedAt: nowStr()}); err != nil {
		t.Fatalf("SetTrigger sc2: %v", err)
	}
	if err := s.SetTrigger(ctx, ownerA, Trigger{ScriptID: sc2.ID, Source: "ledger", EventFilter: "transaction.recorded", CreatedAt: nowStr()}); err != nil {
		t.Fatalf("SetTrigger sc2 ledger: %v", err)
	}

	// contact.created matches both (glob contact.* + exact contact.created)
	got, err := s.ScriptsForEvent(ctx, "crm", "contact.created")
	if err != nil {
		t.Fatalf("ScriptsForEvent: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("contact.created: want 2 scripts, got %v", got)
	}

	// contact.updated matches only the glob (sc1)
	got, _ = s.ScriptsForEvent(ctx, "crm", "contact.updated")
	if len(got) != 1 || got[0] != sc1.ID {
		t.Fatalf("contact.updated: want [sc1], got %v", got)
	}

	// wrong source matches nothing
	got, _ = s.ScriptsForEvent(ctx, "dropbox", "file.created")
	if len(got) != 0 {
		t.Fatalf("dropbox: want 0, got %v", got)
	}

	// ClearTrigger removes the sc1 binding
	if err := s.ClearTrigger(ctx, ownerA, Trigger{ScriptID: sc1.ID, Source: "crm", EventFilter: "contact.*"}); err != nil {
		t.Fatalf("ClearTrigger: %v", err)
	}
	got, _ = s.ScriptsForEvent(ctx, "crm", "contact.updated")
	if len(got) != 0 {
		t.Fatalf("after clear: want 0, got %v", got)
	}
}

// guard against an accidental import-only compile of database/sql
var _ = sql.ErrNoRows
