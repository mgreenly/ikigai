// Package oauthstate persists the short-lived state that binds a redirect-to-
// Google login round-trip back to the browser that started it. The
// HandshakeStore both mints a handshake and stores it: it returns the persisted
// row plus the one secret that is never stored — the plaintext binding cookie.
package oauthstate

import (
	"context"
	"crypto/sha256"
	"crypto/subtle"
	"database/sql"
	"encoding/hex"
	"errors"
	"fmt"
	"time"

	"dashboard/internal/ids"
)

// Origin identifies where a login round-trip began. A web handshake is the
// plain browser sign-in; an mcp handshake additionally carries the originating
// MCP client's /oauth/authorize context so the callback can resume it.
const (
	OriginWeb = "web"
	OriginMCP = "mcp"
)

// Handshake is one persisted oauth_state row: the server's record of a pending
// login round-trip, awaiting its callback. It holds the SHA-256 hash of the
// binding cookie, never the cookie itself.
type Handshake struct {
	ID                string
	BindingCookieHash string
	CreatedAt         time.Time
	ExpiresAt         time.Time

	// Origin is OriginWeb or OriginMCP.
	Origin string

	// MCP-origin context. Empty for web handshakes.
	MCPClientID            string
	MCPRedirectURI         string
	MCPCodeChallenge       string
	MCPCodeChallengeMethod string
	MCPClientState         string
	MCPResource            string

	// ReturnTo is the validated same-site destination captured for web logins.
	// It is empty when no destination was supplied.
	ReturnTo string
}

// MCPContext is the originating MCP client's /oauth/authorize context, captured
// when an MCP-origin login begins and replayed when the callback resumes it.
type MCPContext struct {
	ClientID            string
	RedirectURI         string
	CodeChallenge       string
	CodeChallengeMethod string
	ClientState         string
	Resource            string
}

// HandshakeStore mints and persists Handshakes in SQLite, with each handshake
// valid for ttl.
type HandshakeStore struct {
	db  *sql.DB
	ttl time.Duration
}

// NewHandshakeStore constructs a HandshakeStore over db, minting handshakes that
// stay valid for ttl.
func NewHandshakeStore(db *sql.DB, ttl time.Duration) *HandshakeStore {
	return &HandshakeStore{db: db, ttl: ttl}
}

// Create mints a new web-origin handshake, stores it, and returns the stored
// row together with the plaintext binding cookie. The row keeps only the
// cookie's hash; the plaintext is returned here and never persisted — the
// browser gets the plaintext, the database gets the hash, and the callback
// re-hashes to compare. The INSERT omits origin, so the schema default 'web'
// applies.
func (s *HandshakeStore) Create(ctx context.Context) (Handshake, string, error) {
	cookie := ids.New()
	now := time.Now().UTC()
	handshake := Handshake{
		ID:                ids.New(),
		BindingCookieHash: hashCookie(cookie),
		CreatedAt:         now,
		ExpiresAt:         now.Add(s.ttl),
		Origin:            OriginWeb,
	}
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO oauth_state (id, binding_cookie_hash, created_at, expires_at)
		VALUES (?, ?, ?, ?)`,
		handshake.ID, handshake.BindingCookieHash,
		handshake.CreatedAt.Format(time.RFC3339Nano), handshake.ExpiresAt.Format(time.RFC3339Nano))
	if err != nil {
		return Handshake{}, "", fmt.Errorf("insert oauth_state: %w", err)
	}
	return handshake, cookie, nil
}

// CreateWeb mints a new web-origin handshake with a validated same-site
// destination. Validation belongs to the HTTP boundary; this store persists the
// supplied value verbatim alongside the otherwise ordinary web handshake.
func (s *HandshakeStore) CreateWeb(ctx context.Context, returnTo string) (Handshake, string, error) {
	cookie := ids.New()
	now := time.Now().UTC()
	handshake := Handshake{
		ID:                ids.New(),
		BindingCookieHash: hashCookie(cookie),
		CreatedAt:         now,
		ExpiresAt:         now.Add(s.ttl),
		Origin:            OriginWeb,
		ReturnTo:          returnTo,
	}
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO oauth_state (id, binding_cookie_hash, created_at, expires_at, return_to)
		VALUES (?, ?, ?, ?, ?)`,
		handshake.ID, handshake.BindingCookieHash,
		handshake.CreatedAt.Format(time.RFC3339Nano), handshake.ExpiresAt.Format(time.RFC3339Nano),
		handshake.ReturnTo)
	if err != nil {
		return Handshake{}, "", fmt.Errorf("insert oauth_state: %w", err)
	}
	return handshake, cookie, nil
}

// CreateMCP mints a new MCP-origin handshake, mirroring Create but also
// persisting the originating MCP client's /oauth/authorize context. It returns
// the stored row (with Origin=OriginMCP and the MCP fields populated) and the
// plaintext binding cookie.
func (s *HandshakeStore) CreateMCP(ctx context.Context, mcp MCPContext) (Handshake, string, error) {
	cookie := ids.New()
	now := time.Now().UTC()
	handshake := Handshake{
		ID:                     ids.New(),
		BindingCookieHash:      hashCookie(cookie),
		CreatedAt:              now,
		ExpiresAt:              now.Add(s.ttl),
		Origin:                 OriginMCP,
		MCPClientID:            mcp.ClientID,
		MCPRedirectURI:         mcp.RedirectURI,
		MCPCodeChallenge:       mcp.CodeChallenge,
		MCPCodeChallengeMethod: mcp.CodeChallengeMethod,
		MCPClientState:         mcp.ClientState,
		MCPResource:            mcp.Resource,
	}
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO oauth_state (
			id, binding_cookie_hash, created_at, expires_at, origin,
			mcp_client_id, mcp_redirect_uri, mcp_code_challenge,
			mcp_code_challenge_method, mcp_client_state, mcp_resource)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		handshake.ID, handshake.BindingCookieHash,
		handshake.CreatedAt.Format(time.RFC3339Nano), handshake.ExpiresAt.Format(time.RFC3339Nano),
		handshake.Origin,
		handshake.MCPClientID, handshake.MCPRedirectURI, handshake.MCPCodeChallenge,
		handshake.MCPCodeChallengeMethod, handshake.MCPClientState, handshake.MCPResource)
	if err != nil {
		return Handshake{}, "", fmt.Errorf("insert oauth_state: %w", err)
	}
	return handshake, cookie, nil
}

// Consume's failure modes, distinguished so the caller can tell a replay
// (deleted/unknown) from an expiry from a forged cookie — all of which the
// handler treats as a 400, but which it logs differently.
var (
	ErrHandshakeNotFound = errors.New("oauthstate: handshake not found")
	ErrHandshakeExpired  = errors.New("oauthstate: handshake expired")
	ErrBindingMismatch   = errors.New("oauthstate: binding cookie mismatch")
)

// Consume validates and single-use-consumes the handshake named by id: it looks
// the row up, checks it is unexpired and that cookie hashes to the stored
// binding hash, then deletes the row so a replay finds nothing. The hash and
// expiry are checked before the delete, and the whole sequence runs in one
// transaction so a concurrent replay cannot slip between the check and the
// delete. The binding-hash compare is constant-time. The returned handshake
// carries its origin and (for MCP origins) the captured MCP context, so the
// caller can branch on it.
func (s *HandshakeStore) Consume(ctx context.Context, id, cookie string) (Handshake, error) {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return Handshake{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	handshake := Handshake{ID: id}
	var createdAt, expiresAt string
	var (
		mcpClientID            sql.NullString
		mcpRedirectURI         sql.NullString
		mcpCodeChallenge       sql.NullString
		mcpCodeChallengeMethod sql.NullString
		mcpClientState         sql.NullString
		mcpResource            sql.NullString
		returnTo               sql.NullString
	)
	err = tx.QueryRowContext(ctx, `
		SELECT binding_cookie_hash, created_at, expires_at, origin,
			mcp_client_id, mcp_redirect_uri, mcp_code_challenge,
			mcp_code_challenge_method, mcp_client_state, mcp_resource, return_to
		FROM oauth_state WHERE id = ?`, id).
		Scan(&handshake.BindingCookieHash, &createdAt, &expiresAt, &handshake.Origin,
			&mcpClientID, &mcpRedirectURI, &mcpCodeChallenge,
			&mcpCodeChallengeMethod, &mcpClientState, &mcpResource, &returnTo)
	if errors.Is(err, sql.ErrNoRows) {
		return Handshake{}, ErrHandshakeNotFound
	}
	if err != nil {
		return Handshake{}, fmt.Errorf("select oauth_state: %w", err)
	}
	if handshake.CreatedAt, err = time.Parse(time.RFC3339Nano, createdAt); err != nil {
		return Handshake{}, fmt.Errorf("parse created_at: %w", err)
	}
	if handshake.ExpiresAt, err = time.Parse(time.RFC3339Nano, expiresAt); err != nil {
		return Handshake{}, fmt.Errorf("parse expires_at: %w", err)
	}
	handshake.MCPClientID = mcpClientID.String
	handshake.MCPRedirectURI = mcpRedirectURI.String
	handshake.MCPCodeChallenge = mcpCodeChallenge.String
	handshake.MCPCodeChallengeMethod = mcpCodeChallengeMethod.String
	handshake.MCPClientState = mcpClientState.String
	handshake.MCPResource = mcpResource.String
	handshake.ReturnTo = returnTo.String

	if time.Now().UTC().After(handshake.ExpiresAt) {
		return Handshake{}, ErrHandshakeExpired
	}
	if subtle.ConstantTimeCompare([]byte(hashCookie(cookie)), []byte(handshake.BindingCookieHash)) != 1 {
		return Handshake{}, ErrBindingMismatch
	}

	if _, err := tx.ExecContext(ctx, `DELETE FROM oauth_state WHERE id = ?`, id); err != nil {
		return Handshake{}, fmt.Errorf("delete oauth_state: %w", err)
	}
	if err := tx.Commit(); err != nil {
		return Handshake{}, fmt.Errorf("commit: %w", err)
	}
	return handshake, nil
}

func hashCookie(plain string) string {
	sum := sha256.Sum256([]byte(plain))
	return hex.EncodeToString(sum[:])
}
