// Package runner drives the async run lifecycle for scripts: it materializes a
// script's body + pinned config into a persistent per-run dir, execs `python3
// main.py` from that dir with the event payload on stdin/$EVENT_JSON, streams
// stdout/stderr to log files, and writes the run's terminal state back to the
// store (atomically emitting the completion event). See ARCHITECTURE.md §5.2 and
// PLAN.md §A6.
//
// Spawn returns immediately; the work happens on a goroutine. Cancel signals an
// in-flight run by run_id (distinguished from a TTL expiry so the run is
// classified cancelled rather than failed, and emits NO event). Recover is the
// boot-time crash sweep.
package runner

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"syscall"
	"time"

	"scripts/internal/script"
)

// python3 is the interpreter binary name (constant day-one; config.interpreter
// is reserved but not honored yet — A2/§10 deferred).
const python3 = "python3"

// tailMax is the byte ceiling for the captured stdout/stderr tails carried into
// the completion event (last 8 KB; §A6 step 9 / §A7 payload).
const tailMax = 8192

// Runner drives run lifecycles. It satisfies script.Runner.
type Runner struct {
	store   *script.Store
	dataDir string // service root: <dataDir>/runs/<run_id>/
	ttl     time.Duration

	mu      sync.Mutex
	cancels map[string]context.CancelFunc
	// userCancelled records run_ids whose in-flight run was cancelled by an
	// explicit Cancel call (vs a TTL expiry), so the goroutine classifies the
	// terminal status as cancelled (no event) rather than failed.
	userCancelled map[string]bool
}

// New constructs a Runner over the store, the service root (run trees live
// under <dataDir>/runs/), and the per-run TTL backstop.
func New(store *script.Store, dataDir string, ttl time.Duration) *Runner {
	return &Runner{
		store:         store,
		dataDir:       dataDir,
		ttl:           ttl,
		cancels:       make(map[string]context.CancelFunc),
		userCancelled: make(map[string]bool),
	}
}

// runDir is the rebuildable per-run execution tree: <dataDir>/runs/<run_id>/.
func (r *Runner) runDir(runID string) string {
	return filepath.Join(r.dataDir, "runs", runID)
}

// Spawn starts a goroutine and returns immediately. input is the event bytes
// ("{}" for a manual run). The goroutine owns the full lifecycle (A6 steps).
func (r *Runner) Spawn(run script.Run, input []byte) {
	go r.execute(run, input)
}

// execute is the A6 goroutine: it fetches the script, materializes the run dir,
// execs python3 under a TTL/cancel context with its own process group, then
// writes the terminal state (and completion event) via store.FinishRun.
func (r *Runner) execute(run script.Run, input []byte) {
	ctx, cancel := context.WithTimeout(context.Background(), r.ttl)

	// Register the cancel func keyed by run_id so Cancel can find it; deregister
	// (and clear the user-cancelled flag) on completion.
	r.mu.Lock()
	r.cancels[run.ID] = cancel
	r.mu.Unlock()
	defer func() {
		cancel()
		r.mu.Lock()
		delete(r.cancels, run.ID)
		delete(r.userCancelled, run.ID)
		r.mu.Unlock()
	}()

	// Persistence uses a fresh background context: the run ctx may be
	// cancelled/expired by the time we write the terminal state.
	bg := context.Background()
	endedAt := func() string { return time.Now().UTC().Format(time.RFC3339Nano) }

	// finish writes the terminal state and (for succeeded|failed) emits the
	// completion event atomically. ScriptName + flat trigger ctx ride along for
	// the payload; tails/trunc are read from the on-disk logs.
	dir := r.runDir(run.ID)
	finish := func(scriptName, status string, exitCode *int, errMsg string) {
		stdoutTail, stdoutTrunc := readTail(filepath.Join(dir, "stdout.log"))
		stderrTail, stderrTrunc := readTail(filepath.Join(dir, "stderr.log"))
		_ = r.store.FinishRun(bg, script.FinishRunInput{
			RunID:          run.ID,
			ScriptID:       run.ScriptID,
			ScriptName:     scriptName,
			Status:         status,
			ExitCode:       exitCode,
			EndedAt:        endedAt(),
			ErrMsg:         errMsg,
			TriggerSource:  run.TriggerSource,
			TriggerType:    run.TriggerType,
			TriggerEventID: run.TriggerEventID,
			StdoutTail:     stdoutTail,
			StderrTail:     stderrTail,
			StdoutTrunc:    stdoutTrunc,
			StderrTrunc:    stderrTrunc,
		})
	}

	// Fetch the executed source/config/name. A tombstoned or missing script
	// (run_id may dangle, §7A) fails the run without materializing.
	sc, err := r.store.ScriptForRun(ctx, run.ScriptID)
	if err != nil {
		if errors.Is(err, script.ErrNotFound) {
			finish("", script.RunFailed, nil, "script not found (deleted before run)")
		} else {
			finish("", script.RunFailed, nil, "load script: "+err.Error())
		}
		return
	}

	// Create the persistent run dir (0700) and materialize body + pinned config.
	if err := os.MkdirAll(dir, 0o700); err != nil {
		finish(sc.Name, script.RunFailed, nil, "create run dir: "+err.Error())
		return
	}
	if err := os.WriteFile(filepath.Join(dir, "main.py"), []byte(sc.Body), 0o600); err != nil {
		finish(sc.Name, script.RunFailed, nil, "write main.py: "+err.Error())
		return
	}
	cfgJSON, err := json.Marshal(sc.Config)
	if err != nil {
		finish(sc.Name, script.RunFailed, nil, "marshal config: "+err.Error())
		return
	}
	if err := os.WriteFile(filepath.Join(dir, "config.json"), cfgJSON, 0o600); err != nil {
		finish(sc.Name, script.RunFailed, nil, "write config.json: "+err.Error())
		return
	}

	// Open the captured streams in the run dir.
	stdoutFile, err := os.OpenFile(filepath.Join(dir, "stdout.log"), os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0o600)
	if err != nil {
		finish(sc.Name, script.RunFailed, nil, "open stdout.log: "+err.Error())
		return
	}
	defer stdoutFile.Close()
	stderrFile, err := os.OpenFile(filepath.Join(dir, "stderr.log"), os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0o600)
	if err != nil {
		finish(sc.Name, script.RunFailed, nil, "open stderr.log: "+err.Error())
		return
	}
	defer stderrFile.Close()

	// Build the command: python3 main.py from the run dir, the event payload on
	// $EVENT_JSON and stdin, streams to the log files. Setpgid puts the child in
	// its own process group so a TTL/cancel kills the whole tree.
	cmd := exec.CommandContext(ctx, python3, "main.py")
	cmd.Dir = dir
	cmd.Env = append(os.Environ(), "EVENT_JSON="+string(input))
	cmd.Stdin = bytes.NewReader(input)
	cmd.Stdout = stdoutFile
	cmd.Stderr = stderrFile
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	// On ctx cancel/timeout, kill the whole process group (negative pgid), not
	// just the parent, so child processes the script spawned die too.
	cmd.Cancel = func() error {
		if cmd.Process == nil {
			return nil
		}
		return syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)
	}

	runErr := cmd.Run()

	// Classify the terminal status: explicit user cancel wins over TTL, TTL over
	// a non-zero/spawn error, and a clean return is success.
	r.mu.Lock()
	userCancelled := r.userCancelled[run.ID]
	r.mu.Unlock()

	switch {
	case userCancelled:
		// Operator abort: terminal cancelled, store emits NO event.
		finish(sc.Name, script.RunCancelled, nil, "cancelled")
	case ctx.Err() == context.DeadlineExceeded:
		finish(sc.Name, script.RunFailed, nil, "run TTL exceeded")
	case runErr == nil:
		code := 0
		finish(sc.Name, script.RunSucceeded, &code, "")
	default:
		// Non-zero exit carries the exit code; a spawn error (no ExitCode) does not.
		var exitErr *exec.ExitError
		if errors.As(runErr, &exitErr) {
			code := exitErr.ExitCode()
			finish(sc.Name, script.RunFailed, &code, runErr.Error())
		} else {
			finish(sc.Name, script.RunFailed, nil, runErr.Error())
		}
	}
}

// Cancel marks run_id user-cancelled and cancels its context (kills the process
// group). Returns false if the run_id is not in flight. Terminal status =
// cancelled, NO event.
func (r *Runner) Cancel(runID string) bool {
	r.mu.Lock()
	cancel, ok := r.cancels[runID]
	if ok {
		r.userCancelled[runID] = true
	}
	r.mu.Unlock()
	if !ok {
		return false
	}
	cancel()
	return true
}

// Recover sweeps running->failed on boot (delegates SweepRunning). Returns the
// count swept. It does NOT delete run dirs — they are persistent history; a
// crashed run keeps whatever partial tree it had.
func (r *Runner) Recover(ctx context.Context) (int, error) {
	ids, err := r.store.SweepRunning(ctx)
	if err != nil {
		return 0, err
	}
	return len(ids), nil
}

// readTail returns the last tailMax bytes of the file and whether it was
// truncated (file larger than tailMax). A missing/unreadable file yields "",
// false — a best-effort read for the completion payload.
func readTail(path string) (string, bool) {
	info, err := os.Stat(path)
	if err != nil {
		return "", false
	}
	size := info.Size()
	if size <= tailMax {
		b, err := os.ReadFile(path)
		if err != nil {
			return "", false
		}
		return string(b), false
	}
	f, err := os.Open(path)
	if err != nil {
		return "", false
	}
	defer f.Close()
	b := make([]byte, tailMax)
	if _, err := f.ReadAt(b, size-tailMax); err != nil {
		return "", false
	}
	return string(b), true
}
