// Package pat is the Personal Access Token data layer: owner-minted,
// cross-service opaque bearer tokens. A PAT is a sibling of the OAuth access
// token at the auth gate, persisted in its own personal_tokens table and never
// part of the OAuth chain model. Only the SHA-256 hash of the plaintext is
// stored; the plaintext (prefix ms_pat_) exists for exactly one moment, in
// Create's return value. The HTTP endpoints and auth-gate branch that drive
// this store live in internal/server.
package pat

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"
	"time"

	"dashboard/internal/ids"
)

// Prefix is the plaintext prefix for a PAT, alongside ms_oat_ (access) and
// ms_ort_ (refresh). It is how the auth gate forks token validation.
const Prefix = "ms_pat_"

var (
	ErrNotFound  = errors.New("pat: not found")
	ErrRevoked   = errors.New("pat: revoked")
	ErrExpired   = errors.New("pat: expired")
	ErrBadPrefix = errors.New("pat: bad credential prefix")
)

// PAT represents a personal_tokens row (without the plaintext — only the hash
// is persisted). last_used_at and expires_at are present structurally but not
// written in v1 (ADR §D6/§D7).
type PAT struct {
	ID         string
	PublicID   string
	OwnerEmail string
	OwnerID    string
	Label      string
	CreatedAt  time.Time
	LastUsedAt *time.Time
	ExpiresAt  *time.Time
	RevokedAt  *time.Time
}

// Store mints, validates, lists, and revokes personal access tokens.
type Store struct {
	DB  *sql.DB
	Now func() time.Time
}

func NewStore(db *sql.DB) *Store {
	return &Store{DB: db, Now: time.Now}
}

// Create mints a fresh PAT (plaintext returned once) and persists only its
// hash. The plaintext is ms_pat_ + ids.New() + ids.New(), mirroring the OAuth
// token shape. expires_at is always NULL in v1 (ADR §D6). This is the only
// place the plaintext ever exists.
func (s *Store) Create(ctx context.Context, ownerEmail, ownerID, label string) (plaintext string, p PAT, err error) {
	plaintext = Prefix + ids.New() + ids.New()
	now := s.Now().UTC()
	p = PAT{
		ID:         ids.New(),
		PublicID:   ids.New(),
		OwnerEmail: ownerEmail,
		OwnerID:    ownerID,
		Label:      label,
		CreatedAt:  now,
	}
	_, err = s.DB.ExecContext(ctx, `
		INSERT INTO personal_tokens (id, public_id, owner_email, owner_id, label, token_hash, created_at, expires_at)
		VALUES (?, ?, ?, ?, ?, ?, ?, NULL)
	`, p.ID, p.PublicID, p.OwnerEmail, p.OwnerID, p.Label, hashString(plaintext), now.Format(time.RFC3339Nano))
	if err != nil {
		return "", PAT{}, fmt.Errorf("insert personal token: %w", err)
	}
	return plaintext, p, nil
}

// ValidatePAT looks up a PAT plaintext and returns the row iff it exists, is
// not revoked, and is not expired. The unwanted-prefix case returns
// ErrBadPrefix. Performs no writes (the hottest loopback path; ADR §D2/§D7).
func (s *Store) ValidatePAT(ctx context.Context, plaintext string) (PAT, error) {
	if !strings.HasPrefix(plaintext, Prefix) {
		return PAT{}, ErrBadPrefix
	}
	row := s.DB.QueryRowContext(ctx, `
		SELECT id, public_id, owner_email, owner_id, label, created_at, last_used_at, expires_at, revoked_at
		FROM personal_tokens WHERE token_hash = ?
	`, hashString(plaintext))
	p, err := scanPAT(row)
	if err != nil {
		return PAT{}, err
	}
	if p.RevokedAt != nil {
		return PAT{}, ErrRevoked
	}
	if p.ExpiresAt != nil && !s.Now().UTC().Before(*p.ExpiresAt) {
		return PAT{}, ErrExpired
	}
	return p, nil
}

// ListByOwner returns the live (non-revoked) PATs owned by ownerEmail, newest
// first.
func (s *Store) ListByOwner(ctx context.Context, ownerEmail string) ([]PAT, error) {
	rows, err := s.DB.QueryContext(ctx, `
		SELECT id, public_id, owner_email, owner_id, label, created_at, last_used_at, expires_at, revoked_at
		FROM personal_tokens
		WHERE owner_email = ? AND revoked_at IS NULL
		ORDER BY created_at DESC
	`, ownerEmail)
	if err != nil {
		return nil, fmt.Errorf("list personal tokens: %w", err)
	}
	defer rows.Close()
	var out []PAT
	for rows.Next() {
		p, err := scanPAT(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, p)
	}
	return out, rows.Err()
}

// GetByPublicID returns a PAT row by its user-facing public_id. Callers compare
// owner_email to enforce per-visitor scope.
func (s *Store) GetByPublicID(ctx context.Context, publicID string) (PAT, error) {
	row := s.DB.QueryRowContext(ctx, `
		SELECT id, public_id, owner_email, owner_id, label, created_at, last_used_at, expires_at, revoked_at
		FROM personal_tokens WHERE public_id = ?
	`, publicID)
	return scanPAT(row)
}

// Revoke sets revoked_at on the PAT if it is not already revoked. Idempotent.
func (s *Store) Revoke(ctx context.Context, id string) error {
	now := s.Now().UTC().Format(time.RFC3339Nano)
	if _, err := s.DB.ExecContext(ctx, `UPDATE personal_tokens SET revoked_at = ? WHERE id = ? AND revoked_at IS NULL`, now, id); err != nil {
		return fmt.Errorf("revoke personal token: %w", err)
	}
	return nil
}

type scannable interface {
	Scan(dst ...any) error
}

func scanPAT(row scannable) (PAT, error) {
	var (
		p          PAT
		created    string
		lastUsedAt sql.NullString
		expiresAt  sql.NullString
		revokedAt  sql.NullString
	)
	err := row.Scan(&p.ID, &p.PublicID, &p.OwnerEmail, &p.OwnerID, &p.Label, &created, &lastUsedAt, &expiresAt, &revokedAt)
	if errors.Is(err, sql.ErrNoRows) {
		return PAT{}, ErrNotFound
	}
	if err != nil {
		return PAT{}, fmt.Errorf("scan personal token: %w", err)
	}
	p.CreatedAt, _ = time.Parse(time.RFC3339Nano, created)
	if lastUsedAt.Valid {
		t, _ := time.Parse(time.RFC3339Nano, lastUsedAt.String)
		p.LastUsedAt = &t
	}
	if expiresAt.Valid {
		t, _ := time.Parse(time.RFC3339Nano, expiresAt.String)
		p.ExpiresAt = &t
	}
	if revokedAt.Valid {
		t, _ := time.Parse(time.RFC3339Nano, revokedAt.String)
		p.RevokedAt = &t
	}
	return p, nil
}

// hashString returns the hex SHA-256 of s. Duplicated from internal/oauth (a
// trivial helper deliberately not extracted, to keep pat decoupled).
func hashString(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:])
}
