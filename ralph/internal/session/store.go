package session

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"time"
)

// Store is the SQLite persistence for sessions and runs. All reads that take
// an owner are owner-scoped: a row owned by another caller reads as absent.
type Store struct {
	db  *sql.DB
	now func() time.Time
}

// NewStore wraps a migrated *sql.DB (the sessions/runs tables must exist).
func NewStore(db *sql.DB) *Store {
	return &Store{db: db, now: func() time.Time { return time.Now().UTC() }}
}

func (s *Store) nowStr() string {
	return s.now().UTC().Format(time.RFC3339Nano)
}

func marshalConfig(c Config) (string, error) {
	b, err := json.Marshal(c)
	if err != nil {
		return "", fmt.Errorf("session: marshal config: %w", err)
	}
	return string(b), nil
}

func unmarshalConfig(s string) (Config, error) {
	var c Config
	if err := json.Unmarshal([]byte(s), &c); err != nil {
		return Config{}, fmt.Errorf("session: unmarshal config: %w", err)
	}
	return c, nil
}

// InsertSession persists a new session row.
func (s *Store) InsertSession(ctx context.Context, sess Session) error {
	cfg, err := marshalConfig(sess.Config)
	if err != nil {
		return err
	}
	_, err = s.db.ExecContext(ctx,
		`INSERT INTO sessions
		   (id, owner_email, name, prompt, system_prompt, config_json, status, created_at, updated_at)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		sess.ID, sess.OwnerEmail, nullStr(sess.Name), sess.Prompt, nullStr(sess.SystemPrompt),
		cfg, sess.Status, sess.CreatedAt, sess.UpdatedAt,
	)
	if err != nil {
		return fmt.Errorf("session: insert: %w", err)
	}
	return nil
}

// GetSession returns the owner's session, or ErrNotFound when it is missing or
// owned by another caller.
func (s *Store) GetSession(ctx context.Context, owner, id string) (Session, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, owner_email, name, prompt, system_prompt, config_json, status, created_at, updated_at
		   FROM sessions WHERE id = ? AND owner_email = ?`,
		id, owner,
	)
	return scanSession(row)
}

// GetSessionByID returns a session by id with no owner scoping (for the
// event-triggered run path, where there is no caller identity). ErrNotFound when
// the session is gone.
func (s *Store) GetSessionByID(ctx context.Context, id string) (Session, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, owner_email, name, prompt, system_prompt, config_json, status, created_at, updated_at
		   FROM sessions WHERE id = ?`,
		id,
	)
	return scanSession(row)
}

// ListSessions returns all of the owner's sessions, newest first.
func (s *Store) ListSessions(ctx context.Context, owner string) ([]Session, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT id, owner_email, name, prompt, system_prompt, config_json, status, created_at, updated_at
		   FROM sessions WHERE owner_email = ? ORDER BY created_at DESC, id DESC`,
		owner,
	)
	if err != nil {
		return nil, fmt.Errorf("session: list: %w", err)
	}
	defer rows.Close()
	var out []Session
	for rows.Next() {
		sess, err := scanSession(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, sess)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("session: list rows: %w", err)
	}
	return out, nil
}

// UpdateSession persists editable fields (name/prompt/system_prompt/config)
// and bumps updated_at. It is owner-scoped; a no-match (missing or
// foreign-owned) returns ErrNotFound.
func (s *Store) UpdateSession(ctx context.Context, owner string, sess Session) error {
	cfg, err := marshalConfig(sess.Config)
	if err != nil {
		return err
	}
	res, err := s.db.ExecContext(ctx,
		`UPDATE sessions
		    SET name = ?, prompt = ?, system_prompt = ?, config_json = ?, updated_at = ?
		  WHERE id = ? AND owner_email = ?`,
		nullStr(sess.Name), sess.Prompt, nullStr(sess.SystemPrompt), cfg, sess.UpdatedAt,
		sess.ID, owner,
	)
	if err != nil {
		return fmt.Errorf("session: update: %w", err)
	}
	return requireOne(res, "update")
}

// DeleteSession removes the owner's session row; runs cascade. A no-match
// returns ErrNotFound.
func (s *Store) DeleteSession(ctx context.Context, owner, id string) error {
	res, err := s.db.ExecContext(ctx,
		`DELETE FROM sessions WHERE id = ? AND owner_email = ?`, id, owner,
	)
	if err != nil {
		return fmt.Errorf("session: delete: %w", err)
	}
	return requireOne(res, "delete")
}

// SetSessionStatus flips a session's status (not owner-scoped: the runner and
// sweep act on sessions by id).
func (s *Store) SetSessionStatus(ctx context.Context, id, status string) error {
	_, err := s.db.ExecContext(ctx,
		`UPDATE sessions SET status = ?, updated_at = ? WHERE id = ?`,
		status, s.nowStr(), id,
	)
	if err != nil {
		return fmt.Errorf("session: set status: %w", err)
	}
	return nil
}

// InsertRun persists a new run row.
func (s *Store) InsertRun(ctx context.Context, r Run) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO runs (id, session_id, status, started_at, ended_at, usage_json, error, log_path)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
		r.ID, r.SessionID, r.Status, r.StartedAt,
		nullStr(r.EndedAt), nullStr(r.UsageJSON), nullStr(r.Error), r.LogPath,
	)
	if err != nil {
		return fmt.Errorf("session: insert run: %w", err)
	}
	return nil
}

// GetLatestRun returns the newest run for a session by started_at, or
// (nil, nil) when the session has never run.
func (s *Store) GetLatestRun(ctx context.Context, sessionID string) (*Run, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT id, session_id, status, started_at, ended_at, usage_json, error, log_path
		   FROM runs WHERE session_id = ? ORDER BY started_at DESC, id DESC LIMIT 1`,
		sessionID,
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
		return fmt.Errorf("session: update run terminal: %w", err)
	}
	return requireOne(res, "update run")
}

// SweepRunning is crash recovery: every run left 'running' by a crash is
// marked 'failed' (with ended_at + an interrupted error) and its session is
// flipped back to 'idle'. Returns the number of runs swept. The sandbox
// folders are left untouched (forward-only on disk).
func (s *Store) SweepRunning(ctx context.Context) (int, error) {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return 0, fmt.Errorf("session: sweep begin: %w", err)
	}
	defer tx.Rollback()

	now := s.nowStr()
	res, err := tx.ExecContext(ctx,
		`UPDATE runs SET status = ?, ended_at = ?, error = ?
		  WHERE status = ?`,
		RunFailed, now, "interrupted by restart", RunRunning,
	)
	if err != nil {
		return 0, fmt.Errorf("session: sweep runs: %w", err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return 0, fmt.Errorf("session: sweep rows: %w", err)
	}
	if _, err := tx.ExecContext(ctx,
		`UPDATE sessions SET status = ?, updated_at = ? WHERE status = ?`,
		StatusIdle, now, StatusRunning,
	); err != nil {
		return 0, fmt.Errorf("session: sweep sessions: %w", err)
	}
	if err := tx.Commit(); err != nil {
		return 0, fmt.Errorf("session: sweep commit: %w", err)
	}
	return int(n), nil
}

// --- scan helpers ---

// scanner is satisfied by both *sql.Row and *sql.Rows.
type scanner interface {
	Scan(dest ...any) error
}

func scanSession(sc scanner) (Session, error) {
	var (
		sess    Session
		name    sql.NullString
		sysProm sql.NullString
		cfgJSON string
	)
	err := sc.Scan(
		&sess.ID, &sess.OwnerEmail, &name, &sess.Prompt, &sysProm,
		&cfgJSON, &sess.Status, &sess.CreatedAt, &sess.UpdatedAt,
	)
	if errors.Is(err, sql.ErrNoRows) {
		return Session{}, ErrNotFound
	}
	if err != nil {
		return Session{}, fmt.Errorf("session: scan: %w", err)
	}
	sess.Name = name.String
	sess.SystemPrompt = sysProm.String
	cfg, err := unmarshalConfig(cfgJSON)
	if err != nil {
		return Session{}, err
	}
	sess.Config = cfg
	return sess, nil
}

func scanRun(sc scanner) (Run, error) {
	var (
		r       Run
		endedAt sql.NullString
		usage   sql.NullString
		errMsg  sql.NullString
	)
	err := sc.Scan(
		&r.ID, &r.SessionID, &r.Status, &r.StartedAt,
		&endedAt, &usage, &errMsg, &r.LogPath,
	)
	if errors.Is(err, sql.ErrNoRows) {
		return Run{}, ErrNotFound
	}
	if err != nil {
		return Run{}, fmt.Errorf("session: scan run: %w", err)
	}
	r.EndedAt = endedAt.String
	r.UsageJSON = usage.String
	r.Error = errMsg.String
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
		return fmt.Errorf("session: %s rows: %w", op, err)
	}
	if n == 0 {
		return ErrNotFound
	}
	return nil
}
