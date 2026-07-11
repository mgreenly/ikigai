// Package oauth is the OAuth authorization-server data layer: DCR clients,
// chains, tokens, and authorization codes. The HTTP endpoints that drive these
// stores live in internal/server.
package oauth

import (
	"errors"
	"time"
)

const (
	// Token plaintext prefixes for access and refresh tokens.
	AccessPrefix  = "ms_oat_"
	RefreshPrefix = "ms_ort_"
)

const (
	KindAccess  = "access"
	KindRefresh = "refresh"
)

var (
	ErrNotFound         = errors.New("oauth: not found")
	ErrExpired          = errors.New("oauth: expired")
	ErrConsumed         = errors.New("oauth: already consumed")
	ErrRevoked          = errors.New("oauth: revoked")
	ErrUsed             = errors.New("oauth: already used")
	ErrBadPrefix        = errors.New("oauth: bad credential prefix")
	ErrClientMismatch   = errors.New("oauth: client mismatch")
	ErrPKCEMismatch     = errors.New("oauth: pkce mismatch")
	ErrRedirectMismatch = errors.New("oauth: redirect_uri mismatch")
	ErrResourceMismatch = errors.New("oauth: resource mismatch")
)

// Chain represents an oauth_chains row.
type Chain struct {
	ID         string
	PublicID   string
	ClientID   string
	OwnerEmail string
	OwnerID    string
	Resource   string
	CreatedAt  time.Time
	RevokedAt  *time.Time
}

// Token represents an oauth_tokens row (without the plaintext — only the
// hash is persisted).
type Token struct {
	ID        string
	ChainID   string
	Kind      string
	IssuedAt  time.Time
	ExpiresAt time.Time
	UsedAt    *time.Time
	RevokedAt *time.Time
}

// AuthCode represents an oauth_authcodes row.
type AuthCode struct {
	ID                  string
	ClientID            string
	OwnerEmail          string
	OwnerID             string
	CodeChallenge       string
	CodeChallengeMethod string
	RedirectURI         string
	Resource            string
	OriginalState       string
	IssuedAt            time.Time
	ExpiresAt           time.Time
	UsedAt              *time.Time
	ChainID             *string
}

// Client represents a dcr_clients row.
type Client struct {
	ClientID     string
	ClientName   string
	RedirectURIs []string
	RegisteredAt time.Time
	LastUsedAt   *time.Time
}

// ValidatedToken is what the validate path returns: the token record plus
// the chain it belongs to.
type ValidatedToken struct {
	Token Token
	Chain Chain
}
