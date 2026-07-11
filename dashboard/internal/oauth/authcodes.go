package oauth

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

// AuthCodeStore persists short-lived authorization codes.
type AuthCodeStore struct {
	DB  *sql.DB
	Now func() time.Time
	TTL time.Duration
}

func NewAuthCodeStore(db *sql.DB, ttl time.Duration) *AuthCodeStore {
	return &AuthCodeStore{DB: db, Now: time.Now, TTL: ttl}
}

// IssueParams carries everything bound to a fresh code.
type IssueParams struct {
	ClientID            string
	OwnerEmail          string
	OwnerID             string
	CodeChallenge       string
	CodeChallengeMethod string
	RedirectURI         string
	Resource            string
	OriginalState       string
}

// Issue mints a fresh code (plaintext returned once) and persists only its
// hash bound to client_id + PKCE challenge + redirect_uri.
func (s *AuthCodeStore) Issue(ctx context.Context, p IssueParams) (plaintext string, rec AuthCode, err error) {
	plaintext = "ms_aco_" + ids.New()
	now := s.Now().UTC()
	rec = AuthCode{
		ID:                  ids.New(),
		ClientID:            p.ClientID,
		OwnerEmail:          p.OwnerEmail,
		OwnerID:             p.OwnerID,
		CodeChallenge:       p.CodeChallenge,
		CodeChallengeMethod: p.CodeChallengeMethod,
		RedirectURI:         p.RedirectURI,
		Resource:            p.Resource,
		OriginalState:       p.OriginalState,
		IssuedAt:            now,
		ExpiresAt:           now.Add(s.TTL),
	}
	_, err = s.DB.ExecContext(ctx, `
		INSERT INTO oauth_authcodes (
			id, code_hash, client_id, owner_email, owner_id,
			code_challenge, code_challenge_method, redirect_uri,
			resource, original_state, issued_at, expires_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`,
		rec.ID, hashString(plaintext),
		rec.ClientID, rec.OwnerEmail, rec.OwnerID,
		rec.CodeChallenge, rec.CodeChallengeMethod, rec.RedirectURI,
		rec.Resource, rec.OriginalState,
		rec.IssuedAt.Format(time.RFC3339Nano), rec.ExpiresAt.Format(time.RFC3339Nano),
	)
	if err != nil {
		return "", AuthCode{}, fmt.Errorf("insert authcode: %w", err)
	}
	return plaintext, rec, nil
}

// LookupTx fetches the authcode row keyed by the plaintext's hash. Used
// inside the token-exchange transaction.
func (s *AuthCodeStore) LookupTx(ctx context.Context, tx *sql.Tx, plaintext string) (AuthCode, error) {
	row := tx.QueryRowContext(ctx, `
		SELECT id, client_id, owner_email, owner_id, code_challenge, code_challenge_method,
		       redirect_uri, resource, original_state, issued_at, expires_at,
		       used_at, chain_id
		FROM oauth_authcodes WHERE code_hash = ?
	`, hashString(plaintext))
	var (
		c       AuthCode
		issued  string
		expires string
		usedAt  sql.NullString
		chainID sql.NullString
	)
	err := row.Scan(&c.ID, &c.ClientID, &c.OwnerEmail, &c.OwnerID, &c.CodeChallenge,
		&c.CodeChallengeMethod, &c.RedirectURI, &c.Resource, &c.OriginalState,
		&issued, &expires, &usedAt, &chainID)
	if errors.Is(err, sql.ErrNoRows) {
		return AuthCode{}, ErrNotFound
	}
	if err != nil {
		return AuthCode{}, fmt.Errorf("select authcode: %w", err)
	}
	c.IssuedAt, _ = time.Parse(time.RFC3339Nano, issued)
	c.ExpiresAt, _ = time.Parse(time.RFC3339Nano, expires)
	if usedAt.Valid {
		t, _ := time.Parse(time.RFC3339Nano, usedAt.String)
		c.UsedAt = &t
	}
	if chainID.Valid {
		c.ChainID = &chainID.String
	}
	return c, nil
}

// MarkUsed marks a code as consumed inside tx, binding it to chainID.
func (s *AuthCodeStore) MarkUsed(ctx context.Context, tx *sql.Tx, codeID, chainID string) error {
	now := s.Now().UTC().Format(time.RFC3339Nano)
	_, err := tx.ExecContext(ctx, `
		UPDATE oauth_authcodes SET used_at = ?, chain_id = ?
		WHERE id = ? AND used_at IS NULL
	`, now, chainID, codeID)
	if err != nil {
		return fmt.Errorf("mark authcode used: %w", err)
	}
	return nil
}

// hashString returns the hex SHA-256 of s; used uniformly for tokens and codes.
func hashString(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:])
}
