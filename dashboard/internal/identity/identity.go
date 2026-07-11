// Package identity persists the durable local handles for OIDC identities.
package identity

import (
	"context"
	"database/sql"
	"fmt"
	"time"

	"dashboard/internal/ids"
)

// Claims are the identity-bearing id_token fields captured at login.
type Claims struct {
	Iss, Sub, Email, Name, Picture string
}

// Identity is a stored identities row.
type Identity struct {
	ID, Iss, Sub, Email, Name, Picture string
}

// Store resolves OIDC subject pairs to durable local identity handles.
type Store struct {
	DB  *sql.DB
	Now func() time.Time
	New func() string
}

// NewStore constructs an identity Store over db.
func NewStore(db *sql.DB) *Store {
	return &Store{DB: db, Now: time.Now, New: ids.New}
}

// ResolveOrCreate upserts an identity by its issuer and subject and returns its
// durable local handle. Attributes always reflect the latest login.
func (s *Store) ResolveOrCreate(ctx context.Context, c Claims) (string, error) {
	now := s.Now().UTC().Format(time.RFC3339Nano)
	row := s.DB.QueryRowContext(ctx, `
		INSERT INTO identities (id, iss, sub, email, name, picture, created_at, updated_at)
		VALUES (?, ?, ?, ?, NULLIF(?, ''), NULLIF(?, ''), ?, ?)
		ON CONFLICT (iss, sub) DO UPDATE SET
			email = excluded.email,
			name = excluded.name,
			picture = excluded.picture,
			updated_at = excluded.updated_at
		RETURNING id
	`, s.New(), c.Iss, c.Sub, c.Email, c.Name, c.Picture, now, now)
	var id string
	if err := row.Scan(&id); err != nil {
		return "", fmt.Errorf("resolve identity: %w", err)
	}
	return id, nil
}

// Lookup returns the identity named by its durable handle. An unknown handle
// returns sql.ErrNoRows so callers can distinguish absence from an empty row.
func (s *Store) Lookup(ctx context.Context, id string) (Identity, error) {
	row := s.DB.QueryRowContext(ctx, `
		SELECT id, iss, sub, email, name, picture
		FROM identities WHERE id = ?
	`, id)
	var (
		identity Identity
		name     sql.NullString
		picture  sql.NullString
	)
	if err := row.Scan(&identity.ID, &identity.Iss, &identity.Sub, &identity.Email, &name, &picture); err != nil {
		if err == sql.ErrNoRows {
			return Identity{}, err
		}
		return Identity{}, fmt.Errorf("lookup identity: %w", err)
	}
	identity.Name = name.String
	identity.Picture = picture.String
	return identity, nil
}
