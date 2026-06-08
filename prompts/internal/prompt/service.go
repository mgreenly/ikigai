package prompt

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"agentkit/model"
	"prompts/internal/ids"
	"prompts/internal/sandbox"
)

// ValidationError wraps a config-validation failure (bad model, wrong
// provider, missing key). Distinct so the MCP layer can map it to an
// invalid-params response rather than an internal error.
type ValidationError struct{ msg string }

func (e *ValidationError) Error() string { return e.msg }

func validationErrf(format string, a ...any) *ValidationError {
	return &ValidationError{msg: fmt.Sprintf(format, a...)}
}

// Runner drives a run's async lifecycle. Spawn returns immediately; the
// runner writes the run's terminal state on its own. Cancel signals an
// in-flight run by run_id; returns whether one was found.
type Runner interface {
	Spawn(run Run)
	Cancel(runID string) bool
}

// Service is prompts' domain service — the only mutator of prompt/run state.
// Runs are fully concurrent: there is no prompt status and no single-flight
// gate. The MCP handler talks only to it.
type Service struct {
	store   *Store
	sandbox *sandbox.Manager
	runsDir string
	runner  Runner
	now     func() time.Time
}

// NewService wires the store, sandbox manager, run-logs base dir, and runner.
func NewService(store *Store, sb *sandbox.Manager, runsDir string, runner Runner) *Service {
	return &Service{
		store:   store,
		sandbox: sb,
		runsDir: runsDir,
		runner:  runner,
		now:     func() time.Time { return time.Now().UTC() },
	}
}

func (s *Service) nowStr() string { return s.now().UTC().Format(time.RFC3339Nano) }

// CreateInput is the create payload. Triggers is the optional create-time sugar:
// each (source, event_filter) binding is applied via SetTrigger after the prompt
// row is inserted (same validation; an invalid one rejects the whole create).
type CreateInput struct {
	Name         string
	UserPrompt   string
	SystemPrompt string
	Config       Config
	Triggers     []TriggerSpec
}

// UpdateInput is the update payload (all fields replace).
type UpdateInput struct {
	Name         string
	UserPrompt   string
	SystemPrompt string
	Config       Config
}

// validateConfig enforces the §5.1 rules: the model resolves, its provider is
// anthropic, and ANTHROPIC_API_KEY is present (validated per prompt).
func validateConfig(c Config) error {
	resolved, err := model.Resolve(c.Model)
	if err != nil {
		return validationErrf("invalid config: %v", err)
	}
	if resolved.Provider != model.ProviderAnthropic {
		return validationErrf("invalid config: model %q resolves to provider %q; only anthropic is supported", c.Model, resolved.Provider)
	}
	if os.Getenv("ANTHROPIC_API_KEY") == "" {
		return validationErrf("invalid config: ANTHROPIC_API_KEY is not set")
	}
	return nil
}

// Create validates config and inserts a prompt. The sandbox is no longer
// created here — it is per-run, created at Run time keyed by run_id.
// Config.Provider is normalized to "anthropic".
func (s *Service) Create(ctx context.Context, ownerEmail string, in CreateInput) (Prompt, error) {
	if err := validateConfig(in.Config); err != nil {
		return Prompt{}, err
	}
	// Validate every inline trigger BEFORE inserting the prompt, so an invalid
	// binding rejects the whole create without leaving a triggerless orphan.
	for _, ts := range in.Triggers {
		if err := validateTrigger(ts.Source, ts.EventFilter); err != nil {
			return Prompt{}, err
		}
	}
	cfg := in.Config
	cfg.Provider = string(model.ProviderAnthropic)

	now := s.nowStr()
	p := Prompt{
		ID:           ids.NewULID(),
		OwnerEmail:   ownerEmail,
		Name:         in.Name,
		UserPrompt:   in.UserPrompt,
		SystemPrompt: in.SystemPrompt,
		Config:       cfg,
		CreatedAt:    now,
		UpdatedAt:    now,
	}
	if err := s.store.InsertPrompt(ctx, p); err != nil {
		return Prompt{}, err
	}
	// Apply the inline triggers (already validated) via the store SetTrigger.
	for _, ts := range in.Triggers {
		if err := s.store.SetTrigger(ctx, Trigger{
			PromptID:    p.ID,
			Source:      ts.Source,
			EventFilter: ts.EventFilter,
			CreatedAt:   now,
		}); err != nil {
			return Prompt{}, err
		}
	}
	return p, nil
}

// List returns the owner's prompts, each as a PromptDetail carrying its derived
// RunningCount + LastRun.
func (s *Service) List(ctx context.Context, ownerEmail string) ([]PromptDetail, error) {
	prompts, err := s.store.ListPrompts(ctx, ownerEmail)
	if err != nil {
		return nil, err
	}
	out := make([]PromptDetail, 0, len(prompts))
	for _, p := range prompts {
		d, err := s.detail(ctx, p)
		if err != nil {
			return nil, err
		}
		out = append(out, d)
	}
	return out, nil
}

// Get returns the owner's prompt as a PromptDetail (RunningCount + LastRun).
func (s *Service) Get(ctx context.Context, ownerEmail, id string) (PromptDetail, error) {
	p, err := s.store.GetPrompt(ctx, ownerEmail, id)
	if err != nil {
		return PromptDetail{}, err
	}
	return s.detail(ctx, p)
}

// detail enriches a Prompt with its derived run summary (RunningCount +
// LastRun) — the shared body of Get/List.
func (s *Service) detail(ctx context.Context, p Prompt) (PromptDetail, error) {
	running, err := s.store.RunningCount(ctx, p.ID)
	if err != nil {
		return PromptDetail{}, err
	}
	last, err := s.store.GetLatestRun(ctx, p.ID)
	if err != nil {
		return PromptDetail{}, err
	}
	return PromptDetail{Prompt: p, RunningCount: running, LastRun: last}, nil
}

// Update edits name/user_prompt/system_prompt/config. ALWAYS allowed (no
// single-flight); re-validates config.
func (s *Service) Update(ctx context.Context, ownerEmail, id string, in UpdateInput) (Prompt, error) {
	p, err := s.store.GetPrompt(ctx, ownerEmail, id)
	if err != nil {
		return Prompt{}, err
	}
	if err := validateConfig(in.Config); err != nil {
		return Prompt{}, err
	}
	cfg := in.Config
	cfg.Provider = string(model.ProviderAnthropic)

	p.Name = in.Name
	p.UserPrompt = in.UserPrompt
	p.SystemPrompt = in.SystemPrompt
	p.Config = cfg
	p.UpdatedAt = s.nowStr()

	if err := s.store.UpdatePrompt(ctx, ownerEmail, p); err != nil {
		return Prompt{}, err
	}
	return p, nil
}

// Delete is a TOMBSTONE: it removes ONLY the prompt row and the prompt's
// trigger(s). It deliberately leaves the prompt's runs, their on-disk run
// directories (output.jsonl, input/, sandbox/), and any emitted outbox rows in
// place — a run stays owner-addressable by run_id (via the run's denormalized
// owner_email) after its prompt is gone. ALWAYS allowed (no single-flight).
func (s *Service) Delete(ctx context.Context, ownerEmail, id string) error {
	if _, err := s.store.GetPrompt(ctx, ownerEmail, id); err != nil {
		return err
	}
	// Row first (owner-scoped): if this fails nothing else is touched.
	if err := s.store.DeletePrompt(ctx, ownerEmail, id); err != nil {
		return err
	}
	// Remove the prompt's trigger(s) explicitly — there is no FK cascade.
	if err := s.store.DeleteTriggers(ctx, id); err != nil {
		return err
	}
	return nil
}

// Run starts a run for the owner's prompt. It is ALWAYS accepted — there is no
// single-flight gate, so any number of runs of one prompt may execute
// concurrently, each in its own run-scoped sandbox (keyed by run_id).
func (s *Service) Run(ctx context.Context, ownerEmail, id string) (Run, error) {
	p, err := s.store.GetPrompt(ctx, ownerEmail, id)
	if err != nil {
		return Run{}, err
	}
	return s.startRun(ctx, p, "", "", "")
}

// materializeInput pins the prompt's changeable execution inputs to
// runs/<run_id>/input/ BEFORE spawn: user_prompt.txt, system_prompt.txt (empty
// file when none), and config.json (the resolved Config). Once written, this is
// the record of exactly what the run executes — the runner reads from here, so
// editing or deleting the prompt mid-run cannot change what the run runs.
func (s *Service) materializeInput(run Run, p Prompt) error {
	inputDir := filepath.Join(s.runsDir, run.ID, "input")
	if err := os.MkdirAll(inputDir, 0o755); err != nil {
		return fmt.Errorf("prompt: create run input dir: %w", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "user_prompt.txt"), []byte(p.UserPrompt), 0o644); err != nil {
		return fmt.Errorf("prompt: write user_prompt: %w", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "system_prompt.txt"), []byte(p.SystemPrompt), 0o644); err != nil {
		return fmt.Errorf("prompt: write system_prompt: %w", err)
	}
	cfgJSON, err := json.Marshal(p.Config)
	if err != nil {
		return fmt.Errorf("prompt: marshal run config: %w", err)
	}
	if err := os.WriteFile(filepath.Join(inputDir, "config.json"), cfgJSON, 0o644); err != nil {
		return fmt.Errorf("prompt: write config: %w", err)
	}
	return nil
}

// startRun is the shared run-start path for Run (manual) and RunByEvent (event).
// It builds the run row (denormalizing owner_email / prompt_name and the
// trigger context), materializes input/ from the prompt's CURRENT definition,
// inserts the row, creates the run-scoped sandbox, and hands off to the runner
// — which executes from disk, never from p.
func (s *Service) startRun(ctx context.Context, p Prompt, source, evType, eventID string) (Run, error) {
	runID := ids.NewULID()
	run := Run{
		ID:             runID,
		PromptID:       p.ID,
		OwnerEmail:     p.OwnerEmail,
		PromptName:     p.Name,
		Status:         RunRunning,
		StartedAt:      s.nowStr(),
		LogPath:        filepath.Join(s.runsDir, runID, "output.jsonl"),
		TriggerSource:  source,
		TriggerType:    evType,
		TriggerEventID: eventID,
	}
	// Pin the execution inputs to disk BEFORE the row exists / spawn happens.
	if err := s.materializeInput(run, p); err != nil {
		return Run{}, err
	}
	if err := s.store.InsertRun(ctx, run); err != nil {
		return Run{}, err
	}
	// Create the run-scoped sandbox (runs/<run_id>/sandbox) before spawn.
	if err := s.sandbox.Create(runID); err != nil {
		return Run{}, fmt.Errorf("prompt: create run sandbox: %w", err)
	}
	s.runner.Spawn(run)
	return run, nil
}

// RunByEvent is the event-triggered run path: it starts a run for a prompt by id
// WITHOUT owner scoping (the trigger linkage, set by the owner, is the
// authority — there is no caller identity on an event). It otherwise mirrors
// Run: ALWAYS accepted (no single-flight), inserts the run row, creates the
// run-scoped sandbox, and hands off to the runner. ErrNotFound if the prompt
// is gone.
//
// source / evType / eventID are the trigger context (the upstream producer, the
// fired event type, and the upstream event id that fired this run); they are
// persisted on the Run row and FinishRun reads them back to populate the
// run.succeeded / run.failed outcome payload. All empty for a manual run (see
// Run, which passes "" / "" / ""). payload is reserved for future inputs to the
// run (e.g. exposing the upstream event body); currently unused.
func (s *Service) RunByEvent(ctx context.Context, id, source, evType, eventID string, payload []byte) (Run, error) {
	p, err := s.store.GetPromptByID(ctx, id)
	if err != nil {
		return Run{}, err
	}
	return s.startRun(ctx, p, source, evType, eventID)
}

// readLines reads up to limit lines of the file at path starting at 1-based
// offset, mirroring sandbox.Read's line-slice semantics (offset<=0 → from
// start, limit<=0 → no limit).
func readLines(path string, offset, limit int) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			return "", fmt.Errorf("prompt: run log not found")
		}
		return "", fmt.Errorf("prompt: open run log: %w", err)
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
		return "", fmt.Errorf("prompt: read run log: %w", err)
	}
	return b.String(), nil
}

// --- First-class, run_id-addressed run readers (A7) ---
//
// These are owner-scoped via the RUN's denormalized owner_email, not via its
// prompt — so a run remains readable after its prompt has been tombstone-
// deleted. (RunList is the exception: it lists a prompt's runs and so scopes via
// the prompt.)

// runForOwner loads a run by run_id and enforces that it belongs to the caller
// via the run's denormalized owner_email. ErrNotFound when the run is missing or
// owned by another caller — identical to the prompt-not-owned semantics, so a
// foreign run is indistinguishable from an absent one.
func (s *Service) runForOwner(ctx context.Context, ownerEmail, runID string) (Run, error) {
	r, err := s.store.GetRun(ctx, runID)
	if err != nil {
		return Run{}, err
	}
	if r.OwnerEmail != ownerEmail {
		return Run{}, ErrNotFound
	}
	return r, nil
}

// RunList returns the owner's prompt's runs, newest first. It scopes via the
// PROMPT (loading it enforces ownership); ErrNotFound if the prompt is missing
// or not owned.
func (s *Service) RunList(ctx context.Context, ownerEmail, promptID string) ([]Run, error) {
	if _, err := s.store.GetPrompt(ctx, ownerEmail, promptID); err != nil {
		return nil, err
	}
	return s.store.ListRunsByPrompt(ctx, promptID)
}

// RunGet returns a single run by run_id, owner-scoped via the run's owner_email
// (works after the run's prompt has been tombstoned).
func (s *Service) RunGet(ctx context.Context, ownerEmail, runID string) (Run, error) {
	return s.runForOwner(ctx, ownerEmail, runID)
}

// RunOutput returns up to limit lines of a run's output.jsonl, starting at
// 1-based offset (same line-slice semantics as sandbox.Read). Owner-scoped via
// the run's owner_email; survives a tombstoned prompt.
func (s *Service) RunOutput(ctx context.Context, ownerEmail, runID string, offset, limit int) (string, error) {
	r, err := s.runForOwner(ctx, ownerEmail, runID)
	if err != nil {
		return "", err
	}
	return readLines(r.LogPath, offset, limit)
}

// RunCancel signals the in-flight run by run_id. Owner-scoped via the run's
// owner_email. Idempotent: a run that is not in flight is not an error (the
// foreground may race the run's own completion).
func (s *Service) RunCancel(ctx context.Context, ownerEmail, runID string) error {
	r, err := s.runForOwner(ctx, ownerEmail, runID)
	if err != nil {
		return err
	}
	s.runner.Cancel(r.ID)
	return nil
}

// RunFsList lists the entries under path in a run's sandbox (runs/<run_id>/
// sandbox). Owner-scoped via the run's owner_email; survives a tombstoned prompt.
func (s *Service) RunFsList(ctx context.Context, ownerEmail, runID, path string) ([]sandbox.Entry, error) {
	r, err := s.runForOwner(ctx, ownerEmail, runID)
	if err != nil {
		return nil, err
	}
	return s.sandbox.List(r.ID, path)
}

// RunFsRead returns up to limit lines of the file at path in a run's sandbox,
// starting at 1-based offset. Owner-scoped via the run's owner_email; survives a
// tombstoned prompt.
func (s *Service) RunFsRead(ctx context.Context, ownerEmail, runID, path string, offset, limit int) (string, error) {
	r, err := s.runForOwner(ctx, ownerEmail, runID)
	if err != nil {
		return "", err
	}
	return s.sandbox.Read(r.ID, path, offset, limit)
}

// SetTrigger attaches one (source, event_filter) binding to the owner's prompt.
// Loading the prompt first enforces ownership (ErrNotFound if not owned).
// validateTrigger rejects an unknown source or an event_filter that matches no
// type the producer publishes (A12) with ErrValidation. May be called
// repeatedly to attach several bindings; the composite key dedupes a repeat.
func (s *Service) SetTrigger(ctx context.Context, ownerEmail, id, source, eventFilter string) (Trigger, error) {
	if _, err := s.store.GetPrompt(ctx, ownerEmail, id); err != nil {
		return Trigger{}, err
	}
	if err := validateTrigger(source, eventFilter); err != nil {
		return Trigger{}, err
	}
	t := Trigger{
		PromptID:    id,
		Source:      source,
		EventFilter: eventFilter,
		CreatedAt:   s.nowStr(),
	}
	if err := s.store.SetTrigger(ctx, t); err != nil {
		return Trigger{}, err
	}
	return t, nil
}

// ClearTrigger removes one (source, event_filter) binding from the owner's
// prompt. Loading the prompt first enforces ownership. A binding that does not
// exist returns ErrNotFound.
func (s *Service) ClearTrigger(ctx context.Context, ownerEmail, id, source, eventFilter string) error {
	if _, err := s.store.GetPrompt(ctx, ownerEmail, id); err != nil {
		return err
	}
	return s.store.ClearTrigger(ctx, id, source, eventFilter)
}

// PromptsForEvent returns every prompt id whose binding matches (source, evType)
// — the event→prompts fan-out the consumer runs on each arrival. It is NOT
// owner-scoped: a fired event has no caller identity, and the trigger linkage
// (set by the owner) is the authority. A thin passthrough to the store.
func (s *Service) PromptsForEvent(ctx context.Context, source, evType string) ([]string, error) {
	return s.store.PromptsForEvent(ctx, source, evType)
}

// TriggerSources returns the static known-producer set (A12), for set_trigger
// validation.
func (s *Service) TriggerSources() []string {
	return triggerSources()
}
