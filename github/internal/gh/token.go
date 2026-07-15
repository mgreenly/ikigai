// Package gh contains the small GitHub REST client pieces used by the service.
package gh

import (
	"bytes"
	"context"
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"
)

var (
	// ErrAppAuth marks GitHub App authentication failures that require operator action.
	ErrAppAuth = errors.New("github: app authentication failed")

	// apiBase is a var so tests can redirect GitHub API calls to an offline stub.
	apiBase = "https://api.github.com"
)

const tokenSlack = 60 * time.Second

type tokenSource struct {
	appID      string
	org        string
	signer     *rsa.PrivateKey
	httpClient *http.Client

	mu     sync.Mutex
	instID string
	cached string
	expiry time.Time

	now func() time.Time
}

// token returns a valid installation token, minting/refreshing as needed.
// force=true discards the cache and re-mints unconditionally.
func (t *tokenSource) token(ctx context.Context, force bool) (string, error) {
	token, _, err := t.tokenWithExpiry(ctx, force)
	return token, err
}

// tokenWithExpiry returns the installation token and the expiry GitHub
// supplied for it. It is the single cache and minting path used by both REST
// requests and callers that need the token for git transport.
func (t *tokenSource) tokenWithExpiry(ctx context.Context, force bool) (string, time.Time, error) {
	t.mu.Lock()
	defer t.mu.Unlock()

	now := t.currentTime()
	if !force && t.cached != "" && now.Add(tokenSlack).Before(t.expiry) {
		return t.cached, t.expiry, nil
	}

	if t.instID == "" {
		instID, err := t.resolveInstallationLocked(ctx, now)
		if err != nil {
			return "", time.Time{}, err
		}
		t.instID = instID
	}

	token, expiry, err := t.mintTokenLocked(ctx, now)
	if err != nil {
		return "", time.Time{}, err
	}
	t.cached = token
	t.expiry = expiry
	return token, expiry, nil
}

func (t *tokenSource) do(ctx context.Context, req *http.Request) (*http.Response, error) {
	resp, err := t.doOnce(ctx, req, false)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != http.StatusUnauthorized {
		return resp, nil
	}
	closeBody(resp)

	resp, err = t.doOnce(ctx, req, true)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode == http.StatusUnauthorized {
		closeBody(resp)
		return nil, fmt.Errorf("github: unauthorized after token refresh: %s", resp.Status)
	}
	return resp, nil
}

func (t *tokenSource) doOnce(ctx context.Context, req *http.Request, force bool) (*http.Response, error) {
	token, err := t.token(ctx, force)
	if err != nil {
		return nil, err
	}

	next := req.Clone(ctx)
	if req.Body != nil && req.GetBody != nil {
		body, err := req.GetBody()
		if err != nil {
			return nil, err
		}
		next.Body = body
	}
	next.Header = req.Header.Clone()
	next.Header.Set("Authorization", "Bearer "+token)
	return t.client().Do(next)
}

func (t *tokenSource) resolveInstallationLocked(ctx context.Context, now time.Time) (string, error) {
	jwt, err := t.appJWT(now)
	if err != nil {
		return "", err
	}

	reqURL, err := url.JoinPath(apiBase, "orgs", t.org, "installation")
	if err != nil {
		return "", fmt.Errorf("%w: installation URL: %v", ErrAppAuth, err)
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, reqURL, nil)
	if err != nil {
		return "", fmt.Errorf("%w: installation request: %v", ErrAppAuth, err)
	}
	req.Header.Set("Authorization", "Bearer "+jwt)
	req.Header.Set("Accept", "application/vnd.github+json")

	resp, err := t.client().Do(req)
	if err != nil {
		return "", fmt.Errorf("%w: resolve installation: %v", ErrAppAuth, err)
	}
	defer closeBody(resp)

	if resp.StatusCode == http.StatusUnauthorized || resp.StatusCode == http.StatusNotFound {
		return "", appAuthStatusError(resp)
	}
	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		return "", fmt.Errorf("github: resolve installation failed: %s", resp.Status)
	}

	var out struct {
		ID json.Number `json:"id"`
	}
	dec := json.NewDecoder(resp.Body)
	dec.UseNumber()
	if err := dec.Decode(&out); err != nil {
		return "", fmt.Errorf("%w: decode installation: %v", ErrAppAuth, err)
	}
	if out.ID.String() == "" {
		return "", fmt.Errorf("%w: installation response missing id", ErrAppAuth)
	}
	return out.ID.String(), nil
}

func (t *tokenSource) mintTokenLocked(ctx context.Context, now time.Time) (string, time.Time, error) {
	jwt, err := t.appJWT(now)
	if err != nil {
		return "", time.Time{}, err
	}

	reqURL, err := url.JoinPath(apiBase, "app", "installations", t.instID, "access_tokens")
	if err != nil {
		return "", time.Time{}, fmt.Errorf("%w: token URL: %v", ErrAppAuth, err)
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, reqURL, nil)
	if err != nil {
		return "", time.Time{}, fmt.Errorf("%w: token request: %v", ErrAppAuth, err)
	}
	req.Header.Set("Authorization", "Bearer "+jwt)
	req.Header.Set("Accept", "application/vnd.github+json")

	resp, err := t.client().Do(req)
	if err != nil {
		return "", time.Time{}, fmt.Errorf("%w: mint token: %v", ErrAppAuth, err)
	}
	defer closeBody(resp)

	if resp.StatusCode == http.StatusUnauthorized || resp.StatusCode == http.StatusNotFound {
		return "", time.Time{}, appAuthStatusError(resp)
	}
	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		return "", time.Time{}, fmt.Errorf("%w: mint token failed: %s", ErrAppAuth, resp.Status)
	}

	var out struct {
		Token     string    `json:"token"`
		ExpiresAt time.Time `json:"expires_at"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
		return "", time.Time{}, fmt.Errorf("%w: decode token: %v", ErrAppAuth, err)
	}
	if out.Token == "" || out.ExpiresAt.IsZero() {
		return "", time.Time{}, fmt.Errorf("%w: token response missing token or expiry", ErrAppAuth)
	}
	return out.Token, out.ExpiresAt, nil
}

func (t *tokenSource) appJWT(now time.Time) (string, error) {
	if t.signer == nil {
		return "", fmt.Errorf("%w: missing private key", ErrAppAuth)
	}
	if t.appID == "" {
		return "", fmt.Errorf("%w: missing app id", ErrAppAuth)
	}

	iat := now.Add(-tokenSlack).Unix()
	header, err := json.Marshal(struct {
		Alg string `json:"alg"`
		Typ string `json:"typ"`
	}{
		Alg: "RS256",
		Typ: "JWT",
	})
	if err != nil {
		return "", fmt.Errorf("%w: encode JWT header: %v", ErrAppAuth, err)
	}
	claims, err := json.Marshal(struct {
		Iss string `json:"iss"`
		Iat int64  `json:"iat"`
		Exp int64  `json:"exp"`
	}{
		Iss: t.appID,
		Iat: iat,
		Exp: iat + int64((10 * time.Minute).Seconds()),
	})
	if err != nil {
		return "", fmt.Errorf("%w: encode JWT claims: %v", ErrAppAuth, err)
	}

	unsigned := base64.RawURLEncoding.EncodeToString(header) + "." + base64.RawURLEncoding.EncodeToString(claims)
	sum := sha256.Sum256([]byte(unsigned))
	sig, err := rsa.SignPKCS1v15(rand.Reader, t.signer, crypto.SHA256, sum[:])
	if err != nil {
		return "", fmt.Errorf("%w: sign JWT: %v", ErrAppAuth, err)
	}
	return unsigned + "." + base64.RawURLEncoding.EncodeToString(sig), nil
}

func (t *tokenSource) currentTime() time.Time {
	if t.now != nil {
		return t.now()
	}
	return time.Now()
}

func (t *tokenSource) client() *http.Client {
	if t.httpClient != nil {
		return t.httpClient
	}
	return http.DefaultClient
}

func parseAppPrivateKey(pemText string) (*rsa.PrivateKey, error) {
	block, _ := pem.Decode([]byte(pemText))
	if block == nil {
		return nil, fmt.Errorf("%w: private key PEM not found", ErrAppAuth)
	}
	if key, err := x509.ParsePKCS1PrivateKey(block.Bytes); err == nil {
		return key, nil
	}
	parsed, err := x509.ParsePKCS8PrivateKey(block.Bytes)
	if err != nil {
		return nil, fmt.Errorf("%w: parse private key", ErrAppAuth)
	}
	key, ok := parsed.(*rsa.PrivateKey)
	if !ok {
		return nil, fmt.Errorf("%w: private key is not RSA", ErrAppAuth)
	}
	return key, nil
}

func privateKeyFromEnv() (*rsa.PrivateKey, error) {
	return parseAppPrivateKey(os.Getenv("IKIGENBA_APP_PRIVATE_KEY"))
}

func appAuthStatusError(resp *http.Response) error {
	message := readErrorMessage(resp.Body)
	if message == "" {
		return fmt.Errorf("%w: %s", ErrAppAuth, resp.Status)
	}
	return fmt.Errorf("%w: %s: %s", ErrAppAuth, resp.Status, message)
}

func readErrorMessage(body io.Reader) string {
	data, err := io.ReadAll(io.LimitReader(body, 4096))
	if err != nil {
		return ""
	}
	data = bytes.TrimSpace(data)
	if len(data) == 0 {
		return ""
	}

	var out struct {
		Message string `json:"message"`
	}
	if err := json.Unmarshal(data, &out); err == nil && out.Message != "" {
		return out.Message
	}
	return strings.TrimSpace(string(data))
}

func closeBody(resp *http.Response) {
	if resp != nil && resp.Body != nil {
		_ = resp.Body.Close()
	}
}
