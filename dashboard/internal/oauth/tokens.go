package oauth

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"

	"dashboard/internal/ids"
)

// TokenStore mints and validates chains, access tokens, and refresh tokens.
type TokenStore struct {
	DB         *sql.DB
	Now        func() time.Time
	AccessTTL  time.Duration
	RefreshTTL time.Duration
}

func NewTokenStore(db *sql.DB, accessTTL, refreshTTL time.Duration) *TokenStore {
	return &TokenStore{DB: db, Now: time.Now, AccessTTL: accessTTL, RefreshTTL: refreshTTL}
}

// TokenPair is what IssueChainAndTokens returns to the caller building the
// token-endpoint response.
type TokenPair struct {
	ChainID          string
	ChainPublicID    string
	AccessToken      string
	RefreshToken     string
	AccessExpiresAt  time.Time
	RefreshExpiresAt time.Time
}

// IssueChainAndTokens creates a fresh chain row and a fresh access/refresh
// pair, all inside tx. The caller commits.
func (s *TokenStore) IssueChainAndTokens(ctx context.Context, tx *sql.Tx, clientID, ownerEmail, ownerID, resource string) (TokenPair, error) {
	now := s.Now().UTC()
	chainID := ids.New()
	publicID := ids.New()
	_, err := tx.ExecContext(ctx, `
		INSERT INTO oauth_chains (id, public_id, client_id, owner_email, owner_id, resource, created_at)
		VALUES (?, ?, ?, ?, ?, ?, ?)
	`, chainID, publicID, clientID, ownerEmail, ownerID, resource, now.Format(time.RFC3339Nano))
	if err != nil {
		return TokenPair{}, fmt.Errorf("insert chain: %w", err)
	}
	access, accessExp, err := s.insertToken(ctx, tx, chainID, KindAccess, now)
	if err != nil {
		return TokenPair{}, err
	}
	refresh, refreshExp, err := s.insertToken(ctx, tx, chainID, KindRefresh, now)
	if err != nil {
		return TokenPair{}, err
	}
	return TokenPair{
		ChainID:          chainID,
		ChainPublicID:    publicID,
		AccessToken:      access,
		RefreshToken:     refresh,
		AccessExpiresAt:  accessExp,
		RefreshExpiresAt: refreshExp,
	}, nil
}

func (s *TokenStore) insertToken(ctx context.Context, tx *sql.Tx, chainID, kind string, now time.Time) (plaintext string, expiresAt time.Time, err error) {
	var prefix string
	var ttl time.Duration
	switch kind {
	case KindAccess:
		prefix = AccessPrefix
		ttl = s.AccessTTL
	case KindRefresh:
		prefix = RefreshPrefix
		ttl = s.RefreshTTL
	default:
		return "", time.Time{}, fmt.Errorf("unknown token kind %q", kind)
	}
	plaintext = prefix + ids.New() + ids.New()
	expiresAt = now.Add(ttl)
	_, err = tx.ExecContext(ctx, `
		INSERT INTO oauth_tokens (id, chain_id, kind, token_hash, issued_at, expires_at)
		VALUES (?, ?, ?, ?, ?, ?)
	`, ids.New(), chainID, kind, hashString(plaintext),
		now.Format(time.RFC3339Nano), expiresAt.Format(time.RFC3339Nano))
	if err != nil {
		return "", time.Time{}, fmt.Errorf("insert token: %w", err)
	}
	return plaintext, expiresAt, nil
}

// ValidateAccess looks up an access-token plaintext and returns the record
// and chain iff all of the validity conditions hold. The unwanted-prefix
// case returns ErrBadPrefix.
func (s *TokenStore) ValidateAccess(ctx context.Context, plaintext string) (ValidatedToken, error) {
	if !strings.HasPrefix(plaintext, AccessPrefix) {
		return ValidatedToken{}, ErrBadPrefix
	}
	return s.lookupActive(ctx, plaintext, KindAccess)
}

// LookupRefreshTx looks up a refresh token inside a transaction. Returns the
// row regardless of state — caller inspects used_at / revoked_at / expires_at
// to decide reuse vs. normal.
func (s *TokenStore) LookupRefreshTx(ctx context.Context, tx *sql.Tx, plaintext string) (Token, Chain, error) {
	if !strings.HasPrefix(plaintext, RefreshPrefix) {
		return Token{}, Chain{}, ErrBadPrefix
	}
	row := tx.QueryRowContext(ctx, `
		SELECT t.id, t.chain_id, t.kind, t.issued_at, t.expires_at, t.used_at, t.revoked_at,
		       c.id, c.public_id, c.client_id, c.owner_email, c.owner_id, c.resource, c.created_at, c.revoked_at
		FROM oauth_tokens t JOIN oauth_chains c ON t.chain_id = c.id
		WHERE t.token_hash = ? AND t.kind = ?
	`, hashString(plaintext), KindRefresh)
	return scanTokenChainRow(row)
}

func (s *TokenStore) lookupActive(ctx context.Context, plaintext, kind string) (ValidatedToken, error) {
	row := s.DB.QueryRowContext(ctx, `
		SELECT t.id, t.chain_id, t.kind, t.issued_at, t.expires_at, t.used_at, t.revoked_at,
		       c.id, c.public_id, c.client_id, c.owner_email, c.owner_id, c.resource, c.created_at, c.revoked_at
		FROM oauth_tokens t JOIN oauth_chains c ON t.chain_id = c.id
		WHERE t.token_hash = ? AND t.kind = ?
	`, hashString(plaintext), kind)
	tok, chain, err := scanTokenChainRow(row)
	if err != nil {
		return ValidatedToken{}, err
	}
	if tok.RevokedAt != nil || chain.RevokedAt != nil {
		return ValidatedToken{}, ErrRevoked
	}
	if !s.Now().UTC().Before(tok.ExpiresAt) {
		return ValidatedToken{}, ErrExpired
	}
	if kind == KindRefresh && tok.UsedAt != nil {
		return ValidatedToken{}, ErrUsed
	}
	return ValidatedToken{Token: tok, Chain: chain}, nil
}

type scannable interface {
	Scan(dst ...any) error
}

func scanTokenChainRow(row scannable) (Token, Chain, error) {
	var (
		tok       Token
		chain     Chain
		issuedAt  string
		expiresAt string
		usedAt    sql.NullString
		tokRev    sql.NullString
		chainCre  string
		chainRev  sql.NullString
	)
	err := row.Scan(
		&tok.ID, &tok.ChainID, &tok.Kind, &issuedAt, &expiresAt, &usedAt, &tokRev,
		&chain.ID, &chain.PublicID, &chain.ClientID, &chain.OwnerEmail, &chain.OwnerID, &chain.Resource, &chainCre, &chainRev,
	)
	if errors.Is(err, sql.ErrNoRows) {
		return Token{}, Chain{}, ErrNotFound
	}
	if err != nil {
		return Token{}, Chain{}, fmt.Errorf("scan token+chain: %w", err)
	}
	tok.IssuedAt, _ = time.Parse(time.RFC3339Nano, issuedAt)
	tok.ExpiresAt, _ = time.Parse(time.RFC3339Nano, expiresAt)
	if usedAt.Valid {
		t, _ := time.Parse(time.RFC3339Nano, usedAt.String)
		tok.UsedAt = &t
	}
	if tokRev.Valid {
		t, _ := time.Parse(time.RFC3339Nano, tokRev.String)
		tok.RevokedAt = &t
	}
	chain.CreatedAt, _ = time.Parse(time.RFC3339Nano, chainCre)
	if chainRev.Valid {
		t, _ := time.Parse(time.RFC3339Nano, chainRev.String)
		chain.RevokedAt = &t
	}
	return tok, chain, nil
}

// MarkRefreshUsed marks a single refresh token as used.
func (s *TokenStore) MarkRefreshUsed(ctx context.Context, tx *sql.Tx, tokenID string) error {
	now := s.Now().UTC().Format(time.RFC3339Nano)
	_, err := tx.ExecContext(ctx, `UPDATE oauth_tokens SET used_at = ? WHERE id = ? AND used_at IS NULL`, now, tokenID)
	if err != nil {
		return fmt.Errorf("mark refresh used: %w", err)
	}
	return nil
}

// IssueSuccessorTokensTx mints a new access/refresh pair on an existing chain.
func (s *TokenStore) IssueSuccessorTokensTx(ctx context.Context, tx *sql.Tx, chainID string) (TokenPair, error) {
	now := s.Now().UTC()
	access, accessExp, err := s.insertToken(ctx, tx, chainID, KindAccess, now)
	if err != nil {
		return TokenPair{}, err
	}
	refresh, refreshExp, err := s.insertToken(ctx, tx, chainID, KindRefresh, now)
	if err != nil {
		return TokenPair{}, err
	}
	return TokenPair{
		ChainID:          chainID,
		AccessToken:      access,
		RefreshToken:     refresh,
		AccessExpiresAt:  accessExp,
		RefreshExpiresAt: refreshExp,
	}, nil
}

// RevokeChain writes revoked_at on the chain and every token row that does
// not already carry one. Idempotent.
func (s *TokenStore) RevokeChain(ctx context.Context, chainID string) error {
	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()
	if err := s.RevokeChainTx(ctx, tx, chainID); err != nil {
		return err
	}
	return tx.Commit()
}

// RevokeChainTx is the in-transaction variant used by reuse cascades.
func (s *TokenStore) RevokeChainTx(ctx context.Context, tx *sql.Tx, chainID string) error {
	now := s.Now().UTC().Format(time.RFC3339Nano)
	if _, err := tx.ExecContext(ctx, `UPDATE oauth_chains SET revoked_at = ? WHERE id = ? AND revoked_at IS NULL`, now, chainID); err != nil {
		return fmt.Errorf("revoke chain: %w", err)
	}
	if _, err := tx.ExecContext(ctx, `UPDATE oauth_tokens SET revoked_at = ? WHERE chain_id = ? AND revoked_at IS NULL`, now, chainID); err != nil {
		return fmt.Errorf("revoke chain tokens: %w", err)
	}
	return nil
}

// ChainSummary is what the agents block renders per chain.
type ChainSummary struct {
	PublicID   string
	ClientID   string
	ClientName string // nullable in dcr_clients; empty string when absent
	CreatedAt  time.Time
	LastUsedAt time.Time // MAX(issued_at) across the chain's tokens
}

// ListChainsByOwner returns the live (non-revoked) chains owned by
// ownerEmail, joined with dcr_clients for the client_name and with
// oauth_tokens for the most recent issued_at. Ordered most-recent first.
func (s *TokenStore) ListChainsByOwner(ctx context.Context, ownerEmail string) ([]ChainSummary, error) {
	rows, err := s.DB.QueryContext(ctx, `
		SELECT c.public_id, c.client_id, COALESCE(d.client_name, ''), c.created_at,
		       COALESCE((SELECT MAX(t.issued_at) FROM oauth_tokens t WHERE t.chain_id = c.id), c.created_at)
		FROM oauth_chains c
		LEFT JOIN dcr_clients d ON d.client_id = c.client_id
		WHERE c.owner_email = ? AND c.revoked_at IS NULL
		ORDER BY c.created_at DESC
	`, ownerEmail)
	if err != nil {
		return nil, fmt.Errorf("list chains: %w", err)
	}
	defer rows.Close()
	var out []ChainSummary
	for rows.Next() {
		var (
			cs                ChainSummary
			created, lastUsed string
		)
		if err := rows.Scan(&cs.PublicID, &cs.ClientID, &cs.ClientName, &created, &lastUsed); err != nil {
			return nil, fmt.Errorf("scan chain: %w", err)
		}
		cs.CreatedAt, _ = time.Parse(time.RFC3339Nano, created)
		cs.LastUsedAt, _ = time.Parse(time.RFC3339Nano, lastUsed)
		out = append(out, cs)
	}
	return out, rows.Err()
}

// GetChainByPublicID returns a chain row by its user-facing public_id.
// Callers compare owner_email to enforce per-visitor scope.
func (s *TokenStore) GetChainByPublicID(ctx context.Context, publicID string) (Chain, error) {
	row := s.DB.QueryRowContext(ctx, `SELECT id, public_id, client_id, owner_email, owner_id, resource, created_at, revoked_at FROM oauth_chains WHERE public_id = ?`, publicID)
	var (
		c       Chain
		created string
		revoked sql.NullString
	)
	err := row.Scan(&c.ID, &c.PublicID, &c.ClientID, &c.OwnerEmail, &c.OwnerID, &c.Resource, &created, &revoked)
	if errors.Is(err, sql.ErrNoRows) {
		return Chain{}, ErrNotFound
	}
	if err != nil {
		return Chain{}, fmt.Errorf("select chain: %w", err)
	}
	c.CreatedAt, _ = time.Parse(time.RFC3339Nano, created)
	if revoked.Valid {
		t, _ := time.Parse(time.RFC3339Nano, revoked.String)
		c.RevokedAt = &t
	}
	return c, nil
}

// GetChain returns a chain row by id.
func (s *TokenStore) GetChain(ctx context.Context, chainID string) (Chain, error) {
	row := s.DB.QueryRowContext(ctx, `SELECT id, public_id, client_id, owner_email, owner_id, resource, created_at, revoked_at FROM oauth_chains WHERE id = ?`, chainID)
	var (
		c       Chain
		created string
		revoked sql.NullString
	)
	err := row.Scan(&c.ID, &c.PublicID, &c.ClientID, &c.OwnerEmail, &c.OwnerID, &c.Resource, &created, &revoked)
	if errors.Is(err, sql.ErrNoRows) {
		return Chain{}, ErrNotFound
	}
	if err != nil {
		return Chain{}, fmt.Errorf("select chain: %w", err)
	}
	c.CreatedAt, _ = time.Parse(time.RFC3339Nano, created)
	if revoked.Valid {
		t, _ := time.Parse(time.RFC3339Nano, revoked.String)
		c.RevokedAt = &t
	}
	return c, nil
}
