// Package session is the dashboard's web-session store. The cookie carries an
// opaque plaintext identifier; the table stores only its SHA-256 hash. Two
// ceilings bound a session: 1h idle (last_seen_at + idle <= now) and 12h
// absolute (expires_at <= now). Logout writes revoked_at; once set, the same
// cookie value can never be redeemed again.
package session

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"errors"
	"fmt"
	"time"

	"dashboard/internal/ids"
)

// idleTimeout and absoluteTimeout are the two session ceilings. They are policy,
// not per-instance config, so they are package constants rather than
// constructor parameters: every session on this box obeys the same limits.
const (
	idleTimeout     = 1 * time.Hour
	absoluteTimeout = 12 * time.Hour
)

// Session is one row from web_sessions: the server's record of a live login.
// It holds the SHA-256 hash of the cookie, never the cookie itself.
type Session struct {
	ID         string
	OwnerEmail string
	OwnerID    string
	IssuedAt   time.Time
	ExpiresAt  time.Time
	LastSeenAt time.Time
	RevokedAt  *time.Time
}

// SessionStore mints and persists web sessions in SQLite.
type SessionStore struct {
	db *sql.DB
}

// NewSessionStore constructs a SessionStore over db.
func NewSessionStore(db *sql.DB) *SessionStore {
	return &SessionStore{db: db}
}

// Issued is what Create returns: the plaintext cookie value the browser should
// receive (returned exactly once and never persisted), plus the session id and
// its absolute expiry.
type Issued struct {
	ID          string
	CookieValue string
	ExpiresAt   time.Time
}

// Create writes a fresh session for ownerEmail and returns the plaintext cookie
// value. The row keeps only the cookie's hash; the plaintext is returned here
// and never persisted — the browser gets the plaintext, the database gets the
// hash, and Lookup re-hashes to compare.
func (s *SessionStore) Create(ctx context.Context, ownerEmail, ownerID string) (Issued, error) {
	id := ids.New()
	cookie := ids.New()
	now := time.Now().UTC()
	exp := now.Add(absoluteTimeout)
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO web_sessions (id, owner_email, owner_id, cookie_hash, issued_at, expires_at, last_seen_at)
		VALUES (?, ?, ?, ?, ?, ?, ?)`,
		id, ownerEmail, ownerID, hashCookie(cookie),
		now.Format(time.RFC3339Nano), exp.Format(time.RFC3339Nano), now.Format(time.RFC3339Nano))
	if err != nil {
		return Issued{}, fmt.Errorf("insert session: %w", err)
	}
	return Issued{ID: id, CookieValue: cookie, ExpiresAt: exp}, nil
}

// ErrInvalid covers every "this cookie does not entitle the bearer to a live
// session" cause: not found, revoked, idle-expired, absolute-expired. Wire
// callers should not distinguish, but inner code (and a future audit log) can,
// since each specific error matches ErrInvalid via errors.Is.
var (
	ErrInvalid         = errors.New("session: invalid")
	ErrNotFound        = fmt.Errorf("%w: not found", ErrInvalid)
	ErrRevoked         = fmt.Errorf("%w: revoked", ErrInvalid)
	ErrIdleExpired     = fmt.Errorf("%w: idle expired", ErrInvalid)
	ErrAbsoluteExpired = fmt.Errorf("%w: absolute expired", ErrInvalid)
)

// Lookup hashes the presented cookie value, finds the row, and returns the
// session iff it is unrevoked and within both the idle and absolute ceilings.
// On success it touches last_seen_at so the idle clock restarts.
func (s *SessionStore) Lookup(ctx context.Context, cookieValue string) (Session, error) {
	if cookieValue == "" {
		return Session{}, ErrInvalid
	}
	row := s.db.QueryRowContext(ctx, `
		SELECT id, owner_email, owner_id, issued_at, expires_at, last_seen_at, revoked_at
		FROM web_sessions WHERE cookie_hash = ?`, hashCookie(cookieValue))
	var (
		sess                        Session
		issuedAt, expires, lastSeen string
		revoked                     sql.NullString
	)
	err := row.Scan(&sess.ID, &sess.OwnerEmail, &sess.OwnerID, &issuedAt, &expires, &lastSeen, &revoked)
	if errors.Is(err, sql.ErrNoRows) {
		return Session{}, ErrNotFound
	}
	if err != nil {
		return Session{}, fmt.Errorf("select session: %w", err)
	}
	if sess.IssuedAt, err = time.Parse(time.RFC3339Nano, issuedAt); err != nil {
		return Session{}, fmt.Errorf("parse issued_at: %w", err)
	}
	if sess.ExpiresAt, err = time.Parse(time.RFC3339Nano, expires); err != nil {
		return Session{}, fmt.Errorf("parse expires_at: %w", err)
	}
	if sess.LastSeenAt, err = time.Parse(time.RFC3339Nano, lastSeen); err != nil {
		return Session{}, fmt.Errorf("parse last_seen_at: %w", err)
	}
	if revoked.Valid {
		t, perr := time.Parse(time.RFC3339Nano, revoked.String)
		if perr != nil {
			return Session{}, fmt.Errorf("parse revoked_at: %w", perr)
		}
		sess.RevokedAt = &t
		return sess, ErrRevoked
	}

	now := time.Now().UTC()
	if !now.Before(sess.ExpiresAt) {
		return sess, ErrAbsoluteExpired
	}
	if now.Sub(sess.LastSeenAt) > idleTimeout {
		return sess, ErrIdleExpired
	}

	// Touch last_seen_at — the idle clock restarts on each successful lookup.
	if _, err := s.db.ExecContext(ctx,
		`UPDATE web_sessions SET last_seen_at = ? WHERE id = ?`,
		now.Format(time.RFC3339Nano), sess.ID); err != nil {
		return Session{}, fmt.Errorf("touch session: %w", err)
	}
	sess.LastSeenAt = now
	return sess, nil
}

// Revoke writes revoked_at on the session named by the presented cookie value.
// Idempotent: a second logout is a no-op, and an unknown cookie touches nothing.
func (s *SessionStore) Revoke(ctx context.Context, cookieValue string) error {
	if cookieValue == "" {
		return nil
	}
	now := time.Now().UTC().Format(time.RFC3339Nano)
	_, err := s.db.ExecContext(ctx,
		`UPDATE web_sessions SET revoked_at = ? WHERE cookie_hash = ? AND revoked_at IS NULL`,
		now, hashCookie(cookieValue))
	if err != nil {
		return fmt.Errorf("revoke session: %w", err)
	}
	return nil
}

func hashCookie(plain string) string {
	sum := sha256.Sum256([]byte(plain))
	return hex.EncodeToString(sum[:])
}
