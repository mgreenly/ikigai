package session

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
)

// Default in-memory fire-and-run knobs used when an MCP caller does not pin
// them explicitly. max_staleness bounds how old an occurrence may be before the
// consumer skips it (coalescing a replay storm down to the freshest tick);
// max_attempts caps the fixed-delay retry of a failed fire.
const (
	DefaultMaxStalenessSecs = 300 // 5m
	DefaultMaxAttempts      = 3
)

// Trigger mirrors a session_triggers row: a session's 1:1 link to a cron event
// type. trigger_event is the full event type the session listens for, e.g.
// "cron.nightly". The linkage lives in prompts (cron is subscriber-blind).
type Trigger struct {
	SessionID        string `json:"session_id"`
	TriggerEvent     string `json:"trigger_event"`
	MaxStalenessSecs int    `json:"max_staleness_secs"`
	MaxAttempts      int    `json:"max_attempts"`
	CreatedAt        string `json:"created_at"`
	UpdatedAt        string `json:"updated_at"`
}

// SetTrigger upserts the (1:1) trigger for a session. A second call for the same
// session_id REPLACES the prior trigger (one trigger per session). created_at is
// preserved across a replace; updated_at advances. Not owner-scoped: ownership
// is enforced by the service before this is called.
func (s *Store) SetTrigger(ctx context.Context, t Trigger) error {
	now := s.nowStr()
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO session_triggers
		   (session_id, trigger_event, max_staleness_secs, max_attempts, created_at, updated_at)
		 VALUES (?, ?, ?, ?, ?, ?)
		 ON CONFLICT(session_id) DO UPDATE SET
		   trigger_event      = excluded.trigger_event,
		   max_staleness_secs = excluded.max_staleness_secs,
		   max_attempts       = excluded.max_attempts,
		   updated_at         = excluded.updated_at`,
		t.SessionID, t.TriggerEvent, t.MaxStalenessSecs, t.MaxAttempts, now, now,
	)
	if err != nil {
		return fmt.Errorf("session: set trigger: %w", err)
	}
	return nil
}

// ClearTrigger removes a session's trigger. A no-match returns ErrNotFound so
// the caller can distinguish "had no trigger" from a successful clear.
func (s *Store) ClearTrigger(ctx context.Context, sessionID string) error {
	res, err := s.db.ExecContext(ctx,
		`DELETE FROM session_triggers WHERE session_id = ?`, sessionID,
	)
	if err != nil {
		return fmt.Errorf("session: clear trigger: %w", err)
	}
	return requireOne(res, "clear trigger")
}

// GetTrigger returns a session's trigger, or ErrNotFound when absent.
func (s *Store) GetTrigger(ctx context.Context, sessionID string) (Trigger, error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT session_id, trigger_event, max_staleness_secs, max_attempts, created_at, updated_at
		   FROM session_triggers WHERE session_id = ?`,
		sessionID,
	)
	var t Trigger
	err := row.Scan(&t.SessionID, &t.TriggerEvent, &t.MaxStalenessSecs, &t.MaxAttempts, &t.CreatedAt, &t.UpdatedAt)
	if errors.Is(err, sql.ErrNoRows) {
		return Trigger{}, ErrNotFound
	}
	if err != nil {
		return Trigger{}, fmt.Errorf("session: scan trigger: %w", err)
	}
	return t, nil
}

// TriggersForEvent returns every trigger whose trigger_event matches the given
// event type — the event→sessions fan-out the consumer runs on each cron.<name>
// arrival. Uses the trigger_event index.
func (s *Store) TriggersForEvent(ctx context.Context, eventType string) ([]Trigger, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT session_id, trigger_event, max_staleness_secs, max_attempts, created_at, updated_at
		   FROM session_triggers WHERE trigger_event = ?`,
		eventType,
	)
	if err != nil {
		return nil, fmt.Errorf("session: triggers for event: %w", err)
	}
	defer rows.Close()
	var out []Trigger
	for rows.Next() {
		var t Trigger
		if err := rows.Scan(&t.SessionID, &t.TriggerEvent, &t.MaxStalenessSecs, &t.MaxAttempts, &t.CreatedAt, &t.UpdatedAt); err != nil {
			return nil, fmt.Errorf("session: scan trigger row: %w", err)
		}
		out = append(out, t)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("session: triggers rows: %w", err)
	}
	return out, nil
}
