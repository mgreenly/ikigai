package script

import (
	"context"
	"database/sql"
	"errors"
	"path/filepath"
	"testing"
	"time"

	appkitdatabase "appkit/db"
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
	conn, err := appkitdatabase.Open(filepath.Join(t.TempDir(), "scripts.db"))
	if err != nil {
		t.Fatalf("appkitdatabase.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdatabase.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("appkitdatabase.LoadMigrations: %v", err)
	}
	if err := appkitdatabase.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("appkitdatabase.Migrate: %v", err)
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

// TestSourcePathRoundTrip asserts an import-managed script's source_path
// survives insert→get→list, and that UpdateScript does NOT clobber it (a
// hand-edit via the normal update tool must leave the import binding intact).
func TestSourcePathRoundTrip(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	now := nowStr()
	sc := Script{
		ID:         ids.NewULID(),
		OwnerEmail: ownerA,
		Name:       "nightly",
		Body:       "print('hi')",
		Config:     Config{Interpreter: "python3"},
		SourcePath: "/scripts/nightly.py",
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := s.InsertScript(ctx, sc); err != nil {
		t.Fatalf("InsertScript: %v", err)
	}
	got, err := s.GetScript(ctx, ownerA, sc.ID)
	if err != nil {
		t.Fatalf("GetScript: %v", err)
	}
	if got.SourcePath != "/scripts/nightly.py" {
		t.Fatalf("source_path round-trip: got %q", got.SourcePath)
	}
	list, err := s.ListScripts(ctx, ownerA)
	if err != nil || len(list) != 1 || list[0].SourcePath != "/scripts/nightly.py" {
		t.Fatalf("ListScripts source_path: %+v err=%v", list, err)
	}

	// UpdateScript leaves source_path untouched even though Script carries "".
	upd := got
	upd.SourcePath = ""
	upd.Body = "print('bye')"
	upd.UpdatedAt = nowStr()
	if err := s.UpdateScript(ctx, ownerA, upd); err != nil {
		t.Fatalf("UpdateScript: %v", err)
	}
	got, _ = s.GetScript(ctx, ownerA, sc.ID)
	if got.SourcePath != "/scripts/nightly.py" {
		t.Fatalf("UpdateScript clobbered source_path: got %q", got.SourcePath)
	}

	// A hand-authored script (NULL source_path) reads back as "".
	hand := seedScript(t, s, ownerB)
	got, _ = s.GetScript(ctx, ownerB, hand.ID)
	if got.SourcePath != "" {
		t.Fatalf("hand-authored source_path: want \"\", got %q", got.SourcePath)
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
	if err := s.SetTrigger(ctx, ownerA, Trigger{ScriptID: sc.ID, Source: "crm", Filter: "crm:contact.created", CreatedAt: nowStr()}); err != nil {
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
		TriggerSource: "cron", TriggerKind: "tick", TriggerSubject: "/nightly", TriggerEventID: "evt1",
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
	if err := s.db.QueryRow(`SELECT kind FROM outbox LIMIT 1`).Scan(&typ); err != nil {
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
	_ = s.db.QueryRow(`SELECT kind FROM outbox LIMIT 1`).Scan(&typ)
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
	// R-7XEU-W467
	// R-7YMR-9VWW
	s := newTestStore(t)
	ctx := context.Background()
	sc1 := seedScript(t, s, ownerA)
	sc2 := seedScript(t, s, ownerA)
	sc3 := seedScript(t, s, ownerA)
	svc := NewService(s, t.TempDir(), nil)

	// The service derives source from the canonical filter; callers do not get
	// to provide a second, potentially mismatched source argument.
	if _, err := svc.SetTrigger(ctx, ownerB, sc1.ID, "dropbox:create/bills/**/*.pdf"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign SetTrigger: want ErrNotFound, got %v", err)
	}
	trig, err := svc.SetTrigger(ctx, ownerA, sc1.ID, "dropbox:create/bills/**/*.pdf")
	if err != nil {
		t.Fatalf("SetTrigger sc1: %v", err)
	}
	var source string
	var count int
	if err := s.db.QueryRowContext(ctx, `SELECT source, COUNT(*) FROM script_triggers WHERE script_id = ? AND filter = ?`, sc1.ID, trig.Filter).Scan(&source, &count); err != nil {
		t.Fatal(err)
	}
	if source != "dropbox" || count != 1 {
		t.Fatalf("stored trigger = source %q, count %d", source, count)
	}
	if _, err := svc.SetTrigger(ctx, ownerA, sc1.ID, trig.Filter); err != nil {
		t.Fatalf("repeat SetTrigger: %v", err)
	}
	if err := s.db.QueryRowContext(ctx, `SELECT COUNT(*) FROM script_triggers WHERE script_id = ? AND filter = ?`, sc1.ID, trig.Filter).Scan(&count); err != nil {
		t.Fatal(err)
	}
	if count != 1 {
		t.Fatalf("repeat SetTrigger created %d rows", count)
	}
	if _, err := svc.SetTrigger(ctx, ownerA, sc2.ID, "dropbox:**"); err != nil {
		t.Fatalf("SetTrigger sc2: %v", err)
	}
	if _, err := svc.SetTrigger(ctx, ownerA, sc3.ID, "dropbox:create/bills/*.pdf"); err != nil {
		t.Fatalf("SetTrigger sc3: %v", err)
	}
	got, err := s.ScriptsForEvent(ctx, "dropbox", "dropbox:create/bills/aws/1.pdf")
	if err != nil {
		t.Fatalf("ScriptsForEvent: %v", err)
	}
	if len(got) != 2 || got[0] != sc1.ID || got[1] != sc2.ID {
		t.Fatalf("nested bill: want [%s %s], got %v", sc1.ID, sc2.ID, got)
	}

	// contact.updated matches only the glob (sc1)
	got, _ = s.ScriptsForEvent(ctx, "dropbox", "dropbox:create/notes.txt")
	if len(got) != 1 || got[0] != sc2.ID {
		t.Fatalf("notes: want [sc2], got %v", got)
	}

	// wrong source matches nothing
	got, _ = s.ScriptsForEvent(ctx, "crm", "crm:contact.created")
	if len(got) != 0 {
		t.Fatalf("dropbox: want 0, got %v", got)
	}

	// ClearTrigger removes the sc1 binding
	if err := svc.ClearTrigger(ctx, ownerA, sc1.ID, "dropbox:create/bills/**/*.pdf"); err != nil {
		t.Fatalf("ClearTrigger: %v", err)
	}
	if err := svc.ClearTrigger(ctx, ownerA, sc1.ID, "dropbox:create/bills/**/*.pdf"); err != nil {
		t.Fatal(err)
	}
	if err := svc.ClearTrigger(ctx, ownerB, sc1.ID, "dropbox:create/bills/**/*.pdf"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign ClearTrigger: want ErrNotFound, got %v", err)
	}
	if err := svc.ClearTrigger(ctx, ownerA, "missing", "dropbox:create/bills/**/*.pdf"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("missing ClearTrigger: want ErrNotFound, got %v", err)
	}
	got, _ = s.ScriptsForEvent(ctx, "dropbox", "dropbox:create/bills/aws/1.pdf")
	if len(got) != 1 || got[0] != sc2.ID {
		t.Fatalf("after clear: want 0, got %v", got)
	}
}

// guard against an accidental import-only compile of database/sql
var _ = sql.ErrNoRows
