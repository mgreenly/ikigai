package script

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"scripts/internal/ids"
)

// Runner is the run-lifecycle surface the Service depends on (runner.Runner
// satisfies it). The Service depends on the interface, not the concrete runner,
// so the two packages stay decoupled.
type Runner interface {
	Spawn(run Run, input []byte)
	Cancel(runID string) bool
}

// Service is scripts' domain service — the only thing the MCP handler talks to
// and the only mutator of script/run/trigger state. It owns CRUD, the run
// lifecycle entry points, run-instance reads, and trigger ops.
type Service struct {
	store   *Store
	runner  Runner
	dataDir string // runs dir root: a run's persisted tree is <dataDir>/<run_id>/
	now     func() time.Time
}

// NewService wires the store, the runs dir root, and the runner.
//
// runsDir is the directory that directly contains the per-run trees, i.e. the
// value cmd/scripts passes is filepath.Join(<dataDir>, "runs"). A run's
// persisted dir is therefore filepath.Join(runsDir, run_id). The runner is
// constructed with the parent <dataDir> and joins "runs" itself
// (runner.runDir = filepath.Join(dataDir, "runs", run_id)), so both the service
// and the runner resolve to the identical <dataDir>/runs/<run_id>/ tree.
func NewService(store *Store, runsDir string, runner Runner) *Service {
	return &Service{
		store:   store,
		runner:  runner,
		dataDir: runsDir,
		now:     func() time.Time { return time.Now().UTC() },
	}
}

func (s *Service) nowStr() string { return s.now().UTC().Format(time.RFC3339Nano) }

// runDir is the persisted per-run tree: <runsDir>/<run_id>/. dataDir already
// includes the "runs" segment (see NewService), so we join only the run id —
// matching runner.runDir which joins ("runs", run_id) onto its parent dataDir.
func (s *Service) runDir(runID string) string {
	return filepath.Join(s.dataDir, runID)
}

// CreateInput / UpdateInput are the MCP-facing mutation payloads.
type CreateInput struct {
	Name, Body string
	Config     Config
}

type UpdateInput struct {
	Name, Body *string
	Config     *Config
}

// --- CRUD (owner-scoped) ---

// Create validates name/body, generates an id, defaults the config, and inserts
// the script.
func (s *Service) Create(ctx context.Context, owner string, in CreateInput) (Script, error) {
	if strings.TrimSpace(in.Name) == "" {
		return Script{}, fmt.Errorf("%w: name is required", ErrValidation)
	}
	if strings.TrimSpace(in.Body) == "" {
		return Script{}, fmt.Errorf("%w: body is required", ErrValidation)
	}
	cfg := in.Config
	if cfg.Interpreter == "" {
		cfg.Interpreter = "python3"
	}
	now := s.nowStr()
	sc := Script{
		ID:         ids.NewULID(),
		OwnerEmail: owner,
		Name:       in.Name,
		Body:       in.Body,
		Config:     cfg,
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := s.store.InsertScript(ctx, sc); err != nil {
		return Script{}, err
	}
	return sc, nil
}

// Update applies the optional Name/Body/Config pointers (nil = leave as-is) and
// bumps updated_at. Owner-scoped: a missing/foreign script reads as ErrNotFound.
func (s *Service) Update(ctx context.Context, owner, id string, in UpdateInput) (Script, error) {
	sc, err := s.store.GetScript(ctx, owner, id)
	if err != nil {
		return Script{}, err
	}
	if in.Name != nil {
		if strings.TrimSpace(*in.Name) == "" {
			return Script{}, fmt.Errorf("%w: name cannot be empty", ErrValidation)
		}
		sc.Name = *in.Name
	}
	if in.Body != nil {
		if strings.TrimSpace(*in.Body) == "" {
			return Script{}, fmt.Errorf("%w: body cannot be empty", ErrValidation)
		}
		sc.Body = *in.Body
	}
	if in.Config != nil {
		cfg := *in.Config
		if cfg.Interpreter == "" {
			cfg.Interpreter = "python3"
		}
		sc.Config = cfg
	}
	sc.UpdatedAt = s.nowStr()
	if err := s.store.UpdateScript(ctx, owner, sc); err != nil {
		return Script{}, err
	}
	return sc, nil
}

// Delete tombstones the owner's script (the store removes the row + its
// triggers; runs and on-disk artifacts survive as history).
func (s *Service) Delete(ctx context.Context, owner, id string) error {
	return s.store.DeleteScript(ctx, owner, id)
}

// List returns each script with derived RunningCount + LastRun.
func (s *Service) List(ctx context.Context, owner string) ([]ScriptDetail, error) {
	scripts, err := s.store.ListScripts(ctx, owner)
	if err != nil {
		return nil, err
	}
	out := make([]ScriptDetail, 0, len(scripts))
	for _, sc := range scripts {
		detail, err := s.detail(ctx, sc)
		if err != nil {
			return nil, err
		}
		out = append(out, detail)
	}
	return out, nil
}

// Get returns the owner's script with derived RunningCount + LastRun.
func (s *Service) Get(ctx context.Context, owner, id string) (ScriptDetail, error) {
	sc, err := s.store.GetScript(ctx, owner, id)
	if err != nil {
		return ScriptDetail{}, err
	}
	return s.detail(ctx, sc)
}

// detail attaches RunningCount + LastRun (with ElapsedSecs) to a script.
func (s *Service) detail(ctx context.Context, sc Script) (ScriptDetail, error) {
	count, err := s.store.RunningCount(ctx, sc.ID)
	if err != nil {
		return ScriptDetail{}, err
	}
	last, err := s.store.LastRun(ctx, sc.ID)
	if err != nil {
		return ScriptDetail{}, err
	}
	if last != nil {
		withElapsed := *last
		withElapsed.ElapsedSecs = elapsedSecs(withElapsed)
		last = &withElapsed
	}
	return ScriptDetail{Script: sc, RunningCount: count, LastRun: last}, nil
}

// --- Run lifecycle ---

// Run inserts a running run (empty trigger = manual), spawns it with "{}" as the
// event payload, and returns the run. ALWAYS accepted (no single-flight gate;
// scripts allows full concurrency).
func (s *Service) Run(ctx context.Context, owner, scriptID string) (Run, error) {
	if _, err := s.store.GetScript(ctx, owner, scriptID); err != nil {
		return Run{}, err
	}
	run := s.newRun(scriptID, "", "", "")
	if err := s.store.InsertRun(ctx, run); err != nil {
		return Run{}, err
	}
	s.runner.Spawn(run, []byte("{}"))
	return run, nil
}

// RunForEvent is the consumer path; NOT owner-scoped (the trigger linkage, set
// by the owner, is the authority — a fired event carries no caller identity). It
// inserts a run carrying the trigger context and spawns it with the event
// payload as stdin/$EVENT_JSON. A missing/tombstoned script id is treated as a
// no-op (returns nil): the consumer is fire-and-forget and never stalls; a
// dangling trigger that outraced a delete simply fires nothing.
func (s *Service) RunForEvent(ctx context.Context, scriptID, source, evType, eventID string, payload []byte) error {
	if _, err := s.store.ScriptForRun(ctx, scriptID); err != nil {
		// ErrNotFound (tombstoned/dangling id) → nothing to fire; consumer treats
		// nil as handled. Any other error propagates.
		if err == ErrNotFound {
			return nil
		}
		return err
	}
	run := s.newRun(scriptID, source, evType, eventID)
	if err := s.store.InsertRun(ctx, run); err != nil {
		return err
	}
	s.runner.Spawn(run, payload)
	return nil
}

// newRun builds a running run row with the §A4 log paths. Empty trigger fields
// mark a manual run.
func (s *Service) newRun(scriptID, source, evType, eventID string) Run {
	runID := ids.NewULID()
	return Run{
		ID:             runID,
		ScriptID:       scriptID,
		Status:         RunRunning,
		StartedAt:      s.nowStr(),
		TriggerSource:  source,
		TriggerType:    evType,
		TriggerEventID: eventID,
		StdoutPath:     filepath.Join("runs", runID, "stdout.log"),
		StderrPath:     filepath.Join("runs", runID, "stderr.log"),
	}
}

// --- Run-instance reads ---

// RunList returns the owner's runs (each with ElapsedSecs). scriptID/status "" =
// no filter on that dimension.
func (s *Service) RunList(ctx context.Context, owner, scriptID, status string) ([]Run, error) {
	runs, err := s.store.ListRuns(ctx, owner, scriptID, status)
	if err != nil {
		return nil, err
	}
	for i := range runs {
		runs[i].ElapsedSecs = elapsedSecs(runs[i])
	}
	return runs, nil
}

// RunGet returns one owner-scoped run (with ElapsedSecs).
func (s *Service) RunGet(ctx context.Context, owner, runID string) (Run, error) {
	run, err := s.store.GetRun(ctx, owner, runID)
	if err != nil {
		return Run{}, err
	}
	run.ElapsedSecs = elapsedSecs(run)
	return run, nil
}

// RunOutput returns a line-slice [offset, offset+limit) of the persisted run
// logs. stream is "stdout" | "stderr" | "both". offset/limit are in LINES
// (offset<=0 → from start, limit<=0 → no limit). Owner scope is enforced via
// GetRun before any file is touched.
func (s *Service) RunOutput(ctx context.Context, owner, runID, stream string, offset, limit int) (string, error) {
	if _, err := s.store.GetRun(ctx, owner, runID); err != nil {
		return "", err
	}
	dir := s.runDir(runID)
	switch stream {
	case "stdout":
		return readLines(filepath.Join(dir, "stdout.log"), offset, limit)
	case "stderr":
		return readLines(filepath.Join(dir, "stderr.log"), offset, limit)
	case "both", "":
		out, err := readLines(filepath.Join(dir, "stdout.log"), offset, limit)
		if err != nil {
			return "", err
		}
		errOut, err := readLines(filepath.Join(dir, "stderr.log"), offset, limit)
		if err != nil {
			return "", err
		}
		var b strings.Builder
		b.WriteString("=== stdout ===\n")
		b.WriteString(out)
		b.WriteString("=== stderr ===\n")
		b.WriteString(errOut)
		return b.String(), nil
	default:
		return "", fmt.Errorf("%w: unknown stream %q (want stdout|stderr|both)", ErrValidation, stream)
	}
}

// RunCancel verifies ownership, then signals the in-flight run by id. If the run
// is not in flight (already terminal or never started), Cancel returns false and
// we return ErrValidation: there is nothing to cancel. Owner scope is enforced
// via GetRun (ErrNotFound for a missing/foreign run).
func (s *Service) RunCancel(ctx context.Context, owner, runID string) error {
	run, err := s.store.GetRun(ctx, owner, runID)
	if err != nil {
		return err
	}
	if run.Status != RunRunning {
		return fmt.Errorf("%w: run %s is already terminal (%s)", ErrValidation, runID, run.Status)
	}
	if !s.runner.Cancel(runID) {
		// Owner-scoped but not in flight on this process (raced its own
		// completion, or swept). Nothing to cancel.
		return fmt.Errorf("%w: run %s is not in flight", ErrValidation, runID)
	}
	return nil
}

// RunFsList lists the run's persisted dir tree at subpath (run-scoped;
// owner-verified). subpath is relative to the run dir root; entries' Path is
// returned relative to that root. Path traversal is rejected: subpath is
// cleaned and must resolve within the run dir.
func (s *Service) RunFsList(ctx context.Context, owner, runID, subpath string) ([]FileEntry, error) {
	if _, err := s.store.GetRun(ctx, owner, runID); err != nil {
		return nil, err
	}
	root := s.runDir(runID)
	target, err := resolveWithin(root, subpath)
	if err != nil {
		return nil, err
	}
	entries, err := os.ReadDir(target)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, fmt.Errorf("%w: path %q not found in run", ErrNotFound, subpath)
		}
		return nil, fmt.Errorf("script: run fs list: %w", err)
	}
	out := make([]FileEntry, 0, len(entries))
	for _, e := range entries {
		info, err := e.Info()
		if err != nil {
			return nil, fmt.Errorf("script: run fs stat: %w", err)
		}
		rel, err := filepath.Rel(root, filepath.Join(target, e.Name()))
		if err != nil {
			return nil, fmt.Errorf("script: run fs rel: %w", err)
		}
		fe := FileEntry{Path: rel, IsDir: e.IsDir()}
		if !e.IsDir() {
			fe.Size = info.Size()
		}
		out = append(out, fe)
	}
	return out, nil
}

// RunFsRead returns a line-slice of one file inside the run dir (owner-verified,
// path-traversal guarded to the run root). offset/limit are in LINES (offset<=0
// → from start, limit<=0 → no limit) — consistent with RunOutput.
func (s *Service) RunFsRead(ctx context.Context, owner, runID, path string, offset, limit int) (string, error) {
	if _, err := s.store.GetRun(ctx, owner, runID); err != nil {
		return "", err
	}
	root := s.runDir(runID)
	target, err := resolveWithin(root, path)
	if err != nil {
		return "", err
	}
	return readLines(target, offset, limit)
}

// --- Trigger ops ---

// SetTrigger validates (source, event_filter) against the known producers, then
// upserts the trigger on the owner's script. Owner scope + ErrNotFound is
// enforced by the store; validation rejects an unsatisfiable binding with
// ErrValidation.
func (s *Service) SetTrigger(ctx context.Context, owner, scriptID, source, eventFilter string) (Trigger, error) {
	if _, err := s.store.GetScript(ctx, owner, scriptID); err != nil {
		return Trigger{}, err
	}
	if err := validateTrigger(source, eventFilter); err != nil {
		return Trigger{}, err
	}
	t := Trigger{
		ScriptID:    scriptID,
		Source:      source,
		EventFilter: eventFilter,
		CreatedAt:   s.nowStr(),
	}
	if err := s.store.SetTrigger(ctx, owner, t); err != nil {
		return Trigger{}, err
	}
	return t, nil
}

// ClearTrigger removes a trigger from the owner's script (ErrNotFound if the
// script is missing/foreign).
func (s *Service) ClearTrigger(ctx context.Context, owner, scriptID, source, eventFilter string) error {
	return s.store.ClearTrigger(ctx, owner, Trigger{
		ScriptID:    scriptID,
		Source:      source,
		EventFilter: eventFilter,
	})
}

// ScriptsForEvent delegates to the store; the consumer uses it for fan-out.
func (s *Service) ScriptsForEvent(ctx context.Context, source, evType string) ([]string, error) {
	return s.store.ScriptsForEvent(ctx, source, evType)
}

// TriggerSources returns the static known-producer set, for set_trigger
// validation.
func (s *Service) TriggerSources() []string {
	return triggerSources()
}

// --- helpers ---

// elapsedSecs computes wall-clock seconds for a run. A running run measures
// against now; a terminal run measures StartedAt→EndedAt. Returns 0 if the
// timestamps cannot be parsed.
func elapsedSecs(r Run) int {
	start, err := time.Parse(time.RFC3339Nano, r.StartedAt)
	if err != nil {
		return 0
	}
	end := time.Now().UTC()
	if r.EndedAt != "" {
		if parsed, err := time.Parse(time.RFC3339Nano, r.EndedAt); err == nil {
			end = parsed
		}
	}
	secs := int(end.Sub(start).Seconds())
	if secs < 0 {
		return 0
	}
	return secs
}

// resolveWithin resolves rel against root and rejects any path that tries to
// escape the run dir (path-traversal guard). An empty rel resolves to root
// itself. A rel that contains a ".." element, or whose cleaned form lands
// outside root, is rejected with ErrValidation.
func resolveWithin(root, rel string) (string, error) {
	// Reject absolute inputs and any explicit parent-dir traversal outright.
	if filepath.IsAbs(rel) {
		return "", fmt.Errorf("%w: path %q must be relative to the run dir", ErrValidation, rel)
	}
	for _, seg := range strings.Split(filepath.ToSlash(rel), "/") {
		if seg == ".." {
			return "", fmt.Errorf("%w: path %q escapes the run dir", ErrValidation, rel)
		}
	}
	target := filepath.Join(root, filepath.Clean(rel))
	// Defense in depth: confirm target is root or a descendant of root.
	if target != root {
		prefix := root + string(os.PathSeparator)
		if !strings.HasPrefix(target, prefix) {
			return "", fmt.Errorf("%w: path %q escapes the run dir", ErrValidation, rel)
		}
	}
	return target, nil
}

// readLines reads up to limit lines of the file at path starting at 1-based
// offset (offset<=0 → from start, limit<=0 → no limit). A missing file reads as
// empty (a run dir may not yet hold a log).
func readLines(path string, offset, limit int) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			return "", nil
		}
		return "", fmt.Errorf("script: open %s: %w", filepath.Base(path), err)
	}
	defer f.Close()

	if offset <= 0 {
		offset = 1
	}
	var b strings.Builder
	sc := bufio.NewScanner(f)
	sc.Buffer(make([]byte, 0, 64*1024), 16*1024*1024)
	lineNo := 0
	written := 0
	for sc.Scan() {
		lineNo++
		if lineNo < offset {
			continue
		}
		if limit > 0 && written >= limit {
			break
		}
		b.WriteString(sc.Text())
		b.WriteByte('\n')
		written++
	}
	if err := sc.Err(); err != nil {
		return "", fmt.Errorf("script: read %s: %w", filepath.Base(path), err)
	}
	return b.String(), nil
}
