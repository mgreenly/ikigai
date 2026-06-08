package script

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"sync"
	"testing"
)

// fakeRunner records Spawn/Cancel calls instead of execing python.
type fakeRunner struct {
	mu       sync.Mutex
	spawns   []spawnCall
	cancels  []string
	cancelOK bool // value Cancel returns
}

type spawnCall struct {
	run   Run
	input []byte
}

func (f *fakeRunner) Spawn(run Run, input []byte) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.spawns = append(f.spawns, spawnCall{run: run, input: append([]byte(nil), input...)})
}

func (f *fakeRunner) Cancel(runID string) bool {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.cancels = append(f.cancels, runID)
	return f.cancelOK
}

func (f *fakeRunner) lastSpawn(t *testing.T) spawnCall {
	t.Helper()
	f.mu.Lock()
	defer f.mu.Unlock()
	if len(f.spawns) == 0 {
		t.Fatalf("expected a Spawn, got none")
	}
	return f.spawns[len(f.spawns)-1]
}

// newTestService builds a Service over a real temp-db store and a fake runner.
// runsDir is a temp dir that stands in for <dataDir>/runs.
func newTestService(t *testing.T) (*Service, *Store, *fakeRunner, string) {
	t.Helper()
	store := newTestStore(t)
	fr := &fakeRunner{}
	runsDir := t.TempDir()
	svc := NewService(store, runsDir, fr)
	return svc, store, fr, runsDir
}

func TestServiceCreate(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()

	// Validation: empty name / body.
	if _, err := svc.Create(ctx, ownerA, CreateInput{Name: "", Body: "print(1)"}); !errors.Is(err, ErrValidation) {
		t.Fatalf("empty name: want ErrValidation, got %v", err)
	}
	if _, err := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "  "}); !errors.Is(err, ErrValidation) {
		t.Fatalf("empty body: want ErrValidation, got %v", err)
	}

	// Success: id + timestamps set, config defaulted to python3.
	sc, err := svc.Create(ctx, ownerA, CreateInput{Name: "nightly", Body: "print('hi')"})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if sc.ID == "" || sc.CreatedAt == "" || sc.UpdatedAt == "" {
		t.Fatalf("Create: missing id/timestamps: %+v", sc)
	}
	if sc.Config.Interpreter != "python3" {
		t.Fatalf("Create: want default interpreter python3, got %q", sc.Config.Interpreter)
	}
	if sc.OwnerEmail != ownerA {
		t.Fatalf("Create: owner = %q", sc.OwnerEmail)
	}
}

func TestServiceUpdatePartial(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "orig", Body: "print(1)"})

	newName := "renamed"
	got, err := svc.Update(ctx, ownerA, sc.ID, UpdateInput{Name: &newName})
	if err != nil {
		t.Fatalf("Update: %v", err)
	}
	if got.Name != "renamed" {
		t.Fatalf("Update name = %q", got.Name)
	}
	if got.Body != "print(1)" {
		t.Fatalf("Update should leave body unchanged, got %q", got.Body)
	}
	if got.UpdatedAt == sc.UpdatedAt {
		// Both are RFC3339Nano; with the same monotonic clock they can collide
		// only if no time passed. Tolerate equality but require non-empty.
		if got.UpdatedAt == "" {
			t.Fatalf("Update: empty UpdatedAt")
		}
	}

	// Owner mismatch → ErrNotFound.
	if _, err := svc.Update(ctx, ownerB, sc.ID, UpdateInput{Name: &newName}); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign update: want ErrNotFound, got %v", err)
	}
}

func TestServiceDeleteTombstone(t *testing.T) {
	svc, store, _, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})
	// A run exists; delete must not remove it (tombstone).
	seedRun(t, store, sc.ID, RunSucceeded)

	if err := svc.Delete(ctx, ownerA, sc.ID); err != nil {
		t.Fatalf("Delete: %v", err)
	}
	if _, err := svc.Get(ctx, ownerA, sc.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("after delete Get: want ErrNotFound, got %v", err)
	}
	// Runs survive as history.
	runs, err := store.ListRuns(ctx, ownerA, "", "")
	if err != nil {
		t.Fatalf("ListRuns: %v", err)
	}
	// ListRuns joins scripts; with the script tombstoned the run dangles, so the
	// owner-scoped ListRuns no longer sees it — assert the run row itself still
	// exists via a direct count.
	_ = runs
	var n int
	if err := store.db.QueryRowContext(ctx, `SELECT COUNT(*) FROM runs WHERE script_id = ?`, sc.ID).Scan(&n); err != nil {
		t.Fatalf("count runs: %v", err)
	}
	if n != 1 {
		t.Fatalf("tombstone: run should survive, count = %d", n)
	}

	// Foreign / missing delete → ErrNotFound.
	if err := svc.Delete(ctx, ownerA, "nope"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("delete missing: want ErrNotFound, got %v", err)
	}
}

func TestServiceListGetDerived(t *testing.T) {
	svc, store, _, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})
	seedRun(t, store, sc.ID, RunRunning)
	seedRun(t, store, sc.ID, RunSucceeded)

	detail, err := svc.Get(ctx, ownerA, sc.ID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if detail.RunningCount != 1 {
		t.Fatalf("RunningCount = %d, want 1", detail.RunningCount)
	}
	if detail.LastRun == nil {
		t.Fatalf("LastRun nil")
	}

	list, err := svc.List(ctx, ownerA)
	if err != nil {
		t.Fatalf("List: %v", err)
	}
	if len(list) != 1 {
		t.Fatalf("List len = %d, want 1", len(list))
	}
	if list[0].RunningCount != 1 || list[0].LastRun == nil {
		t.Fatalf("List detail not attached: %+v", list[0])
	}

	// Owner B sees nothing / ErrNotFound.
	if _, err := svc.Get(ctx, ownerB, sc.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign Get: want ErrNotFound, got %v", err)
	}
	if l, _ := svc.List(ctx, ownerB); len(l) != 0 {
		t.Fatalf("foreign List len = %d, want 0", len(l))
	}
}

func TestServiceRunManual(t *testing.T) {
	svc, store, fr, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})

	run, err := svc.Run(ctx, ownerA, sc.ID)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}
	if run.Status != RunRunning {
		t.Fatalf("Run status = %q", run.Status)
	}
	if run.TriggerSource != "" {
		t.Fatalf("manual run should have empty trigger, got %q", run.TriggerSource)
	}
	// A running row was inserted.
	got, err := store.GetRun(ctx, ownerA, run.ID)
	if err != nil {
		t.Fatalf("GetRun: %v", err)
	}
	if got.Status != RunRunning {
		t.Fatalf("inserted run status = %q", got.Status)
	}
	// Spawn called once with "{}".
	if len(fr.spawns) != 1 {
		t.Fatalf("Spawn called %d times, want 1", len(fr.spawns))
	}
	sp := fr.lastSpawn(t)
	if string(sp.input) != "{}" {
		t.Fatalf("manual Spawn input = %q, want {}", sp.input)
	}

	// Owner mismatch → ErrNotFound, no spawn.
	if _, err := svc.Run(ctx, ownerB, sc.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign Run: want ErrNotFound, got %v", err)
	}
	if len(fr.spawns) != 1 {
		t.Fatalf("foreign Run should not Spawn; count = %d", len(fr.spawns))
	}
}

func TestServiceRunForEvent(t *testing.T) {
	svc, store, fr, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})

	payload := []byte(`{"id":"evt1"}`)
	if err := svc.RunForEvent(ctx, sc.ID, "crm", "contact.created", "evt1", payload); err != nil {
		t.Fatalf("RunForEvent: %v", err)
	}
	sp := fr.lastSpawn(t)
	if string(sp.input) != string(payload) {
		t.Fatalf("RunForEvent Spawn input = %q, want %q", sp.input, payload)
	}
	if sp.run.TriggerSource != "crm" || sp.run.TriggerType != "contact.created" || sp.run.TriggerEventID != "evt1" {
		t.Fatalf("RunForEvent trigger fields = %+v", sp.run)
	}
	// Row persisted with trigger fields.
	got, err := store.GetRun(ctx, ownerA, sp.run.ID)
	if err != nil {
		t.Fatalf("GetRun: %v", err)
	}
	if got.TriggerSource != "crm" {
		t.Fatalf("persisted trigger source = %q", got.TriggerSource)
	}

	// Missing script → no-op (nil), no spawn.
	before := len(fr.spawns)
	if err := svc.RunForEvent(ctx, "does-not-exist", "crm", "contact.created", "evt2", payload); err != nil {
		t.Fatalf("RunForEvent missing script: want nil, got %v", err)
	}
	if len(fr.spawns) != before {
		t.Fatalf("RunForEvent missing script should not Spawn")
	}
}

func TestServiceRunGetElapsed(t *testing.T) {
	svc, store, _, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})
	r := Run{
		ID:         "run-elapsed",
		ScriptID:   sc.ID,
		Status:     RunSucceeded,
		StartedAt:  "2020-01-01T00:00:00Z",
		EndedAt:    "2020-01-01T00:00:05Z",
		StdoutPath: "runs/x/stdout.log",
		StderrPath: "runs/x/stderr.log",
	}
	if err := store.InsertRun(ctx, r); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	got, err := svc.RunGet(ctx, ownerA, r.ID)
	if err != nil {
		t.Fatalf("RunGet: %v", err)
	}
	if got.ElapsedSecs != 5 {
		t.Fatalf("ElapsedSecs = %d, want 5", got.ElapsedSecs)
	}

	if _, err := svc.RunGet(ctx, ownerB, r.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign RunGet: want ErrNotFound, got %v", err)
	}
}

func TestServiceRunCancel(t *testing.T) {
	svc, store, fr, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})
	run := seedRun(t, store, sc.ID, RunRunning)

	// In flight: Cancel returns true.
	fr.cancelOK = true
	if err := svc.RunCancel(ctx, ownerA, run.ID); err != nil {
		t.Fatalf("RunCancel: %v", err)
	}
	if len(fr.cancels) != 1 || fr.cancels[0] != run.ID {
		t.Fatalf("runner.Cancel not called with run id: %v", fr.cancels)
	}

	// Not in flight: ErrValidation.
	fr.cancelOK = false
	run2 := seedRun(t, store, sc.ID, RunRunning)
	if err := svc.RunCancel(ctx, ownerA, run2.ID); !errors.Is(err, ErrValidation) {
		t.Fatalf("RunCancel not-in-flight: want ErrValidation, got %v", err)
	}

	// Already terminal: ErrValidation, no Cancel call.
	term := seedRun(t, store, sc.ID, RunSucceeded)
	before := len(fr.cancels)
	if err := svc.RunCancel(ctx, ownerA, term.ID); !errors.Is(err, ErrValidation) {
		t.Fatalf("RunCancel terminal: want ErrValidation, got %v", err)
	}
	if len(fr.cancels) != before {
		t.Fatalf("RunCancel terminal should not call runner.Cancel")
	}

	// Foreign owner → ErrNotFound.
	if err := svc.RunCancel(ctx, ownerB, run.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign RunCancel: want ErrNotFound, got %v", err)
	}
}

func TestServiceTriggers(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})

	// Unknown source → ErrValidation.
	if _, err := svc.SetTrigger(ctx, ownerA, sc.ID, "bogus", "x.*"); !errors.Is(err, ErrValidation) {
		t.Fatalf("unknown source: want ErrValidation, got %v", err)
	}
	// Unsatisfiable filter → ErrValidation.
	if _, err := svc.SetTrigger(ctx, ownerA, sc.ID, "crm", "transaction.recorded"); !errors.Is(err, ErrValidation) {
		t.Fatalf("bad filter: want ErrValidation, got %v", err)
	}

	// Success.
	tr, err := svc.SetTrigger(ctx, ownerA, sc.ID, "crm", "contact.*")
	if err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}
	if tr.ScriptID != sc.ID || tr.Source != "crm" || tr.EventFilter != "contact.*" || tr.CreatedAt == "" {
		t.Fatalf("SetTrigger returned %+v", tr)
	}

	// ScriptsForEvent finds it.
	ids, err := svc.ScriptsForEvent(ctx, "crm", "contact.created")
	if err != nil {
		t.Fatalf("ScriptsForEvent: %v", err)
	}
	if len(ids) != 1 || ids[0] != sc.ID {
		t.Fatalf("ScriptsForEvent = %v, want [%s]", ids, sc.ID)
	}

	// Foreign SetTrigger → ErrNotFound.
	if _, err := svc.SetTrigger(ctx, ownerB, sc.ID, "crm", "contact.*"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign SetTrigger: want ErrNotFound, got %v", err)
	}

	// ClearTrigger.
	if err := svc.ClearTrigger(ctx, ownerA, sc.ID, "crm", "contact.*"); err != nil {
		t.Fatalf("ClearTrigger: %v", err)
	}
	ids2, _ := svc.ScriptsForEvent(ctx, "crm", "contact.created")
	if len(ids2) != 0 {
		t.Fatalf("after ClearTrigger ScriptsForEvent = %v, want empty", ids2)
	}

	// TriggerSources is the static set.
	if len(svc.TriggerSources()) == 0 {
		t.Fatalf("TriggerSources empty")
	}
}

func TestServiceRunOutputAndFs(t *testing.T) {
	svc, store, _, runsDir := newTestService(t)
	ctx := context.Background()
	sc, _ := svc.Create(ctx, ownerA, CreateInput{Name: "x", Body: "print(1)"})
	run := seedRun(t, store, sc.ID, RunSucceeded)

	// Hand-create the persisted run dir under runsDir/<run_id>/.
	dir := filepath.Join(runsDir, run.ID)
	if err := os.MkdirAll(filepath.Join(dir, "out"), 0o700); err != nil {
		t.Fatalf("mkdir run dir: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "stdout.log"), []byte("line1\nline2\nline3\n"), 0o600); err != nil {
		t.Fatalf("write stdout: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "stderr.log"), []byte("err1\n"), 0o600); err != nil {
		t.Fatalf("write stderr: %v", err)
	}
	if err := os.WriteFile(filepath.Join(dir, "out", "result.txt"), []byte("a\nb\n"), 0o600); err != nil {
		t.Fatalf("write artifact: %v", err)
	}

	// RunOutput stdout line-slice [offset=2, limit=1) → line2.
	out, err := svc.RunOutput(ctx, ownerA, run.ID, "stdout", 2, 1)
	if err != nil {
		t.Fatalf("RunOutput: %v", err)
	}
	if out != "line2\n" {
		t.Fatalf("RunOutput slice = %q, want line2", out)
	}

	// RunOutput unknown stream → ErrValidation.
	if _, err := svc.RunOutput(ctx, ownerA, run.ID, "bogus", 0, 0); !errors.Is(err, ErrValidation) {
		t.Fatalf("RunOutput bad stream: want ErrValidation, got %v", err)
	}

	// RunFsList root: sees stdout.log, stderr.log, out/.
	entries, err := svc.RunFsList(ctx, ownerA, run.ID, "")
	if err != nil {
		t.Fatalf("RunFsList: %v", err)
	}
	names := map[string]FileEntry{}
	for _, e := range entries {
		names[e.Path] = e
	}
	if _, ok := names["stdout.log"]; !ok {
		t.Fatalf("RunFsList missing stdout.log: %+v", entries)
	}
	if d, ok := names["out"]; !ok || !d.IsDir {
		t.Fatalf("RunFsList missing out dir: %+v", entries)
	}

	// RunFsList subdir.
	sub, err := svc.RunFsList(ctx, ownerA, run.ID, "out")
	if err != nil {
		t.Fatalf("RunFsList subdir: %v", err)
	}
	if len(sub) != 1 || sub[0].Path != filepath.Join("out", "result.txt") {
		t.Fatalf("RunFsList subdir = %+v", sub)
	}

	// RunFsRead a file.
	body, err := svc.RunFsRead(ctx, ownerA, run.ID, "out/result.txt", 0, 0)
	if err != nil {
		t.Fatalf("RunFsRead: %v", err)
	}
	if body != "a\nb\n" {
		t.Fatalf("RunFsRead = %q", body)
	}

	// Path traversal rejected.
	if _, err := svc.RunFsList(ctx, ownerA, run.ID, "../.."); !errors.Is(err, ErrValidation) {
		t.Fatalf("RunFsList traversal: want ErrValidation, got %v", err)
	}
	if _, err := svc.RunFsRead(ctx, ownerA, run.ID, "../../etc/passwd", 0, 0); !errors.Is(err, ErrValidation) {
		t.Fatalf("RunFsRead traversal: want ErrValidation, got %v", err)
	}

	// Foreign owner → ErrNotFound.
	if _, err := svc.RunOutput(ctx, ownerB, run.ID, "stdout", 0, 0); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign RunOutput: want ErrNotFound, got %v", err)
	}
	if _, err := svc.RunFsList(ctx, ownerB, run.ID, ""); !errors.Is(err, ErrNotFound) {
		t.Fatalf("foreign RunFsList: want ErrNotFound, got %v", err)
	}
}
