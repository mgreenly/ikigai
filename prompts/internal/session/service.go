package session

import (
	"bufio"
	"context"
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
// runner writes the run's terminal state and flips the session back to idle on
// its own. Cancel signals an in-flight run; returns whether one was found.
type Runner interface {
	Spawn(sess Session, run Run)
	Cancel(sessionID string) bool
}

// Service is prompts' domain service — the only mutator of session/run state
// and the holder of the single-flight gate. The MCP handler talks only to it.
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

// CreateInput is the create payload.
type CreateInput struct {
	Name         string
	Prompt       string
	SystemPrompt string
	Config       Config
}

// UpdateInput is the update payload (all fields replace).
type UpdateInput struct {
	Name         string
	Prompt       string
	SystemPrompt string
	Config       Config
}

// validateConfig enforces the §5.1 rules: the model resolves, its provider is
// anthropic, and ANTHROPIC_API_KEY is present (validated per session).
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

// Create validates config, inserts an idle session, and makes its sandbox
// folder. Config.Provider is normalized to "anthropic".
func (s *Service) Create(ctx context.Context, ownerEmail string, in CreateInput) (Session, error) {
	if err := validateConfig(in.Config); err != nil {
		return Session{}, err
	}
	cfg := in.Config
	cfg.Provider = string(model.ProviderAnthropic)

	now := s.nowStr()
	sess := Session{
		ID:           ids.NewULID(),
		OwnerEmail:   ownerEmail,
		Name:         in.Name,
		Prompt:       in.Prompt,
		SystemPrompt: in.SystemPrompt,
		Config:       cfg,
		Status:       StatusIdle,
		CreatedAt:    now,
		UpdatedAt:    now,
	}
	if err := s.store.InsertSession(ctx, sess); err != nil {
		return Session{}, err
	}
	if err := s.sandbox.Create(sess.ID); err != nil {
		return Session{}, fmt.Errorf("session: create sandbox: %w", err)
	}
	return sess, nil
}

// List returns the owner's sessions.
func (s *Service) List(ctx context.Context, ownerEmail string) ([]Session, error) {
	return s.store.ListSessions(ctx, ownerEmail)
}

// Get returns the owner's session with its latest run attached.
func (s *Service) Get(ctx context.Context, ownerEmail, id string) (SessionDetail, error) {
	sess, err := s.store.GetSession(ctx, ownerEmail, id)
	if err != nil {
		return SessionDetail{}, err
	}
	last, err := s.store.GetLatestRun(ctx, id)
	if err != nil {
		return SessionDetail{}, err
	}
	return SessionDetail{Session: sess, LastRun: last}, nil
}

// Update edits name/prompt/system_prompt/config. Rejected while running;
// re-validates config.
func (s *Service) Update(ctx context.Context, ownerEmail, id string, in UpdateInput) (Session, error) {
	sess, err := s.store.GetSession(ctx, ownerEmail, id)
	if err != nil {
		return Session{}, err
	}
	if sess.Status == StatusRunning {
		return Session{}, ErrRunning
	}
	if err := validateConfig(in.Config); err != nil {
		return Session{}, err
	}
	cfg := in.Config
	cfg.Provider = string(model.ProviderAnthropic)

	sess.Name = in.Name
	sess.Prompt = in.Prompt
	sess.SystemPrompt = in.SystemPrompt
	sess.Config = cfg
	sess.UpdatedAt = s.nowStr()

	if err := s.store.UpdateSession(ctx, ownerEmail, sess); err != nil {
		return Session{}, err
	}
	return sess, nil
}

// Delete removes the session (cascading runs), the sandbox folder, and the run
// logs. Rejected while running. Ordered so a mid-failure leaves things
// recoverable; absent folders are tolerated.
func (s *Service) Delete(ctx context.Context, ownerEmail, id string) error {
	sess, err := s.store.GetSession(ctx, ownerEmail, id)
	if err != nil {
		return err
	}
	if sess.Status == StatusRunning {
		return ErrRunning
	}
	// Row first: if this fails nothing on disk is touched. Once the row is
	// gone, the folders are unreferenced and safe to remove best-effort.
	if err := s.store.DeleteSession(ctx, ownerEmail, id); err != nil {
		return err
	}
	if err := s.sandbox.Remove(id); err != nil {
		return fmt.Errorf("session: remove sandbox: %w", err)
	}
	if err := os.RemoveAll(filepath.Join(s.runsDir, id)); err != nil {
		return fmt.Errorf("session: remove run logs: %w", err)
	}
	return nil
}

// Run is the single-flight gate. Under the single-writer SQLite connection the
// load (which sees status) and the SetSessionStatus(running) below run on the
// one serialized connection, so there is no window where two callers both pass
// the idle check: the first flips the session to running before the second can
// read it as idle. ErrBusy if a run is already in flight.
func (s *Service) Run(ctx context.Context, ownerEmail, id string) (Run, error) {
	sess, err := s.store.GetSession(ctx, ownerEmail, id)
	if err != nil {
		return Run{}, err
	}
	if sess.Status == StatusRunning {
		return Run{}, ErrBusy
	}

	runID := ids.NewULID()
	run := Run{
		ID:        runID,
		SessionID: id,
		Status:    RunRunning,
		StartedAt: s.nowStr(),
		LogPath:   filepath.Join(s.runsDir, id, runID+".jsonl"),
	}
	if err := s.store.InsertRun(ctx, run); err != nil {
		return Run{}, err
	}
	if err := s.store.SetSessionStatus(ctx, id, StatusRunning); err != nil {
		return Run{}, err
	}
	// Reflect the flip so the runner sees the running session.
	sess.Status = StatusRunning
	s.runner.Spawn(sess, run)
	return run, nil
}

// RunByID is the event-triggered run path: it starts a run for a session by id
// WITHOUT owner scoping (the trigger linkage, set by the owner, is the
// authority — there is no caller identity on a cron tick). It otherwise mirrors
// Run: it is the single-flight gate (ErrBusy if a run is already in flight,
// which is how the per-session staleness/serialization guard rides on
// session.status), inserts the run row, flips the session to running, and hands
// off to the runner. ErrNotFound if the session is gone.
//
// triggerEvent / scheduledFor are the trigger context (the cron event type and
// matched slot that fired this run); they are carried in-memory on the Run to
// the runner's terminal write, where they populate the run.succeeded /
// run.failed outcome payload (event-triggering decisions §3). Both empty for a
// manual run (see Run, which passes "" / "").
func (s *Service) RunByID(ctx context.Context, id, triggerEvent, scheduledFor string) (Run, error) {
	sess, err := s.store.GetSessionByID(ctx, id)
	if err != nil {
		return Run{}, err
	}
	if sess.Status == StatusRunning {
		return Run{}, ErrBusy
	}

	runID := ids.NewULID()
	run := Run{
		ID:           runID,
		SessionID:    id,
		Status:       RunRunning,
		StartedAt:    s.nowStr(),
		LogPath:      filepath.Join(s.runsDir, id, runID+".jsonl"),
		TriggerEvent: triggerEvent,
		ScheduledFor: scheduledFor,
	}
	if err := s.store.InsertRun(ctx, run); err != nil {
		return Run{}, err
	}
	if err := s.store.SetSessionStatus(ctx, id, StatusRunning); err != nil {
		return Run{}, err
	}
	sess.Status = StatusRunning
	s.runner.Spawn(sess, run)
	return run, nil
}

// Output returns up to limit lines of the owner's session's latest run log,
// starting at 1-based line offset (offset<=0 means from-start, limit<=0 means
// no limit — same line-slice semantics as sandbox.Read). The log is the
// append-only stream-json event stream (one JSON event per line); lines are
// returned raw, untransformed. Loading the session first enforces ownership
// (ErrNotFound if not owned) before any log file is touched. If the session
// has never run, an error is returned naming the absence.
func (s *Service) Output(ctx context.Context, ownerEmail, id string, offset, limit int) (string, error) {
	if _, err := s.store.GetSession(ctx, ownerEmail, id); err != nil {
		return "", err
	}
	last, err := s.store.GetLatestRun(ctx, id)
	if err != nil {
		return "", err
	}
	if last == nil {
		return "", fmt.Errorf("session: no run output yet")
	}
	return readLines(last.LogPath, offset, limit)
}

// readLines reads up to limit lines of the file at path starting at 1-based
// offset, mirroring sandbox.Read's line-slice semantics (offset<=0 → from
// start, limit<=0 → no limit).
func readLines(path string, offset, limit int) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			return "", fmt.Errorf("session: run log not found")
		}
		return "", fmt.Errorf("session: open run log: %w", err)
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
		return "", fmt.Errorf("session: read run log: %w", err)
	}
	return b.String(), nil
}

// FsList lists the entries under path in the owner's session sandbox. Loading
// the session first enforces ownership (ErrNotFound if not owned) before the
// sandbox is read.
func (s *Service) FsList(ctx context.Context, ownerEmail, id, path string) ([]sandbox.Entry, error) {
	if _, err := s.store.GetSession(ctx, ownerEmail, id); err != nil {
		return nil, err
	}
	return s.sandbox.List(id, path)
}

// FsRead returns up to limit lines of the file at path in the owner's session
// sandbox, starting at 1-based offset. Loading the session first enforces
// ownership (ErrNotFound if not owned) before the sandbox is read.
func (s *Service) FsRead(ctx context.Context, ownerEmail, id, path string, offset, limit int) (string, error) {
	if _, err := s.store.GetSession(ctx, ownerEmail, id); err != nil {
		return "", err
	}
	return s.sandbox.Read(id, path, offset, limit)
}

// SetTriggerInput is the set-trigger payload. MaxStalenessSecs / MaxAttempts
// default (DefaultMaxStalenessSecs / DefaultMaxAttempts) when <= 0.
type SetTriggerInput struct {
	TriggerEvent     string
	MaxStalenessSecs int
	MaxAttempts      int
}

// SetTrigger attaches (or replaces — 1:1) the event trigger on the owner's
// session. Loading the session first enforces ownership (ErrNotFound if not
// owned). TriggerEvent must be non-empty; the value is stored verbatim and the
// consumer fan-out matches it literally against incoming cron.<name> types.
func (s *Service) SetTrigger(ctx context.Context, ownerEmail, id string, in SetTriggerInput) (Trigger, error) {
	if _, err := s.store.GetSession(ctx, ownerEmail, id); err != nil {
		return Trigger{}, err
	}
	if in.TriggerEvent == "" {
		return Trigger{}, validationErrf("invalid trigger: trigger_event is required")
	}
	staleness := in.MaxStalenessSecs
	if staleness <= 0 {
		staleness = DefaultMaxStalenessSecs
	}
	attempts := in.MaxAttempts
	if attempts <= 0 {
		attempts = DefaultMaxAttempts
	}
	t := Trigger{
		SessionID:        id,
		TriggerEvent:     in.TriggerEvent,
		MaxStalenessSecs: staleness,
		MaxAttempts:      attempts,
	}
	if err := s.store.SetTrigger(ctx, t); err != nil {
		return Trigger{}, err
	}
	// Re-read so created_at/updated_at reflect what was persisted.
	return s.store.GetTrigger(ctx, id)
}

// TriggersForEvent returns every trigger whose trigger_event matches eventType
// — the event→sessions fan-out the consumer runs on each cron.<name> arrival.
// It is not owner-scoped: a cron tick has no caller identity, and the trigger
// linkage (set by the owner) is the authority. A thin passthrough to the store.
func (s *Service) TriggersForEvent(ctx context.Context, eventType string) ([]Trigger, error) {
	return s.store.TriggersForEvent(ctx, eventType)
}

// ClearTrigger detaches the owner's session's trigger. Loading the session
// first enforces ownership. A session with no trigger returns ErrNotFound.
func (s *Service) ClearTrigger(ctx context.Context, ownerEmail, id string) error {
	if _, err := s.store.GetSession(ctx, ownerEmail, id); err != nil {
		return err
	}
	return s.store.ClearTrigger(ctx, id)
}

// Cancel signals the in-flight run for the owner's session. It is idempotent:
// no in-flight run is not an error (the foreground may race the run's own
// completion). The sandbox folder is kept.
func (s *Service) Cancel(ctx context.Context, ownerEmail, id string) error {
	if _, err := s.store.GetSession(ctx, ownerEmail, id); err != nil {
		return err
	}
	s.runner.Cancel(id)
	return nil
}
