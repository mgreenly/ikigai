package runner

import (
	"context"
	"database/sql"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"

	appkitdatabase "appkit/db"
	"eventplane/outbox"

	"scripts/internal/db"
	"scripts/internal/ids"
	"scripts/internal/script"
)

const owner = "a@example.com"

func nowStr() string { return time.Now().UTC().Format(time.RFC3339Nano) }

// newStore mirrors store_test's DB setup: a migrated temp SQLite DB with a real
// producer outbox attached so completion events land in the outbox table.
func newStore(t *testing.T) (*script.Store, *sql.DB) {
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
	st := script.NewStore(conn)
	ob, err := outbox.New(conn, outbox.Options{Source: "scripts", Registry: script.Events})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	st.Outbox = ob
	return st, conn
}

// seed inserts a script + a running run for it and returns the run.
func seed(t *testing.T, st *script.Store, body string) script.Run {
	t.Helper()
	ctx := context.Background()
	now := nowStr()
	sc := script.Script{
		ID:         ids.NewULID(),
		OwnerEmail: owner,
		Name:       "nightly",
		Body:       body,
		Config:     script.Config{Interpreter: "python3", TimeoutSecs: 30},
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := st.InsertScript(ctx, sc); err != nil {
		t.Fatalf("InsertScript: %v", err)
	}
	run := script.Run{
		ID:         ids.NewULID(),
		ScriptID:   sc.ID,
		Status:     script.RunRunning,
		StartedAt:  nowStr(),
		StdoutPath: "runs/x/stdout.log",
		StderrPath: "runs/x/stderr.log",
	}
	if err := st.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	return run
}

func outboxCount(t *testing.T, conn *sql.DB) int {
	t.Helper()
	var n int
	if err := conn.QueryRow(`SELECT COUNT(*) FROM outbox`).Scan(&n); err != nil {
		t.Fatalf("outbox count: %v", err)
	}
	return n
}

func lastOutboxPayload(t *testing.T, conn *sql.DB) string {
	t.Helper()
	var p string
	if err := conn.QueryRow(`SELECT payload FROM outbox ORDER BY created_at DESC, event_id DESC LIMIT 1`).Scan(&p); err != nil {
		t.Fatalf("outbox payload: %v", err)
	}
	return p
}

func getRun(t *testing.T, st *script.Store, runID string) script.Run {
	t.Helper()
	r, err := st.GetRun(context.Background(), owner, runID)
	if err != nil {
		t.Fatalf("GetRun: %v", err)
	}
	return r
}

// waitTerminal polls the run row until it leaves 'running' (or fails the test).
func waitTerminal(t *testing.T, st *script.Store, runID string) script.Run {
	t.Helper()
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		r := getRun(t, st, runID)
		if r.Status != script.RunRunning {
			return r
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatalf("run %s never reached a terminal status", runID)
	return script.Run{}
}

func requirePython(t *testing.T) {
	t.Helper()
	if _, err := exec.LookPath("python3"); err != nil {
		t.Skip("python3 not on PATH")
	}
}

func TestSpawnSucceeds(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second)

	body := "open('out.txt', 'w').write('done')\nprint('hello')\n"
	run := seed(t, st, body)

	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded", got.Status)
	}
	if got.ExitCode == nil || *got.ExitCode != 0 {
		t.Fatalf("exit code = %v, want 0", got.ExitCode)
	}
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}

	dir := filepath.Join(dataDir, "runs", run.ID)
	for _, f := range []string{"main.py", "config.json", "stdout.log", "out.txt"} {
		if _, err := os.Stat(filepath.Join(dir, f)); err != nil {
			t.Fatalf("expected %s in run dir: %v", f, err)
		}
	}
	if b, _ := os.ReadFile(filepath.Join(dir, "main.py")); string(b) != body {
		t.Fatalf("main.py = %q, want materialized body", b)
	}
	if b, _ := os.ReadFile(filepath.Join(dir, "out.txt")); string(b) != "done" {
		t.Fatalf("out.txt = %q, want 'done'", b)
	}
}

func TestSpawnUsesRebuildableRunsDirOutsideState(t *testing.T) {
	// R-4LKF-FB23
	requirePython(t)
	st, _ := newStore(t)
	root := t.TempDir()
	stateDir := filepath.Join(root, "state")
	if err := os.MkdirAll(stateDir, 0o700); err != nil {
		t.Fatalf("mkdir state: %v", err)
	}
	r := New(st, root, 30*time.Second)

	run := seed(t, st, "open('out.txt', 'w').write('artifact')\n")
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded", got.Status)
	}
	runDir := filepath.Join(root, "runs", run.ID)
	if _, err := os.Stat(filepath.Join(runDir, "out.txt")); err != nil {
		t.Fatalf("run artifact under non-state runs dir: %v", err)
	}
	if _, err := os.Stat(filepath.Join(stateDir, "runs")); !os.IsNotExist(err) {
		t.Fatalf("state/runs should not exist for rebuildable execution trees; stat err=%v", err)
	}
}

func TestSpawnFailsNonZero(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	r := New(st, t.TempDir(), 30*time.Second)

	run := seed(t, st, "import sys\nsys.exit(3)\n")
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunFailed {
		t.Fatalf("status = %q, want failed", got.Status)
	}
	if got.ExitCode == nil || *got.ExitCode != 3 {
		t.Fatalf("exit code = %v, want 3", got.ExitCode)
	}
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}
}

func TestSpawnStdoutTruncated(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second)

	// Print 10000 'A' chars (no newline) → > 8KB; tail must be the last 8192 'A'.
	run := seed(t, st, "import sys\nsys.stdout.write('A' * 10000)\n")
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)
	if got.Status != script.RunSucceeded {
		t.Fatalf("status = %q, want succeeded", got.Status)
	}
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}

	// Assert truncation via the emitted event payload.
	payload := lastOutboxPayload(t, conn)
	if !strings.Contains(payload, `"stdout_truncated":true`) {
		t.Fatalf("payload missing stdout_truncated=true: %s", payload)
	}
	want := strings.Repeat("A", tailMax)
	if !strings.Contains(payload, want) {
		t.Fatalf("payload tail not last 8KB of A's")
	}
	// The full file on disk keeps all 10000 bytes.
	b, _ := os.ReadFile(filepath.Join(dataDir, "runs", run.ID, "stdout.log"))
	if len(b) != 10000 {
		t.Fatalf("stdout.log len = %d, want 10000", len(b))
	}
}

func TestCancel(t *testing.T) {
	requirePython(t)
	st, conn := newStore(t)
	r := New(st, t.TempDir(), 30*time.Second)

	// A sleeper that writes its pid so we can confirm the kill.
	run := seed(t, st, "import time\ntime.sleep(60)\n")
	r.Spawn(run, []byte("{}"))

	// Wait until the run is registered as in-flight, then cancel.
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		if r.Cancel(run.ID) {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}

	got := waitTerminal(t, st, run.ID)
	if got.Status != script.RunCancelled {
		t.Fatalf("status = %q, want cancelled", got.Status)
	}
	if n := outboxCount(t, conn); n != 0 {
		t.Fatalf("outbox rows = %d, want 0 (cancel emits no event)", n)
	}
	// Cancel on an unknown/finished run returns false.
	if r.Cancel(run.ID) {
		t.Fatalf("Cancel on finished run should return false")
	}
}

func TestTTL(t *testing.T) {
	requirePython(t)
	st, _ := newStore(t)
	r := New(st, t.TempDir(), 150*time.Millisecond)

	run := seed(t, st, "import time\ntime.sleep(60)\n")
	start := time.Now()
	r.Spawn(run, []byte("{}"))
	got := waitTerminal(t, st, run.ID)

	if got.Status != script.RunFailed {
		t.Fatalf("status = %q, want failed", got.Status)
	}
	if got.Error != "run TTL exceeded" {
		t.Fatalf("error = %q, want 'run TTL exceeded'", got.Error)
	}
	if elapsed := time.Since(start); elapsed > 10*time.Second {
		t.Fatalf("TTL did not kill the process promptly (%v)", elapsed)
	}
}

func TestRecover(t *testing.T) {
	st, _ := newStore(t)
	r := New(st, t.TempDir(), 30*time.Second)

	// Plant a running row directly (simulating a crash mid-run).
	run := seed(t, st, "print('x')")

	n, err := r.Recover(context.Background())
	if err != nil {
		t.Fatalf("Recover: %v", err)
	}
	if n != 1 {
		t.Fatalf("Recover count = %d, want 1", n)
	}
	got := getRun(t, st, run.ID)
	if got.Status != script.RunFailed {
		t.Fatalf("status = %q, want failed", got.Status)
	}
}

func TestSpawnTombstonedScript(t *testing.T) {
	st, conn := newStore(t)
	dataDir := t.TempDir()
	r := New(st, dataDir, 30*time.Second)

	// Insert a script + its running run, then tombstone the script (the run row
	// survives with a dangling script_id, §7A). Spawning then hits the
	// ScriptForRun not-found path: fail the run, materialize nothing.
	run := seed(t, st, "print('never runs')")
	if err := st.DeleteScript(context.Background(), owner, run.ScriptID); err != nil {
		t.Fatalf("DeleteScript: %v", err)
	}

	r.Spawn(run, []byte("{}"))

	// The script row is gone, so GetRun's owner-scoped JOIN can't see this run;
	// poll the row directly instead.
	var status, errMsg string
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		if err := conn.QueryRow(`SELECT status, COALESCE(error,'') FROM runs WHERE id = ?`, run.ID).Scan(&status, &errMsg); err != nil {
			t.Fatalf("scan run row: %v", err)
		}
		if status != script.RunRunning {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	if status != script.RunFailed {
		t.Fatalf("status = %q, want failed", status)
	}
	if errMsg == "" {
		t.Fatalf("expected a failure error message")
	}
	// A failed run still emits the scripts.failed completion event.
	if n := outboxCount(t, conn); n != 1 {
		t.Fatalf("outbox rows = %d, want 1", n)
	}
	// Nothing materialized — no run dir.
	if _, err := os.Stat(filepath.Join(dataDir, "runs", run.ID)); err == nil {
		t.Fatalf("run dir should not exist for a tombstoned script")
	}
}
