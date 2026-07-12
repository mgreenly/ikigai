package prompt

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path"
	"path/filepath"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/ikigenba/agentkit"
	"github.com/ikigenba/agentkit/anthropic"
	"github.com/ikigenba/agentkit/google"
	"github.com/ikigenba/agentkit/openai"
	"github.com/ikigenba/agentkit/zai"
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
	// Fetcher reads bytes from the dropbox mirror over loopback for Import. It is
	// field-injected at the composition root (cmd/prompts sets svc.Fetcher), not a
	// NewService parameter, so every existing NewService call site and test stays
	// untouched. nil unless wired (only Import uses it).
	Fetcher ContentFetcher
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
// each canonical filter is applied via SetTrigger after the prompt row is
// inserted (same validation; an invalid one rejects the whole create).
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

var providerEnvVars = map[string]string{
	"anthropic": "ANTHROPIC_API_KEY",
	"openai":    "OPENAI_API_KEY",
	"google":    "GEMINI_API_KEY",
	"zai":       "ZAI_API_KEY",
}

// validateConfig enforces the provider/model/key contract for the stored
// config. getenv is injected so tests can validate the key contract without
// mutating process-wide environment.
func validateConfig(c Config, getenv func(string) string) error {
	envVar, ok := providerEnvVars[c.Provider]
	if !ok {
		return validationErrf("invalid config: provider %q is not supported", c.Provider)
	}
	provider := providerRegistry(c.Provider)
	if _, ok := provider.Pricing(c.Model); !ok {
		return validationErrf("invalid config: provider %q does not support model %q", c.Provider, c.Model)
	}
	if getenv(envVar) == "" {
		return validationErrf("invalid config: %s is not set", envVar)
	}
	return nil
}

func providerRegistry(name string) agentkit.Provider {
	switch name {
	case "anthropic":
		return anthropic.New("")
	case "openai":
		return openai.New("")
	case "google":
		return google.New("")
	case "zai":
		return zai.New("")
	default:
		panic("providerRegistry called with unsupported provider")
	}
}

func applyProviderDefault(c Config) Config {
	if c.Provider == "" {
		c.Provider = "anthropic"
	}
	return c
}

// Create validates config and inserts a prompt. The sandbox is no longer
// created here — it is per-run, created at Run time keyed by run_id.
// Config.Provider is validated against the configured model and preserved.
func (s *Service) Create(ctx context.Context, ownerEmail string, in CreateInput) (Prompt, error) {
	cfg := applyProviderDefault(in.Config)
	if err := validateConfig(cfg, os.Getenv); err != nil {
		return Prompt{}, err
	}
	// Validate every inline trigger BEFORE inserting the prompt, so an invalid
	// binding rejects the whole create without leaving a triggerless orphan.
	for _, ts := range in.Triggers {
		if _, err := validateTrigger(ts.Filter); err != nil {
			return Prompt{}, err
		}
	}
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
		source, _ := validateTrigger(ts.Filter)
		if err := s.store.SetTrigger(ctx, Trigger{
			PromptID:  p.ID,
			Source:    source,
			Filter:    ts.Filter,
			CreatedAt: now,
		}); err != nil {
			return Prompt{}, err
		}
	}
	return p, nil
}

// maxImportBytes caps an imported prompt body at 1 MiB — a prompt file above
// that is almost certainly a mistake (ADR "Asserted defaults").
const maxImportBytes = 1 << 20

// importDefaultModel is the model an imported prompt's config defaults to. A
// prompt is multi-field but a file is one blob, so import maps the file body →
// user_prompt and leaves the other fields at sane defaults (ADR Decision 5). The
// config must still validate, so import seeds an explicit provider and the
// canonical Sonnet id (the same model the describe doc shows as the example
// config) so an imported prompt is immediately runnable.
const importDefaultModel = "claude-sonnet-4-6"

// Import adopts a Dropbox-mirrored file as a prompt. It fetches the current
// mirror bytes over loopback, requires valid UTF-8 text under 1 MiB (a binary
// blob is not prompt text), maps the file body → user_prompt, derives the name
// from the path when none is given, leaves system_prompt empty, and seeds a valid
// default config (importDefaultModel) so the prompt is runnable. It upserts on
// (owner, source_path): re-importing the same path updates the same prompt
// instead of creating a duplicate (config + system_prompt keep their values on a
// re-import — a re-pull is a body refresh, not a config change). Direction is
// strictly Dropbox → prompts; nothing writes back. See
// docs/adr-dropbox-import-sync.md.
func (s *Service) Import(ctx context.Context, owner, sourcePath, name string) (Prompt, error) {
	if strings.TrimSpace(sourcePath) == "" {
		return Prompt{}, fmt.Errorf("%w: source_path is required", ErrValidation)
	}
	data, err := s.Fetcher.Fetch(ctx, sourcePath)
	if err != nil {
		return Prompt{}, err
	}
	if !utf8.Valid(data) {
		return Prompt{}, fmt.Errorf("%w: %q is not valid UTF-8 text (a prompt body must be text)", ErrValidation, sourcePath)
	}
	if len(data) > maxImportBytes {
		return Prompt{}, fmt.Errorf("%w: %q is %d bytes, over the 1 MiB import limit", ErrValidation, sourcePath, len(data))
	}
	if name == "" {
		name = path.Base(sourcePath)
	}
	// Seed a valid default config so the imported prompt is runnable. Config is
	// left as-is on a re-import (the upsert refreshes name + user_prompt only), so
	// this default only takes effect on the first import.
	cfg := Config{Provider: "anthropic", Model: importDefaultModel}
	return s.store.UpsertPromptBySource(ctx, owner, sourcePath, name, string(data), cfg, s.nowStr())
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
	cfg := applyProviderDefault(in.Config)
	if err := validateConfig(cfg, os.Getenv); err != nil {
		return Prompt{}, err
	}

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
	return s.startRun(ctx, p, "", "", "", "", nil)
}

// materializeInput pins the prompt's changeable execution inputs to
// runs/<run_id>/input/ BEFORE spawn: user_prompt.txt, system_prompt.txt (empty
// file when none), and config.json (the resolved Config). Once written, this is
// the record of exactly what the run executes — the runner reads from here, so
// editing or deleting the prompt mid-run cannot change what the run runs. For an
// event-triggered run it also writes event.json (the triggering event envelope);
// that file is absent on a manual run.
func (s *Service) materializeInput(run Run, p Prompt, payload []byte) error {
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
	if run.TriggerSource != "" {
		env := eventEnvelope{Source: run.TriggerSource, Kind: run.TriggerKind, Subject: run.TriggerSubject, EventID: run.TriggerEventID, Payload: json.RawMessage(payload)}
		if len(env.Payload) == 0 {
			env.Payload = json.RawMessage("null")
		}
		b, err := json.Marshal(env)
		if err != nil {
			return fmt.Errorf("prompt: marshal run event: %w", err)
		}
		if err := os.WriteFile(filepath.Join(inputDir, "event.json"), b, 0o644); err != nil {
			return fmt.Errorf("prompt: write event: %w", err)
		}
	}
	return nil
}

// startRun is the shared run-start path for Run (manual) and RunByEvent (event).
// It builds the run row (denormalizing owner_email / prompt_name and the
// trigger context), materializes input/ from the prompt's CURRENT definition,
// inserts the row, creates the durable run-scoped sandbox, and hands off to the
// runner — which executes from disk, never from p.
type eventEnvelope struct {
	Source  string          `json:"source"`
	Kind    string          `json:"kind"`
	Subject string          `json:"subject"`
	EventID string          `json:"event_id"`
	Payload json.RawMessage `json:"payload"`
}

func (s *Service) startRun(ctx context.Context, p Prompt, source, kind, subject, eventID string, payload []byte) (Run, error) {
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
		TriggerKind:    kind,
		TriggerSubject: subject,
		TriggerEventID: eventID,
	}
	// Pin the execution inputs to disk BEFORE the row exists / spawn happens.
	if err := s.materializeInput(run, p, payload); err != nil {
		return Run{}, err
	}
	if err := s.store.InsertRun(ctx, run); err != nil {
		return Run{}, err
	}
	// Create the durable run-scoped sandbox (state/sandboxes/<run_id>/sandbox)
	// before spawn.
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
// Run, which passes "" / "" / ""). payload is the upstream producer's raw event
// body; it is pinned to the run's input/event.json (inside the canonical event
// envelope) for an event-triggered run.
func (s *Service) RunByEvent(ctx context.Context, id, source, kind string, args ...any) (Run, error) {
	var subject, eventID string
	var payload []byte
	switch len(args) {
	case 2:
		eventID, _ = args[0].(string)
		payload = eventPayload(args[1])
	case 3:
		subject, _ = args[0].(string)
		eventID, _ = args[1].(string)
		payload = eventPayload(args[2])
	default:
		return Run{}, fmt.Errorf("%w: invalid event arguments", ErrValidation)
	}
	p, err := s.store.GetPromptByID(ctx, id)
	if err != nil {
		return Run{}, err
	}
	return s.startRun(ctx, p, source, kind, subject, eventID, payload)
}

func eventPayload(v any) []byte {
	switch b := v.(type) {
	case []byte:
		return b
	case json.RawMessage:
		return []byte(b)
	default:
		return nil
	}
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

// RunFsList lists the entries under path in a run's durable sandbox. Owner-
// scoped via the run's owner_email; survives a tombstoned prompt.
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

// SetTrigger attaches one canonical filter to the owner's prompt. Loading the
// prompt first enforces ownership (ErrNotFound if not owned).
func (s *Service) SetTrigger(ctx context.Context, ownerEmail, id, filter string) (Trigger, error) {
	if _, err := s.store.GetPrompt(ctx, ownerEmail, id); err != nil {
		return Trigger{}, err
	}
	source, err := validateTrigger(filter)
	if err != nil {
		return Trigger{}, err
	}
	t := Trigger{
		PromptID:  id,
		Source:    source,
		Filter:    filter,
		CreatedAt: s.nowStr(),
	}
	if err := s.store.SetTrigger(ctx, t); err != nil {
		return Trigger{}, err
	}
	return t, nil
}

// ClearTrigger removes one canonical filter from the owner's prompt. Loading
// the prompt first enforces ownership. A missing binding returns ErrNotFound.
func (s *Service) ClearTrigger(ctx context.Context, ownerEmail, id, filter string) error {
	if _, err := s.store.GetPrompt(ctx, ownerEmail, id); err != nil {
		return err
	}
	return s.store.ClearTrigger(ctx, id, filter)
}

// PromptsForEvent returns every prompt id whose binding matches (source, evType)
// — the event→prompts fan-out the consumer runs on each arrival. It is NOT
// owner-scoped: a fired event has no caller identity, and the trigger linkage
// (set by the owner) is the authority. A thin passthrough to the store.
func (s *Service) PromptsForEvent(ctx context.Context, source, key string) ([]string, error) {
	return s.store.PromptsForEvent(ctx, source, key)
}

// TriggerSources returns the static known-producer set (A12), for set_trigger
// validation.
func (s *Service) TriggerSources() []string {
	return triggerSources()
}
