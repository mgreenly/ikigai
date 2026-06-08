package script

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"

	"eventplane/outbox"
)

// Store is the SQLite persistence for scripts, runs, and triggers. All reads
// that take an owner are owner-scoped: a row owned by another caller reads as
// absent (ErrNotFound).
type Store struct {
	db *sql.DB
	// Outbox, when set, makes scripts an event-plane producer: FinishRun appends
	// the scripts.succeeded / scripts.failed completion event on the SAME
	// transaction as the run's terminal-state write (at-most-once per run,
	// atomic). nil in unit tests; the terminal write still commits without an
	// event. Injected by the Producer hook in cmd/scripts after appkit
	// constructs the outbox.
	Outbox *outbox.Outbox
}

// NewStore wraps a migrated *sql.DB (the scripts/runs/script_triggers tables
// must exist).
func NewStore(db *sql.DB) *Store {
	return &Store{db: db}
}

func marshalConfig(c Config) (string, error) {
	b, err := json.Marshal(c)
	if err != nil {
		return "", fmt.Errorf("script: marshal config: %w", err)
	}
	return string(b), nil
}

func unmarshalConfig(s string) (Config, error) {
	var c Config
	if err := json.Unmarshal([]byte(s), &c); err != nil {
		return Config{}, fmt.Errorf("script: unmarshal config: %w", err)
	}
	return c, nil
}

// --- Script CRUD (owner-scoped) ---

func (s *Store) InsertScript(ctx context.Context, sc Script) error {
	cfg, err := marshalConfig(sc.Config)
	if err != nil {
		return err
	}
	_, err = s.db.ExecContext(ctx,
		`INSERT INTO scripts (id, owner_email, name, body, config_json, created_at, updated_at)
		 VALUES (?, ?, ?, ?, ?, ?, ?)`,
		sc.ID, sc.OwnerEmail, nullStr(sc.Name), sc.Body, cfg, sc.CreatedAt, sc.UpdatedAt,
	)
	if err != nil {
		return fmt.Errorf("script: insert: %w", err)
	}
	return nil
}

func (s *Store) GetScript(ctx context.Context, owner, id string) (Script, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, owner_email, name, body, config_json, created_at, updated_at
		   FROM scripts WHERE id = ? AND owner_email = ?`,
		id, owner,
	)
	return scanScript(row)
}

// ScriptForRun returns the full script row by id with NO owner scoping — the
// runner/consumer path has no caller identity (same precedent as
// ScriptsForEvent / SweepRunning), only run.ScriptID. Returns ErrNotFound when
// the id is absent (a tombstoned/dangling script id; the runner finishes that
// run as failed).
func (s *Store) ScriptForRun(ctx context.Context, scriptID string) (Script, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, owner_email, name, body, config_json, created_at, updated_at
		   FROM scripts WHERE id = ?`,
		scriptID,
	)
	return scanScript(row)
}

// UpdateScript persists editable fields (name/body/config) and bumps
// updated_at. Owner-scoped; a no-match returns ErrNotFound.
func (s *Store) UpdateScript(ctx context.Context, owner string, sc Script) error {
	cfg, err := marshalConfig(sc.Config)
	if err != nil {
		return err
	}
	res, err := s.db.ExecContext(ctx,
		`UPDATE scripts SET name = ?, body = ?, config_json = ?, updated_at = ?
		  WHERE id = ? AND owner_email = ?`,
		nullStr(sc.Name), sc.Body, cfg, sc.UpdatedAt, sc.ID, owner,
	)
	if err != nil {
		return fmt.Errorf("script: update: %w", err)
	}
	return requireOne(res, "update")
}

// DeleteScript is a TOMBSTONE: it deletes the script row + its triggers
// (script_triggers cascades on the script FK); runs + on-disk artifacts SURVIVE
// as append-only history — runs.script_id has NO cascade and is deliberately
// allowed to dangle (002_scripts.sql §7A). A no-match (missing or foreign-owned)
// returns ErrNotFound.
//
// foreign_keys is ON in the chassis pragmas, so a plain DELETE of a script that
// still has runs would be refused by runs.script_id REFERENCES scripts(id). The
// schema's intended dangling-label semantics require the FK check to be off for
// just this delete, so we toggle the (connection-scoped) pragma on a dedicated
// connection around the delete. script_triggers' ON DELETE CASCADE does not fire
// while foreign_keys is OFF, so we delete the trigger rows explicitly to keep
// the live-definition cascade intent.
func (s *Store) DeleteScript(ctx context.Context, owner, id string) error {
	conn, err := s.db.Conn(ctx)
	if err != nil {
		return fmt.Errorf("script: delete conn: %w", err)
	}
	defer conn.Close()

	if _, err := conn.ExecContext(ctx, `PRAGMA foreign_keys = OFF`); err != nil {
		return fmt.Errorf("script: delete fk off: %w", err)
	}
	// Restore the pragma before the connection returns to the pool.
	defer conn.ExecContext(context.Background(), `PRAGMA foreign_keys = ON`)

	tx, err := conn.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("script: delete begin: %w", err)
	}
	defer tx.Rollback()

	res, err := tx.ExecContext(ctx,
		`DELETE FROM scripts WHERE id = ? AND owner_email = ?`, id, owner,
	)
	if err != nil {
		return fmt.Errorf("script: delete: %w", err)
	}
	if err := requireOne(res, "delete"); err != nil {
		return err
	}
	if _, err := tx.ExecContext(ctx,
		`DELETE FROM script_triggers WHERE script_id = ?`, id,
	); err != nil {
		return fmt.Errorf("script: delete triggers: %w", err)
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("script: delete commit: %w", err)
	}
	return nil
}

func (s *Store) ListScripts(ctx context.Context, owner string) ([]Script, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT id, owner_email, name, body, config_json, created_at, updated_at
		   FROM scripts WHERE owner_email = ? ORDER BY created_at DESC, id DESC`,
		owner,
	)
	if err != nil {
		return nil, fmt.Errorf("script: list: %w", err)
	}
	defer rows.Close()
	var out []Script
	for rows.Next() {
		sc, err := scanScript(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, sc)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("script: list rows: %w", err)
	}
	return out, nil
}

// RunningCount returns COUNT(runs WHERE script_id=? AND status='running'). Not
// owner-scoped: the count is derived state read by the service after an
// owner-scoped GetScript.
func (s *Store) RunningCount(ctx context.Context, scriptID string) (int, error) {
	var n int
	err := s.db.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM runs WHERE script_id = ? AND status = ?`,
		scriptID, RunRunning,
	).Scan(&n)
	if err != nil {
		return 0, fmt.Errorf("script: running count: %w", err)
	}
	return n, nil
}

// LastRun returns the newest run for a script by started_at, or (nil, nil) when
// the script has never run.
func (s *Store) LastRun(ctx context.Context, scriptID string) (*Run, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, script_id, status, exit_code, started_at, ended_at, error,
		        trigger_source, trigger_type, trigger_event_id, stdout_path, stderr_path
		   FROM runs WHERE script_id = ? ORDER BY started_at DESC, id DESC LIMIT 1`,
		scriptID,
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

// --- Runs ---

func (s *Store) InsertRun(ctx context.Context, r Run) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO runs
		   (id, script_id, status, exit_code, started_at, ended_at, error,
		    trigger_source, trigger_type, trigger_event_id, stdout_path, stderr_path)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		r.ID, r.ScriptID, r.Status, nullInt(r.ExitCode), r.StartedAt,
		nullStr(r.EndedAt), nullStr(r.Error),
		nullStr(r.TriggerSource), nullStr(r.TriggerType), nullStr(r.TriggerEventID),
		r.StdoutPath, r.StderrPath,
	)
	if err != nil {
		return fmt.Errorf("script: insert run: %w", err)
	}
	return nil
}

// GetRun returns the run, joined to scripts for owner scope: a run whose script
// is missing or owned by another caller reads as ErrNotFound.
func (s *Store) GetRun(ctx context.Context, owner, runID string) (Run, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT r.id, r.script_id, r.status, r.exit_code, r.started_at, r.ended_at, r.error,
		        r.trigger_source, r.trigger_type, r.trigger_event_id, r.stdout_path, r.stderr_path
		   FROM runs r JOIN scripts s ON s.id = r.script_id
		  WHERE r.id = ? AND s.owner_email = ?`,
		runID, owner,
	)
	return scanRun(row)
}

// ListRuns returns the owner's runs (JOIN scripts on owner_email), newest
// first. scriptID/status "" = no filter on that dimension.
func (s *Store) ListRuns(ctx context.Context, owner, scriptID, status string) ([]Run, error) {
	q := `SELECT r.id, r.script_id, r.status, r.exit_code, r.started_at, r.ended_at, r.error,
	             r.trigger_source, r.trigger_type, r.trigger_event_id, r.stdout_path, r.stderr_path
	        FROM runs r JOIN scripts s ON s.id = r.script_id
	       WHERE s.owner_email = ?`
	args := []any{owner}
	if scriptID != "" {
		q += ` AND r.script_id = ?`
		args = append(args, scriptID)
	}
	if status != "" {
		q += ` AND r.status = ?`
		args = append(args, status)
	}
	q += ` ORDER BY r.started_at DESC, r.id DESC`
	rows, err := s.db.QueryContext(ctx, q, args...)
	if err != nil {
		return nil, fmt.Errorf("script: list runs: %w", err)
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
		return nil, fmt.Errorf("script: list runs rows: %w", err)
	}
	return out, nil
}

// SweepRunning is crash recovery: every run left 'running' by a crash is marked
// 'failed' (with an interrupted error). It returns the ids of the runs that
// were swept (Recover uses them). The run dirs are left untouched
// (forward-only on disk).
func (s *Store) SweepRunning(ctx context.Context) (ids []string, err error) {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return nil, fmt.Errorf("script: sweep begin: %w", err)
	}
	defer tx.Rollback()

	rows, err := tx.QueryContext(ctx,
		`SELECT id FROM runs WHERE status = ?`, RunRunning,
	)
	if err != nil {
		return nil, fmt.Errorf("script: sweep select: %w", err)
	}
	for rows.Next() {
		var id string
		if err := rows.Scan(&id); err != nil {
			rows.Close()
			return nil, fmt.Errorf("script: sweep scan: %w", err)
		}
		ids = append(ids, id)
	}
	if err := rows.Err(); err != nil {
		rows.Close()
		return nil, fmt.Errorf("script: sweep rows: %w", err)
	}
	rows.Close()

	if len(ids) > 0 {
		if _, err := tx.ExecContext(ctx,
			`UPDATE runs SET status = ?, error = ? WHERE status = ?`,
			RunFailed, "interrupted by restart", RunRunning,
		); err != nil {
			return nil, fmt.Errorf("script: sweep update: %w", err)
		}
	}
	if err := tx.Commit(); err != nil {
		return nil, fmt.Errorf("script: sweep commit: %w", err)
	}
	return ids, nil
}

// FinishRun is the ATOMIC terminal write + completion-event emit (A5/A7): in one
// transaction it writes the run's terminal status/exit_code/ended_at/error and,
// when this Store is a producer (Outbox set) AND the status is an outcome
// (succeeded|failed, never cancelled), Appends the matching
// scripts.succeeded / scripts.failed event on that SAME tx. If the Append fails
// the whole tx rolls back, so the run is never marked terminal without its event
// (at-most-once-per-run, atomic). Ring() fires AFTER a successful Commit (never
// inside the tx). With a nil Outbox or a cancelled status it is a pure terminal
// write.
func (s *Store) FinishRun(ctx context.Context, in FinishRunInput) error {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("script: finish run begin: %w", err)
	}
	defer tx.Rollback()

	res, err := tx.ExecContext(ctx,
		`UPDATE runs SET status = ?, exit_code = ?, ended_at = ?, error = ? WHERE id = ?`,
		in.Status, nullInt(in.ExitCode), nullStr(in.EndedAt), nullStr(in.ErrMsg), in.RunID,
	)
	if err != nil {
		return fmt.Errorf("script: finish run terminal: %w", err)
	}
	if err := requireOne(res, "finish run"); err != nil {
		return err
	}

	emitted := false
	if s.Outbox != nil {
		ev, ok, err := completionEvent(in)
		if err != nil {
			return err
		}
		if ok {
			// Append on the SAME tx as the terminal write — the atomicity invariant.
			if err := s.Outbox.Append(tx, ev); err != nil {
				return fmt.Errorf("script: finish run append: %w", err)
			}
			emitted = true
		}
	}

	if err := tx.Commit(); err != nil {
		return fmt.Errorf("script: finish run commit: %w", err)
	}
	// Ring AFTER commit: the row is not visible to feed readers until then.
	if emitted {
		s.Outbox.Ring()
	}
	return nil
}

// --- Triggers ---

// SetTrigger upserts a trigger row for the owner's script. Returns ErrNotFound
// if the script is missing or owned by another caller.
func (s *Store) SetTrigger(ctx context.Context, owner string, t Trigger) error {
	if err := s.ownsScript(ctx, owner, t.ScriptID); err != nil {
		return err
	}
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO script_triggers (script_id, source, event_filter, created_at)
		 VALUES (?, ?, ?, ?)
		 ON CONFLICT(script_id, source, event_filter) DO UPDATE SET created_at = excluded.created_at`,
		t.ScriptID, t.Source, t.EventFilter, t.CreatedAt,
	)
	if err != nil {
		return fmt.Errorf("script: set trigger: %w", err)
	}
	return nil
}

// ClearTrigger removes a trigger row for the owner's script. Returns ErrNotFound
// if the script is missing or owned by another caller.
func (s *Store) ClearTrigger(ctx context.Context, owner string, t Trigger) error {
	if err := s.ownsScript(ctx, owner, t.ScriptID); err != nil {
		return err
	}
	_, err := s.db.ExecContext(ctx,
		`DELETE FROM script_triggers WHERE script_id = ? AND source = ? AND event_filter = ?`,
		t.ScriptID, t.Source, t.EventFilter,
	)
	if err != nil {
		return fmt.Errorf("script: clear trigger: %w", err)
	}
	return nil
}

// ScriptsForEvent returns the distinct script_ids whose trigger source matches
// and whose event_filter glob-matches eventType. NOT owner-scoped (the consumer
// has no caller identity); the box is single-owner.
func (s *Store) ScriptsForEvent(ctx context.Context, source, eventType string) ([]string, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT script_id, event_filter FROM script_triggers WHERE source = ?`,
		source,
	)
	if err != nil {
		return nil, fmt.Errorf("script: scripts for event: %w", err)
	}
	defer rows.Close()
	seen := map[string]bool{}
	var out []string
	for rows.Next() {
		var id, filter string
		if err := rows.Scan(&id, &filter); err != nil {
			return nil, fmt.Errorf("script: scripts for event scan: %w", err)
		}
		if seen[id] {
			continue
		}
		if globMatch(filter, eventType) {
			seen[id] = true
			out = append(out, id)
		}
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("script: scripts for event rows: %w", err)
	}
	return out, nil
}

// ownsScript returns nil when the script exists and is owned by owner, else
// ErrNotFound.
func (s *Store) ownsScript(ctx context.Context, owner, scriptID string) error {
	var exists int
	err := s.db.QueryRowContext(ctx,
		`SELECT 1 FROM scripts WHERE id = ? AND owner_email = ?`, scriptID, owner,
	).Scan(&exists)
	if errors.Is(err, sql.ErrNoRows) {
		return ErrNotFound
	}
	if err != nil {
		return fmt.Errorf("script: owns check: %w", err)
	}
	return nil
}

// FinishRunInput is the atomic terminal write payload for FinishRun (A5/A7). The
// Trigger* fields are the run's trigger CONTEXT (matching Run.TriggerSource/
// Type/EventID), carried into the completion-event payload; all empty for a
// manual run.
type FinishRunInput struct {
	RunID, ScriptID, ScriptName string
	Status                      string // succeeded|failed (NEVER cancelled — cancelled emits no event, see A7)
	ExitCode                    *int
	EndedAt                     string
	ErrMsg                      string
	TriggerSource               string // "" for a manual run
	TriggerType                 string
	TriggerEventID              string
	StdoutTail                  string // last 8KB, already read+truncated by runner
	StderrTail                  string
	StdoutTrunc                 bool
	StderrTrunc                 bool
}

// --- scan helpers ---

// scanner is satisfied by both *sql.Row and *sql.Rows.
type scanner interface {
	Scan(dest ...any) error
}

func scanScript(sc scanner) (Script, error) {
	var (
		out     Script
		name    sql.NullString
		cfgJSON string
	)
	err := sc.Scan(
		&out.ID, &out.OwnerEmail, &name, &out.Body, &cfgJSON, &out.CreatedAt, &out.UpdatedAt,
	)
	if errors.Is(err, sql.ErrNoRows) {
		return Script{}, ErrNotFound
	}
	if err != nil {
		return Script{}, fmt.Errorf("script: scan: %w", err)
	}
	out.Name = name.String
	cfg, err := unmarshalConfig(cfgJSON)
	if err != nil {
		return Script{}, err
	}
	out.Config = cfg
	return out, nil
}

func scanRun(sc scanner) (Run, error) {
	var (
		r          Run
		exitCode   sql.NullInt64
		endedAt    sql.NullString
		errMsg     sql.NullString
		trigSource sql.NullString
		trigType   sql.NullString
		trigEvent  sql.NullString
	)
	err := sc.Scan(
		&r.ID, &r.ScriptID, &r.Status, &exitCode, &r.StartedAt, &endedAt, &errMsg,
		&trigSource, &trigType, &trigEvent, &r.StdoutPath, &r.StderrPath,
	)
	if errors.Is(err, sql.ErrNoRows) {
		return Run{}, ErrNotFound
	}
	if err != nil {
		return Run{}, fmt.Errorf("script: scan run: %w", err)
	}
	if exitCode.Valid {
		v := int(exitCode.Int64)
		r.ExitCode = &v
	}
	r.EndedAt = endedAt.String
	r.Error = errMsg.String
	r.TriggerSource = trigSource.String
	r.TriggerType = trigType.String
	r.TriggerEventID = trigEvent.String
	return r, nil
}

func nullStr(s string) any {
	if s == "" {
		return nil
	}
	return s
}

func nullInt(p *int) any {
	if p == nil {
		return nil
	}
	return *p
}

func requireOne(res sql.Result, op string) error {
	n, err := res.RowsAffected()
	if err != nil {
		return fmt.Errorf("script: %s rows: %w", op, err)
	}
	if n == 0 {
		return ErrNotFound
	}
	return nil
}
