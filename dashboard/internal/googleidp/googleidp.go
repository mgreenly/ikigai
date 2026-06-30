// Package googleidp is the narrow seam between the dashboard and Google's
// OAuth/OIDC endpoints. Callers hold a Provider and never know which
// implementation backs it: New returns the live Google-backed provider;
// NewStub (tests) returns a network-free double.
package googleidp

import (
	"context"
	"crypto"
	"crypto/rsa"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/big"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"
)

// Provider is the seam contract. It grows one operation at a time as the login
// flow reaches for it; today the flow only needs to start federation.
type Provider interface {
	// AuthorizeURL returns the URL the browser is redirected to to begin
	// federation. state is the opaque value the state-binding contract records
	// server-side; redirectURI is the callback Google redirects back to.
	AuthorizeURL(state, redirectURI string) string

	// ExchangeCode trades Google's one-time authorization code for tokens over
	// the back channel, verifies the returned id_token, and reports the verified
	// identity. redirectURI must exactly match the one passed to AuthorizeURL.
	ExchangeCode(ctx context.Context, code, redirectURI string) (Identity, error)
}

// Identity is the verified identity ExchangeCode reports — the id_token claims
// the login flow needs. EmailVerified and HostedDomain carry the raw token
// values; the caller applies federation policy (workspace match, verified
// email), this package does not.
type Identity struct {
	Sub           string
	Email         string
	HostedDomain  string
	EmailVerified bool
}

// Credentials carries the Google OAuth client credentials the live provider
// needs. A struct, not positional arguments, so the wiring call site names each
// value.
type Credentials struct {
	ClientID        string
	ClientSecret    string
	WorkspaceDomain string
}

// google is the live Provider: it builds URLs for (and later calls) Google's
// real OAuth/OIDC endpoints. Unexported — callers hold a Provider.
type google struct {
	clientID        string
	clientSecret    string
	workspaceDomain string
	authzEndpoint   string
	tokenEndpoint   string
	jwksEndpoint    string
	issuer          string
	httpClient      *http.Client

	// JWKS cache: loaded once on the first verify, refreshed on a kid miss.
	jwksOnce  sync.Once
	jwksCache map[string]*rsa.PublicKey
	jwksErr   error
}

// httpTimeout bounds each outbound call to Google (token exchange, JWKS fetch).
const httpTimeout = 10 * time.Second

// New returns a Provider backed by Google's live endpoints.
func New(creds Credentials) Provider {
	return &google{
		clientID:        creds.ClientID,
		clientSecret:    creds.ClientSecret,
		workspaceDomain: creds.WorkspaceDomain,
		authzEndpoint:   "https://accounts.google.com/o/oauth2/v2/auth",
		tokenEndpoint:   "https://oauth2.googleapis.com/token",
		jwksEndpoint:    "https://www.googleapis.com/oauth2/v3/certs",
		issuer:          "https://accounts.google.com",
		httpClient:      &http.Client{Timeout: httpTimeout},
	}
}

// stub is a network-free Provider for tests. Its AuthorizeURL echoes state and
// redirectURI as query params on a non-routable host, so callers can assert on
// them without reaching Google.
type stub struct{}

// NewStub returns a Provider that builds deterministic URLs and never touches the
// network — the double referenced in this package's doc comment.
func NewStub() Provider {
	return stub{}
}

// AuthorizeURL returns a deterministic stand-in URL carrying state and
// redirectURI, for tests that assert on what the handler passed.
func (stub) AuthorizeURL(state, redirectURI string) string {
	q := url.Values{}
	q.Set("state", state)
	q.Set("redirect_uri", redirectURI)
	return "https://idp.stub.invalid/authorize?" + q.Encode()
}

// StubIdentity is the canned identity stub.ExchangeCode reports: a verified
// Workspace identity, so callback tests can drive the federation success path.
// Tests gate on StubIdentity.HostedDomain rather than a literal so the stub and
// the configured Workspace domain cannot silently drift apart.
var StubIdentity = Identity{
	Sub:           "stub-sub-1",
	Email:         "owner@int.ikigenba.com",
	HostedDomain:  "int.ikigenba.com",
	EmailVerified: true,
}

// ExchangeCode is the network-free double: it returns the canned StubIdentity
// without touching the network.
func (stub) ExchangeCode(ctx context.Context, code, redirectURI string) (Identity, error) {
	return StubIdentity, nil
}

// AuthorizeURL builds the Google authorization URL. The web sign-in path always
// forces fresh credentials (prompt=login) — this box has no flow that reuses an
// existing Google session.
func (g *google) AuthorizeURL(state, redirectURI string) string {
	q := url.Values{}
	q.Set("client_id", g.clientID)
	q.Set("redirect_uri", redirectURI)
	q.Set("response_type", "code")
	q.Set("scope", "openid email profile")
	q.Set("state", state)
	q.Set("hd", g.workspaceDomain)
	q.Set("access_type", "online")
	q.Set("prompt", "login")
	return g.authzEndpoint + "?" + q.Encode()
}

// ExchangeCode posts the authorization code to Google's token endpoint and
// verifies the returned id_token. It retries exactly once, and only on a 5xx;
// network errors and non-5xx statuses do not retry. The returned Identity is
// the verified token claims — federation policy (workspace, email_verified) is
// the caller's to enforce.
func (g *google) ExchangeCode(ctx context.Context, code, redirectURI string) (Identity, error) {
	form := url.Values{}
	form.Set("client_id", g.clientID)
	form.Set("client_secret", g.clientSecret)
	form.Set("code", code)
	form.Set("grant_type", "authorization_code")
	form.Set("redirect_uri", redirectURI)

	var resp *http.Response
	for attempt := 0; attempt < 2; attempt++ {
		req, err := http.NewRequestWithContext(ctx, http.MethodPost, g.tokenEndpoint, strings.NewReader(form.Encode()))
		if err != nil {
			return Identity{}, fmt.Errorf("build token request: %w", err)
		}
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		resp, err = g.httpClient.Do(req)
		if err != nil {
			return Identity{}, fmt.Errorf("token endpoint: %w", err)
		}
		if resp.StatusCode/100 != 5 {
			break
		}
		// 5xx: drain and close so the connection can be reused, then retry once.
		_, _ = io.Copy(io.Discard, resp.Body)
		resp.Body.Close()
		if attempt == 1 {
			return Identity{}, fmt.Errorf("token endpoint: %d after retry", resp.StatusCode)
		}
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return Identity{}, fmt.Errorf("token endpoint: %d: %s", resp.StatusCode, truncate(string(body), 256))
	}

	var payload struct {
		IDToken string `json:"id_token"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&payload); err != nil {
		return Identity{}, fmt.Errorf("decode token response: %w", err)
	}
	if payload.IDToken == "" {
		return Identity{}, errors.New("token response missing id_token")
	}
	return g.verifyIDToken(payload.IDToken)
}

// idTokenClaims are the id_token fields verifyIDToken reads. Aud and
// EmailVerified are decoded as `any` because Google has historically sent the
// audience as a string or array and email_verified as a bool or string.
type idTokenClaims struct {
	Iss           string `json:"iss"`
	Aud           any    `json:"aud"`
	Exp           int64  `json:"exp"`
	Sub           string `json:"sub"`
	Email         string `json:"email"`
	HostedDomain  string `json:"hd"`
	EmailVerified any    `json:"email_verified"`
}

// verifyIDToken parses Google's id_token, verifies its RS256 signature against
// Google's JWKS, checks iss/aud/exp, and returns the claims as an Identity.
func (g *google) verifyIDToken(jwt string) (Identity, error) {
	parts := strings.Split(jwt, ".")
	if len(parts) != 3 {
		return Identity{}, errors.New("malformed jwt")
	}
	headerJSON, err := base64.RawURLEncoding.DecodeString(parts[0])
	if err != nil {
		return Identity{}, fmt.Errorf("decode header: %w", err)
	}
	var header struct {
		Alg string `json:"alg"`
		Kid string `json:"kid"`
	}
	if err := json.Unmarshal(headerJSON, &header); err != nil {
		return Identity{}, fmt.Errorf("parse header: %w", err)
	}
	if header.Alg != "RS256" {
		return Identity{}, fmt.Errorf("unsupported alg %q", header.Alg)
	}
	payloadJSON, err := base64.RawURLEncoding.DecodeString(parts[1])
	if err != nil {
		return Identity{}, fmt.Errorf("decode payload: %w", err)
	}
	sig, err := base64.RawURLEncoding.DecodeString(parts[2])
	if err != nil {
		return Identity{}, fmt.Errorf("decode signature: %w", err)
	}
	key, err := g.fetchKey(header.Kid)
	if err != nil {
		return Identity{}, err
	}
	if err := verifyRS256(key, []byte(parts[0]+"."+parts[1]), sig); err != nil {
		return Identity{}, fmt.Errorf("signature: %w", err)
	}

	var claims idTokenClaims
	if err := json.Unmarshal(payloadJSON, &claims); err != nil {
		return Identity{}, fmt.Errorf("parse claims: %w", err)
	}
	if claims.Iss != g.issuer && claims.Iss != "accounts.google.com" {
		return Identity{}, fmt.Errorf("issuer %q not Google", claims.Iss)
	}
	if !audienceMatches(claims.Aud, g.clientID) {
		return Identity{}, errors.New("audience mismatch")
	}
	if claims.Exp <= time.Now().Unix() {
		return Identity{}, errors.New("id token expired")
	}

	verified := false
	switch v := claims.EmailVerified.(type) {
	case bool:
		verified = v
	case string:
		verified = v == "true"
	}
	return Identity{
		Sub:           claims.Sub,
		Email:         claims.Email,
		HostedDomain:  claims.HostedDomain,
		EmailVerified: verified,
	}, nil
}

// audienceMatches reports whether the id_token's aud claim contains clientID,
// accepting both the string and array encodings Google may send.
func audienceMatches(aud any, clientID string) bool {
	switch v := aud.(type) {
	case string:
		return v == clientID
	case []any:
		for _, e := range v {
			if s, ok := e.(string); ok && s == clientID {
				return true
			}
		}
	}
	return false
}

// fetchKey returns the RSA public key for the given JWKS key id. It loads
// Google's key set once, then refreshes a single time on a miss to pick up a
// rotated signing key before giving up.
func (g *google) fetchKey(kid string) (*rsa.PublicKey, error) {
	g.jwksOnce.Do(func() {
		g.jwksCache, g.jwksErr = g.loadJWKS()
	})
	if g.jwksErr != nil {
		return nil, g.jwksErr
	}
	k, ok := g.jwksCache[kid]
	if !ok {
		// Refresh once on miss — Google rotates signing keys.
		fresh, err := g.loadJWKS()
		if err != nil {
			return nil, err
		}
		g.jwksCache = fresh
		k, ok = fresh[kid]
		if !ok {
			return nil, fmt.Errorf("unknown signing kid %q", kid)
		}
	}
	return k, nil
}

// loadJWKS fetches Google's JWK set and builds an RSA public key per kid,
// decoding each key's base64url modulus and exponent.
func (g *google) loadJWKS() (map[string]*rsa.PublicKey, error) {
	req, err := http.NewRequestWithContext(context.Background(), http.MethodGet, g.jwksEndpoint, nil)
	if err != nil {
		return nil, err
	}
	resp, err := g.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("jwks fetch: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("jwks fetch: %d", resp.StatusCode)
	}
	var doc struct {
		Keys []struct {
			Kty string `json:"kty"`
			Kid string `json:"kid"`
			N   string `json:"n"`
			E   string `json:"e"`
		} `json:"keys"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&doc); err != nil {
		return nil, fmt.Errorf("jwks decode: %w", err)
	}
	out := map[string]*rsa.PublicKey{}
	for _, k := range doc.Keys {
		if k.Kty != "RSA" {
			continue
		}
		nBytes, err := base64.RawURLEncoding.DecodeString(k.N)
		if err != nil {
			continue
		}
		eBytes, err := base64.RawURLEncoding.DecodeString(k.E)
		if err != nil {
			continue
		}
		e := 0
		for _, b := range eBytes {
			e = e<<8 | int(b)
		}
		out[k.Kid] = &rsa.PublicKey{N: new(big.Int).SetBytes(nBytes), E: e}
	}
	return out, nil
}

// verifyRS256 verifies an RS256 (RSASSA-PKCS1-v1_5 over SHA-256) signature:
// sig must be pub's signature over signed.
func verifyRS256(pub *rsa.PublicKey, signed, sig []byte) error {
	sum := sha256.Sum256(signed)
	return rsa.VerifyPKCS1v15(pub, crypto.SHA256, sum[:], sig)
}

// truncate caps s at n bytes, appending an ellipsis when it overflows, so an
// upstream error body can be logged without dumping an unbounded response.
func truncate(s string, n int) string {
	if len(s) <= n {
		return s
	}
	return s[:n] + "…"
}
