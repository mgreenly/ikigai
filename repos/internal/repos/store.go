// Package repos owns repository and agent-session persistence.
package repos

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"time"
)

var ErrNotFound = errors.New("repos: not found")

type Store struct {
	db *sql.DB
}

func NewStore(db *sql.DB) *Store { return &Store{db: db} }

func (s *Store) InsertRepo(ctx context.Context, repo Repo) error {
	_, err := s.db.ExecContext(ctx, `INSERT INTO repos
		(name, owner_email, clone_url, default_branch, created_at)
		VALUES (?, ?, ?, ?, ?)`, repo.Name, repo.OwnerEmail, repo.CloneURL,
		repo.DefaultBranch, formatTime(repo.CreatedAt))
	return wrap("insert repo", err)
}

func (s *Store) GetRepo(ctx context.Context, name string) (Repo, error) {
	var repo Repo
	var created string
	err := s.db.QueryRowContext(ctx, `SELECT name, owner_email, clone_url,
		default_branch, created_at FROM repos WHERE name = ?`, name).Scan(
		&repo.Name, &repo.OwnerEmail, &repo.CloneURL, &repo.DefaultBranch, &created)
	if err != nil {
		return Repo{}, rowError("get repo", err)
	}
	repo.CreatedAt, err = parseTime(created)
	return repo, wrap("parse repo created_at", err)
}

func (s *Store) ListRepos(ctx context.Context, owner string) ([]Repo, error) {
	query := `SELECT name, owner_email, clone_url, default_branch, created_at FROM repos`
	var args []any
	if owner != "" {
		query += ` WHERE owner_email = ?`
		args = append(args, owner)
	}
	query += ` ORDER BY name`
	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, fmt.Errorf("list repos: %w", err)
	}
	defer rows.Close()
	var result []Repo
	for rows.Next() {
		var repo Repo
		var created string
		if err := rows.Scan(&repo.Name, &repo.OwnerEmail, &repo.CloneURL, &repo.DefaultBranch, &created); err != nil {
			return nil, fmt.Errorf("scan repo: %w", err)
		}
		repo.CreatedAt, err = parseTime(created)
		if err != nil {
			return nil, fmt.Errorf("parse repo created_at: %w", err)
		}
		result = append(result, repo)
	}
	return result, wrap("list repos rows", rows.Err())
}

func (s *Store) DeleteRepo(ctx context.Context, name string) error {
	result, err := s.db.ExecContext(ctx, `DELETE FROM repos WHERE name = ?`, name)
	return mutationResult("delete repo", result, err)
}

func (s *Store) InsertSession(ctx context.Context, session Session) error {
	_, err := s.db.ExecContext(ctx, `INSERT INTO sessions
		(id, repo_name, owner_email, issue_number, attempt, branch, instructions,
		 status, error, pr_url, created_at, started_at, ended_at, log_path)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		session.ID, session.RepoName, session.OwnerEmail, nullableInt(session.IssueNumber),
		session.Attempt, session.Branch, session.Instructions, session.Status,
		nullableString(session.Error), nullableString(session.PRURL), formatTime(session.CreatedAt),
		nullableTime(session.StartedAt), nullableTime(session.EndedAt), session.LogPath)
	return wrap("insert session", err)
}

func (s *Store) GetSession(ctx context.Context, id string) (Session, error) {
	row := s.db.QueryRowContext(ctx, sessionSelect+` WHERE id = ?`, id)
	session, err := scanSession(row)
	if err != nil {
		return Session{}, rowError("get session", err)
	}
	return session, nil
}

func (s *Store) ListSessions(ctx context.Context, repoName, owner string) ([]Session, error) {
	query := sessionSelect + ` WHERE 1=1`
	var args []any
	if repoName != "" {
		query += ` AND repo_name = ?`
		args = append(args, repoName)
	}
	if owner != "" {
		query += ` AND owner_email = ?`
		args = append(args, owner)
	}
	query += ` ORDER BY created_at, id`
	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, fmt.Errorf("list sessions: %w", err)
	}
	defer rows.Close()
	var result []Session
	for rows.Next() {
		session, err := scanSession(rows)
		if err != nil {
			return nil, fmt.Errorf("scan session: %w", err)
		}
		result = append(result, session)
	}
	return result, wrap("list sessions rows", rows.Err())
}

func (s *Store) ActiveSessionForIssue(ctx context.Context, repoName string, issue int) (Session, error) {
	row := s.db.QueryRowContext(ctx, sessionSelect+`
		WHERE repo_name = ? AND issue_number = ? AND status IN ('queued', 'running')
		ORDER BY created_at, id LIMIT 1`, repoName, issue)
	session, err := scanSession(row)
	if err != nil {
		return Session{}, rowError("active session for issue", err)
	}
	return session, nil
}

func (s *Store) CountRunning(ctx context.Context) (int, error) {
	var count int
	err := s.db.QueryRowContext(ctx, `SELECT COUNT(*) FROM sessions WHERE status = 'running'`).Scan(&count)
	return count, wrap("count running", err)
}

func (s *Store) NextQueued(ctx context.Context) (Session, error) {
	row := s.db.QueryRowContext(ctx, sessionSelect+`
		WHERE status = 'queued' ORDER BY created_at, id LIMIT 1`)
	session, err := scanSession(row)
	if err != nil {
		return Session{}, rowError("next queued", err)
	}
	return session, nil
}

func (s *Store) MarkRunning(ctx context.Context, id string, startedAt time.Time) error {
	result, err := s.db.ExecContext(ctx, `UPDATE sessions SET status = 'running', started_at = ?
		WHERE id = ? AND status = 'queued'`, formatTime(startedAt), id)
	return mutationResult("mark running", result, err)
}

// OutcomeAppender writes the outcome event on FinishSession's transaction.
type OutcomeAppender func(context.Context, *sql.Tx, Session) error

// FinishSession atomically updates a terminal session and appends its outcome.
func (s *Store) FinishSession(ctx context.Context, id, status string, sessionError, prURL *string, endedAt time.Time, appendOutcome OutcomeAppender) error {
	if !terminalStatus(status) {
		return fmt.Errorf("finish session: non-terminal status %q", status)
	}
	if appendOutcome == nil {
		return errors.New("finish session: outcome appender is required")
	}
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("finish session: begin: %w", err)
	}
	defer tx.Rollback()
	result, err := tx.ExecContext(ctx, `UPDATE sessions
		SET status = ?, error = ?, pr_url = ?, ended_at = ? WHERE id = ?`,
		status, nullableString(sessionError), nullableString(prURL), formatTime(endedAt), id)
	if err != nil {
		return fmt.Errorf("finish session: update: %w", err)
	}
	if err := requireChanged(result); err != nil {
		return fmt.Errorf("finish session: %w", err)
	}
	session, err := scanSession(tx.QueryRowContext(ctx, sessionSelect+` WHERE id = ?`, id))
	if err != nil {
		return fmt.Errorf("finish session: reload: %w", err)
	}
	if err := appendOutcome(ctx, tx, session); err != nil {
		return fmt.Errorf("finish session: append outcome: %w", err)
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("finish session: commit: %w", err)
	}
	return nil
}

func (s *Store) MaxAttempt(ctx context.Context, repoName string, issue int) (int, error) {
	var max int
	err := s.db.QueryRowContext(ctx, `SELECT COALESCE(MAX(attempt), 0) FROM sessions
		WHERE repo_name = ? AND issue_number = ?`, repoName, issue).Scan(&max)
	return max, wrap("max attempt", err)
}

// SweepRunning marks sessions interrupted by a service restart as failed.
func (s *Store) SweepRunning(ctx context.Context, endedAt time.Time, reason string) (int, error) {
	result, err := s.db.ExecContext(ctx, `UPDATE sessions SET status = 'failed', error = ?, ended_at = ?
		WHERE status = 'running'`, reason, formatTime(endedAt))
	if err != nil {
		return 0, fmt.Errorf("sweep running: %w", err)
	}
	n, err := result.RowsAffected()
	return int(n), wrap("sweep running rows affected", err)
}

const sessionSelect = `SELECT id, repo_name, owner_email, issue_number, attempt,
	branch, instructions, status, error, pr_url, created_at, started_at, ended_at, log_path
	FROM sessions`

type scanner interface{ Scan(...any) error }

func scanSession(row scanner) (Session, error) {
	var session Session
	var issue sql.NullInt64
	var sessionError, prURL, started, ended sql.NullString
	var created string
	err := row.Scan(&session.ID, &session.RepoName, &session.OwnerEmail, &issue,
		&session.Attempt, &session.Branch, &session.Instructions, &session.Status,
		&sessionError, &prURL, &created, &started, &ended, &session.LogPath)
	if err != nil {
		return Session{}, err
	}
	if issue.Valid {
		value := int(issue.Int64)
		session.IssueNumber = &value
	}
	if sessionError.Valid {
		session.Error = &sessionError.String
	}
	if prURL.Valid {
		session.PRURL = &prURL.String
	}
	session.CreatedAt, err = parseTime(created)
	if err != nil {
		return Session{}, err
	}
	if session.StartedAt, err = parseNullableTime(started); err != nil {
		return Session{}, err
	}
	if session.EndedAt, err = parseNullableTime(ended); err != nil {
		return Session{}, err
	}
	return session, nil
}

func terminalStatus(status string) bool {
	return status == StatusSucceeded || status == StatusFailed || status == StatusCancelled
}

func formatTime(value time.Time) string { return value.UTC().Format(time.RFC3339Nano) }

func parseTime(value string) (time.Time, error) { return time.Parse(time.RFC3339Nano, value) }

func nullableTime(value *time.Time) any {
	if value == nil {
		return nil
	}
	return formatTime(*value)
}

func parseNullableTime(value sql.NullString) (*time.Time, error) {
	if !value.Valid {
		return nil, nil
	}
	parsed, err := parseTime(value.String)
	if err != nil {
		return nil, err
	}
	return &parsed, nil
}

func nullableString(value *string) any {
	if value == nil {
		return nil
	}
	return *value
}

func nullableInt(value *int) any {
	if value == nil {
		return nil
	}
	return *value
}

func rowError(action string, err error) error {
	if errors.Is(err, sql.ErrNoRows) {
		return fmt.Errorf("%s: %w", action, ErrNotFound)
	}
	return wrap(action, err)
}

func mutationResult(action string, result sql.Result, err error) error {
	if err != nil {
		return fmt.Errorf("%s: %w", action, err)
	}
	if err := requireChanged(result); err != nil {
		return fmt.Errorf("%s: %w", action, err)
	}
	return nil
}

func requireChanged(result sql.Result) error {
	n, err := result.RowsAffected()
	if err != nil {
		return err
	}
	if n == 0 {
		return ErrNotFound
	}
	return nil
}

func wrap(action string, err error) error {
	if err == nil {
		return nil
	}
	return fmt.Errorf("%s: %w", action, err)
}
