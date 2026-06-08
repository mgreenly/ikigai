package prompt

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"eventplane/outbox"
)

// Store is the SQLite persistence for prompts and runs. All reads that take
// an owner are owner-scoped: a row owned by another caller reads as absent.
type Store struct {
	db  *sql.DB
	now func() time.Time

	// Outbox, when set, makes prompts an event-plane producer: FinishRun appends
	// the run.succeeded / run.failed outcome event on the SAME transaction as the
	// run's terminal-state write (event-triggering decisions §3 — at-most-once per
	// run, atomic). nil in tests/builds that do not exercise the producer; the
	// terminal write still commits, just without an event. Injected by the
	// Producer hook in cmd/prompts after appkit constructs the outbox.
	Outbox *outbox.Outbox
}

// NewStore wraps a migrated *sql.DB (the prompts/runs tables must exist).
func NewStore(db *sql.DB) *Store {
	return &Store{db: db, now: func() time.Time { return time.Now().UTC() }}
}

func (s *Store) nowStr() string {
	return s.now().UTC().Format(time.RFC3339Nano)
}

func marshalConfig(c Config) (string, error) {
	b, err := json.Marshal(c)
	if err != nil {
		return "", fmt.Errorf("prompt: marshal config: %w", err)
	}
	return string(b), nil
}

func unmarshalConfig(s string) (Config, error) {
	var c Config
	if err := json.Unmarshal([]byte(s), &c); err != nil {
		return Config{}, fmt.Errorf("prompt: unmarshal config: %w", err)
	}
	return c, nil
}

// InsertPrompt persists a new prompt row.
func (s *Store) InsertPrompt(ctx context.Context, p Prompt) error {
	cfg, err := marshalConfig(p.Config)
	if err != nil {
		return err
	}
	_, err = s.db.ExecContext(ctx,
		`INSERT INTO prompts
		   (id, owner_email, name, user_prompt, system_prompt, config_json, created_at, updated_at)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
		p.ID, p.OwnerEmail, nullStr(p.Name), p.UserPrompt, nullStr(p.SystemPrompt),
		cfg, p.CreatedAt, p.UpdatedAt,
	)
	if err != nil {
		return fmt.Errorf("prompt: insert: %w", err)
	}
	return nil
}

// GetPrompt returns the owner's prompt, or ErrNotFound when it is missing or
// owned by another caller.
func (s *Store) GetPrompt(ctx context.Context, owner, id string) (Prompt, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, owner_email, name, user_prompt, system_prompt, config_json, created_at, updated_at
		   FROM prompts WHERE id = ? AND owner_email = ?`,
		id, owner,
	)
	return scanPrompt(row)
}

// GetPromptByID returns a prompt by id with no owner scoping (for the
// event-triggered run path, where there is no caller identity). ErrNotFound when
// the prompt is gone.
func (s *Store) GetPromptByID(ctx context.Context, id string) (Prompt, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, owner_email, name, user_prompt, system_prompt, config_json, created_at, updated_at
		   FROM prompts WHERE id = ?`,
		id,
	)
	return scanPrompt(row)
}

// ListPrompts returns all of the owner's prompts, newest first.
func (s *Store) ListPrompts(ctx context.Context, owner string) ([]Prompt, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT id, owner_email, name, user_prompt, system_prompt, config_json, created_at, updated_at
		   FROM prompts WHERE owner_email = ? ORDER BY created_at DESC, id DESC`,
		owner,
	)
	if err != nil {
		return nil, fmt.Errorf("prompt: list: %w", err)
	}
	defer rows.Close()
	var out []Prompt
	for rows.Next() {
		p, err := scanPrompt(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, p)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("prompt: list rows: %w", err)
	}
	return out, nil
}

// UpdatePrompt persists editable fields (name/user_prompt/system_prompt/config)
// and bumps updated_at. It is owner-scoped; a no-match (missing or
// foreign-owned) returns ErrNotFound.
func (s *Store) UpdatePrompt(ctx context.Context, owner string, p Prompt) error {
	cfg, err := marshalConfig(p.Config)
	if err != nil {
		return err
	}
	res, err := s.db.ExecContext(ctx,
		`UPDATE prompts
		    SET name = ?, user_prompt = ?, system_prompt = ?, config_json = ?, updated_at = ?
		  WHERE id = ? AND owner_email = ?`,
		nullStr(p.Name), p.UserPrompt, nullStr(p.SystemPrompt), cfg, p.UpdatedAt,
		p.ID, owner,
	)
	if err != nil {
		return fmt.Errorf("prompt: update: %w", err)
	}
	return requireOne(res, "update")
}

// DeletePrompt removes ONLY the owner's prompt row (tombstone): there is no FK
// cascade, so the prompt's runs (and their on-disk directories) survive and stay
// owner-addressable by run_id. A no-match returns ErrNotFound.
func (s *Store) DeletePrompt(ctx context.Context, owner, id string) error {
	res, err := s.db.ExecContext(ctx,
		`DELETE FROM prompts WHERE id = ? AND owner_email = ?`, id, owner,
	)
	if err != nil {
		return fmt.Errorf("prompt: delete: %w", err)
	}
	return requireOne(res, "delete")
}

// InsertRun persists a new run row.
func (s *Store) InsertRun(ctx context.Context, r Run) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO runs
		   (id, prompt_id, owner_email, prompt_name, status, started_at,
		    ended_at, usage_json, error, trigger_source, trigger_type, trigger_event_id, log_path)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		r.ID, r.PromptID, r.OwnerEmail, nullStr(r.PromptName), r.Status, r.StartedAt,
		nullStr(r.EndedAt), nullStr(r.UsageJSON), nullStr(r.Error),
		nullStr(r.TriggerSource), nullStr(r.TriggerType), nullStr(r.TriggerEventID), r.LogPath,
	)
	if err != nil {
		return fmt.Errorf("prompt: insert run: %w", err)
	}
	return nil
}

// runSelectCols is the column list shared by the run-read queries, in
// scanRun's order.
const runSelectCols = `id, prompt_id, owner_email, prompt_name, status, started_at,
	        ended_at, usage_json, error, trigger_source, trigger_type, trigger_event_id, log_path`

// GetRun returns a run by its run_id, or ErrNotFound when absent. It is NOT
// owner-scoped here (the service scopes via the run's denormalized owner_email,
// which survives a tombstone delete of the run's prompt).
func (s *Store) GetRun(ctx context.Context, runID string) (Run, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT `+runSelectCols+` FROM runs WHERE id = ?`, runID,
	)
	return scanRun(row)
}

// ListRunsByPrompt returns every run of a prompt, newest first.
func (s *Store) ListRunsByPrompt(ctx context.Context, promptID string) ([]Run, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT `+runSelectCols+` FROM runs WHERE prompt_id = ? ORDER BY started_at DESC, id DESC`,
		promptID,
	)
	if err != nil {
		return nil, fmt.Errorf("prompt: list runs: %w", err)
	}
	defer rows.Close()
	var out []Run
	for rows.Next() {
		r, err := scanRun(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, r)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("prompt: list runs rows: %w", err)
	}
	return out, nil
}

// RunningCount returns how many of a prompt's runs are currently 'running' —
// the derived PromptDetail.RunningCount.
func (s *Store) RunningCount(ctx context.Context, promptID string) (int, error) {
	var n int
	err := s.db.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM runs WHERE prompt_id = ? AND status = ?`,
		promptID, RunRunning,
	).Scan(&n)
	if err != nil {
		return 0, fmt.Errorf("prompt: running count: %w", err)
	}
	return n, nil
}

// GetLatestRun returns the newest run for a prompt by started_at, or
// (nil, nil) when the prompt has never run.
func (s *Store) GetLatestRun(ctx context.Context, promptID string) (*Run, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT `+runSelectCols+`
		   FROM runs WHERE prompt_id = ? ORDER BY started_at DESC, id DESC LIMIT 1`,
		promptID,
	)
	r, err := scanRun(row)
	if errors.Is(err, ErrNotFound) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	return &r, nil
}

// UpdateRunTerminal writes a run's terminal outcome (called by the runner).
func (s *Store) UpdateRunTerminal(ctx context.Context, runID, status, endedAt, usageJSON, errMsg string) error {
	res, err := s.db.ExecContext(ctx,
		`UPDATE runs SET status = ?, ended_at = ?, usage_json = ?, error = ? WHERE id = ?`,
		status, nullStr(endedAt), nullStr(usageJSON), nullStr(errMsg), runID,
	)
	if err != nil {
		return fmt.Errorf("prompt: update run terminal: %w", err)
	}
	return requireOne(res, "update run")
}

// FinishRunInput carries everything FinishRun needs to write a run's terminal
// state. The outcome-event fields (prompt_id, prompt_name, trigger_source,
// trigger_type, trigger_event_id) are read FROM the run row inside the
// transaction, so the runner no longer threads them in (A5).
type FinishRunInput struct {
	RunID     string // the run being finished
	Status    string // terminal runs.status (succeeded|failed|cancelled)
	EndedAt   string // terminal timestamp
	UsageJSON string // captured usage ("" → NULL)
	ErrMsg    string // terminal error ("" → NULL; carried on run.failed)
}

// FinishRun writes a run's terminal outcome and, when this Store is a producer
// (Outbox set), Appends the matching run.succeeded / run.failed event — ALL in
// ONE transaction (event-triggering decisions §3): the run's terminal-state
// write and its outbox event commit together or not at all. If the Append fails
// (e.g. an unregistered type slips through), the whole tx rolls back, so the run
// is NOT marked terminal without its event — the at-most-once-per-run, atomic
// invariant. Ring() fires AFTER a successful Commit (never inside the tx). A
// cancelled run (or any non-outcome terminal state) writes the terminal row but
// emits no event. With a nil Outbox the method is a pure terminal write.
func (s *Store) FinishRun(ctx context.Context, in FinishRunInput) error {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("prompt: finish run begin: %w", err)
	}
	defer tx.Rollback()

	// Read the run's event fields from the row itself (A5): the runner no longer
	// threads prompt_id / prompt_name / trigger context — they live on the run.
	var (
		promptID    string
		promptName  sql.NullString
		trigSource  sql.NullString
		trigType    sql.NullString
		trigEventID sql.NullString
	)
	if err := tx.QueryRowContext(ctx,
		`SELECT prompt_id, prompt_name, trigger_source, trigger_type, trigger_event_id FROM runs WHERE id = ?`,
		in.RunID,
	).Scan(&promptID, &promptName, &trigSource, &trigType, &trigEventID); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return ErrNotFound
		}
		return fmt.Errorf("prompt: finish run read: %w", err)
	}

	res, err := tx.ExecContext(ctx,
		`UPDATE runs SET status = ?, ended_at = ?, usage_json = ?, error = ? WHERE id = ?`,
		in.Status, nullStr(in.EndedAt), nullStr(in.UsageJSON), nullStr(in.ErrMsg), in.RunID,
	)
	if err != nil {
		return fmt.Errorf("prompt: finish run terminal: %w", err)
	}
	if err := requireOne(res, "finish run"); err != nil {
		return err
	}

	emitted := false
	if s.Outbox != nil {
		ev, ok, err := outcomeEvent(in.Status, promptID, promptName.String, in.RunID, trigSource.String, trigType.String, trigEventID.String, in.ErrMsg)
		if err != nil {
			return err
		}
		if ok {
			// Append on the SAME tx as the terminal write — the atomicity invariant.
			if err := s.Outbox.Append(tx, ev); err != nil {
				return fmt.Errorf("prompt: finish run append outcome: %w", err)
			}
			emitted = true
		}
	}

	if err := tx.Commit(); err != nil {
		return fmt.Errorf("prompt: finish run commit: %w", err)
	}
	// Ring AFTER commit: the row is not visible to feed readers until then.
	if emitted {
		s.Outbox.Ring()
	}
	return nil
}

// SweepRunning is crash recovery: every run left 'running' by a crash is
// marked 'failed' (with ended_at + an interrupted error). Returns the number
// of runs swept. It touches RUNS ONLY — there is no prompt status. The sandbox
// folders are left untouched (forward-only on disk).
func (s *Store) SweepRunning(ctx context.Context) (int, error) {
	res, err := s.db.ExecContext(ctx,
		`UPDATE runs SET status = ?, ended_at = ?, error = ?
		  WHERE status = ?`,
		RunFailed, s.nowStr(), "interrupted by restart", RunRunning,
	)
	if err != nil {
		return 0, fmt.Errorf("prompt: sweep runs: %w", err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return 0, fmt.Errorf("prompt: sweep rows: %w", err)
	}
	return int(n), nil
}

// --- scan helpers ---

// scanner is satisfied by both *sql.Row and *sql.Rows.
type scanner interface {
	Scan(dest ...any) error
}

func scanPrompt(sc scanner) (Prompt, error) {
	var (
		p       Prompt
		name    sql.NullString
		sysProm sql.NullString
		cfgJSON string
	)
	err := sc.Scan(
		&p.ID, &p.OwnerEmail, &name, &p.UserPrompt, &sysProm,
		&cfgJSON, &p.CreatedAt, &p.UpdatedAt,
	)
	if errors.Is(err, sql.ErrNoRows) {
		return Prompt{}, ErrNotFound
	}
	if err != nil {
		return Prompt{}, fmt.Errorf("prompt: scan: %w", err)
	}
	p.Name = name.String
	p.SystemPrompt = sysProm.String
	cfg, err := unmarshalConfig(cfgJSON)
	if err != nil {
		return Prompt{}, err
	}
	p.Config = cfg
	return p, nil
}

func scanRun(sc scanner) (Run, error) {
	var (
		r           Run
		promptName  sql.NullString
		endedAt     sql.NullString
		usage       sql.NullString
		errMsg      sql.NullString
		trigSource  sql.NullString
		trigType    sql.NullString
		trigEventID sql.NullString
	)
	err := sc.Scan(
		&r.ID, &r.PromptID, &r.OwnerEmail, &promptName, &r.Status, &r.StartedAt,
		&endedAt, &usage, &errMsg, &trigSource, &trigType, &trigEventID, &r.LogPath,
	)
	if errors.Is(err, sql.ErrNoRows) {
		return Run{}, ErrNotFound
	}
	if err != nil {
		return Run{}, fmt.Errorf("prompt: scan run: %w", err)
	}
	r.PromptName = promptName.String
	r.EndedAt = endedAt.String
	r.UsageJSON = usage.String
	r.Error = errMsg.String
	r.TriggerSource = trigSource.String
	r.TriggerType = trigType.String
	r.TriggerEventID = trigEventID.String
	return r, nil
}

func nullStr(s string) any {
	if s == "" {
		return nil
	}
	return s
}

func requireOne(res sql.Result, op string) error {
	n, err := res.RowsAffected()
	if err != nil {
		return fmt.Errorf("prompt: %s rows: %w", op, err)
	}
	if n == 0 {
		return ErrNotFound
	}
	return nil
}
