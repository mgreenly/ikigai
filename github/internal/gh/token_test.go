package gh

import (
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
	"io"
	"net/http"
	"strings"
	"sync"
	"testing"
	"time"
)

func TestAppJWTClaimsAndSignatureVerify(t *testing.T) {
	// R-DLMX-CNDL
	key := mustRSAKey(t)
	now := time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC)
	src := &tokenSource{appID: "12345", signer: key}

	jwt, err := src.appJWT(now)
	if err != nil {
		t.Fatalf("appJWT() error = %v", err)
	}
	parts := strings.Split(jwt, ".")
	if len(parts) != 3 {
		t.Fatalf("JWT has %d parts, want 3", len(parts))
	}

	var header struct {
		Alg string `json:"alg"`
		Typ string `json:"typ"`
	}
	decodeJWTPart(t, parts[0], &header)
	if header.Alg != "RS256" || header.Typ != "JWT" {
		t.Fatalf("header = %+v, want RS256 JWT", header)
	}

	var claims struct {
		Iss string `json:"iss"`
		Iat int64  `json:"iat"`
		Exp int64  `json:"exp"`
	}
	decodeJWTPart(t, parts[1], &claims)
	if claims.Iss != "12345" {
		t.Fatalf("iss = %q, want app id", claims.Iss)
	}
	if !time.Unix(claims.Iat, 0).Before(now) {
		t.Fatalf("iat = %v, want before %v", time.Unix(claims.Iat, 0), now)
	}
	if claims.Exp-claims.Iat > int64((10 * time.Minute).Seconds()) {
		t.Fatalf("exp-iat = %ds, want <= 600s", claims.Exp-claims.Iat)
	}

	unsigned := parts[0] + "." + parts[1]
	sig, err := base64.RawURLEncoding.DecodeString(parts[2])
	if err != nil {
		t.Fatalf("signature base64 decode: %v", err)
	}
	sum := sha256.Sum256([]byte(unsigned))
	if err := rsa.VerifyPKCS1v15(&key.PublicKey, crypto.SHA256, sum[:], sig); err != nil {
		t.Fatalf("signature did not verify against derived public key: %v", err)
	}

	var tamperedClaims map[string]any
	decodeJWTPart(t, parts[1], &tamperedClaims)
	tamperedClaims["iss"] = "other-app"
	tamperedPayload, err := json.Marshal(tamperedClaims)
	if err != nil {
		t.Fatalf("marshal tampered claims: %v", err)
	}
	tamperedUnsigned := parts[0] + "." + base64.RawURLEncoding.EncodeToString(tamperedPayload)
	tamperedSum := sha256.Sum256([]byte(tamperedUnsigned))
	if err := rsa.VerifyPKCS1v15(&key.PublicKey, crypto.SHA256, tamperedSum[:], sig); err == nil {
		t.Fatal("tampered claims verified, want signature failure")
	}
}

func TestRESTBearerUsesInstallationToken(t *testing.T) {
	// R-DO2Q-46UZ
	key := mustRSAKey(t)
	now := time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC)
	var restAuth string
	client := stubClient(func(req *http.Request) (*http.Response, error) {
		switch req.URL.Path {
		case "/orgs/acme/installation":
			return jsonResponse(http.StatusOK, `{"id":42}`), nil
		case "/app/installations/42/access_tokens":
			return jsonResponse(http.StatusCreated, `{"token":"installation-token","expires_at":"2026-07-04T12:10:00Z"}`), nil
		case "/repos/acme/widgets":
			restAuth = req.Header.Get("Authorization")
			return jsonResponse(http.StatusOK, `{"ok":true}`), nil
		default:
			t.Fatalf("unexpected path %s", req.URL.Path)
			return nil, nil
		}
	})
	withAPIBase(t, "https://stub.github.test")
	src := &tokenSource{appID: "12345", org: "acme", signer: key, httpClient: client, now: func() time.Time { return now }}

	req, err := http.NewRequest(http.MethodGet, apiBase+"/repos/acme/widgets", nil)
	if err != nil {
		t.Fatalf("NewRequest: %v", err)
	}
	resp, err := src.do(context.Background(), req)
	if err != nil {
		t.Fatalf("do() error = %v", err)
	}
	closeBody(resp)

	if restAuth != "Bearer installation-token" {
		t.Fatalf("REST Authorization = %q, want installation token bearer", restAuth)
	}
}

func TestTokenCacheHonorsReturnedExpiryAndSlack(t *testing.T) {
	// R-DPAM-HYLO
	key := mustRSAKey(t)
	now := time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC)
	var mintCalls int
	client := stubClient(func(req *http.Request) (*http.Response, error) {
		switch req.URL.Path {
		case "/orgs/acme/installation":
			return jsonResponse(http.StatusOK, `{"id":42}`), nil
		case "/app/installations/42/access_tokens":
			mintCalls++
			if mintCalls == 1 {
				return jsonResponse(http.StatusCreated, `{"token":"first","expires_at":"2026-07-04T12:05:00Z"}`), nil
			}
			return jsonResponse(http.StatusCreated, `{"token":"second","expires_at":"2026-07-04T12:30:00Z"}`), nil
		default:
			t.Fatalf("unexpected path %s", req.URL.Path)
			return nil, nil
		}
	})
	withAPIBase(t, "https://stub.github.test")
	src := &tokenSource{appID: "12345", org: "acme", signer: key, httpClient: client, now: func() time.Time { return now }}

	got, err := src.token(context.Background(), false)
	if err != nil {
		t.Fatalf("first token() error = %v", err)
	}
	if got != "first" || mintCalls != 1 {
		t.Fatalf("first token = %q, mintCalls = %d; want first and 1", got, mintCalls)
	}

	now = time.Date(2026, 7, 4, 12, 3, 59, 0, time.UTC)
	got, err = src.token(context.Background(), false)
	if err != nil {
		t.Fatalf("cached token() error = %v", err)
	}
	if got != "first" || mintCalls != 1 {
		t.Fatalf("cached token = %q, mintCalls = %d; want first and 1", got, mintCalls)
	}

	now = time.Date(2026, 7, 4, 12, 4, 1, 0, time.UTC)
	got, err = src.token(context.Background(), false)
	if err != nil {
		t.Fatalf("refresh token() error = %v", err)
	}
	if got != "second" || mintCalls != 2 {
		t.Fatalf("refreshed token = %q, mintCalls = %d; want second and 2", got, mintCalls)
	}
}

func TestClientTokenSharesHeaderCacheExpiryAndAppAuthFailureR_GSI7_P8NI(t *testing.T) {
	// R-GSI7-P8NI
	key := mustRSAKey(t)
	now := time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC)
	firstExpiry := now.Add(5 * time.Minute)
	secondExpiry := now.Add(30 * time.Minute)
	var mintCalls int
	var bearerTokens []string
	client := stubClient(func(req *http.Request) (*http.Response, error) {
		switch req.URL.Path {
		case "/orgs/acme/installation":
			return jsonResponse(http.StatusOK, `{"id":42}`), nil
		case "/app/installations/42/access_tokens":
			mintCalls++
			if mintCalls == 1 {
				return jsonResponse(http.StatusCreated, `{"token":"first-token","expires_at":"2026-07-04T12:05:00Z"}`), nil
			}
			return jsonResponse(http.StatusCreated, `{"token":"second-token","expires_at":"2026-07-04T12:30:00Z"}`), nil
		case "/repos/acme/widgets":
			bearerTokens = append(bearerTokens, req.Header.Get("Authorization"))
			return jsonResponse(http.StatusOK, `{}`), nil
		default:
			t.Fatalf("unexpected path %s", req.URL.Path)
			return nil, nil
		}
	})
	withAPIBase(t, "https://stub.github.test")
	c := &Client{org: "acme", http: client, ts: &tokenSource{
		appID: "12345", org: "acme", signer: key, httpClient: client,
		now: func() time.Time { return now },
	}}

	token, expiresAt, err := c.Token(context.Background())
	if err != nil {
		t.Fatalf("Token() error = %v", err)
	}
	if token != "first-token" || !expiresAt.Equal(firstExpiry) {
		t.Fatalf("Token() = %q, %v; want first token expiring %v", token, expiresAt, firstExpiry)
	}

	now = firstExpiry.Add(-tokenSlack - time.Second)
	req, err := http.NewRequest(http.MethodGet, apiBase+"/repos/acme/widgets", nil)
	if err != nil {
		t.Fatalf("NewRequest: %v", err)
	}
	resp, err := c.ts.do(context.Background(), req)
	if err != nil {
		t.Fatalf("header path error = %v", err)
	}
	closeBody(resp)
	if len(bearerTokens) != 1 || bearerTokens[0] != "Bearer first-token" || mintCalls != 1 {
		t.Fatalf("header tokens = %v, mint calls = %d; want shared cached first token", bearerTokens, mintCalls)
	}

	now = firstExpiry.Add(-tokenSlack)
	token, expiresAt, err = c.Token(context.Background())
	if err != nil {
		t.Fatalf("refresh Token() error = %v", err)
	}
	if token != "second-token" || !expiresAt.Equal(secondExpiry) || mintCalls != 2 {
		t.Fatalf("refreshed Token() = %q, %v with %d mints; want second token, %v, 2 mints", token, expiresAt, mintCalls, secondExpiry)
	}

	t.Run("mint failure", func(t *testing.T) {
		failingHTTP := stubClient(func(req *http.Request) (*http.Response, error) {
			switch req.URL.Path {
			case "/orgs/acme/installation":
				return jsonResponse(http.StatusOK, `{"id":42}`), nil
			case "/app/installations/42/access_tokens":
				return jsonResponse(http.StatusInternalServerError, `{"message":"mint unavailable"}`), nil
			default:
				t.Fatalf("unexpected path %s", req.URL.Path)
				return nil, nil
			}
		})
		failing := &Client{ts: &tokenSource{
			appID: "12345", org: "acme", signer: key, httpClient: failingHTTP,
			now: func() time.Time { return now },
		}}
		if _, _, err := failing.Token(context.Background()); !errors.Is(err, ErrAppAuth) {
			t.Fatalf("Token() error = %v, want ErrAppAuth", err)
		}
	})
}

func TestRESTUnauthorizedForcesOneRefreshAndRetry(t *testing.T) {
	// R-DQII-VQCD
	t.Run("retry wins", func(t *testing.T) {
		key := mustRSAKey(t)
		now := time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC)
		var mintCalls int
		var restCalls int
		client := stubClient(func(req *http.Request) (*http.Response, error) {
			switch req.URL.Path {
			case "/orgs/acme/installation":
				return jsonResponse(http.StatusOK, `{"id":42}`), nil
			case "/app/installations/42/access_tokens":
				mintCalls++
				token := "first"
				if mintCalls == 2 {
					token = "second"
				}
				return jsonResponse(http.StatusCreated, `{"token":"`+token+`","expires_at":"2026-07-04T12:10:00Z"}`), nil
			case "/repos/acme/widgets":
				restCalls++
				if restCalls == 1 {
					return jsonResponse(http.StatusUnauthorized, `{"message":"bad credentials"}`), nil
				}
				if got := req.Header.Get("Authorization"); got != "Bearer second" {
					t.Fatalf("retry Authorization = %q, want refreshed token", got)
				}
				return jsonResponse(http.StatusOK, `{"ok":true}`), nil
			default:
				t.Fatalf("unexpected path %s", req.URL.Path)
				return nil, nil
			}
		})
		withAPIBase(t, "https://stub.github.test")
		src := &tokenSource{appID: "12345", org: "acme", signer: key, httpClient: client, now: func() time.Time { return now }}

		req, err := http.NewRequest(http.MethodGet, apiBase+"/repos/acme/widgets", nil)
		if err != nil {
			t.Fatalf("NewRequest: %v", err)
		}
		resp, err := src.do(context.Background(), req)
		if err != nil {
			t.Fatalf("do() error = %v", err)
		}
		closeBody(resp)
		if mintCalls != 2 || restCalls != 2 {
			t.Fatalf("mintCalls = %d, restCalls = %d; want 2 and 2", mintCalls, restCalls)
		}
	})

	t.Run("second unauthorized returns error", func(t *testing.T) {
		key := mustRSAKey(t)
		now := time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC)
		var mintCalls int
		var restCalls int
		client := stubClient(func(req *http.Request) (*http.Response, error) {
			switch req.URL.Path {
			case "/orgs/acme/installation":
				return jsonResponse(http.StatusOK, `{"id":42}`), nil
			case "/app/installations/42/access_tokens":
				mintCalls++
				return jsonResponse(http.StatusCreated, `{"token":"token","expires_at":"2026-07-04T12:10:00Z"}`), nil
			case "/repos/acme/widgets":
				restCalls++
				return jsonResponse(http.StatusUnauthorized, `{"message":"bad credentials"}`), nil
			default:
				t.Fatalf("unexpected path %s", req.URL.Path)
				return nil, nil
			}
		})
		withAPIBase(t, "https://stub.github.test")
		src := &tokenSource{appID: "12345", org: "acme", signer: key, httpClient: client, now: func() time.Time { return now }}

		req, err := http.NewRequest(http.MethodGet, apiBase+"/repos/acme/widgets", nil)
		if err != nil {
			t.Fatalf("NewRequest: %v", err)
		}
		resp, err := src.do(context.Background(), req)
		if resp != nil {
			closeBody(resp)
		}
		if err == nil {
			t.Fatal("do() error = nil, want bounded unauthorized error")
		}
		if mintCalls != 2 || restCalls != 2 {
			t.Fatalf("mintCalls = %d, restCalls = %d; want 2 and 2", mintCalls, restCalls)
		}
	})
}

func TestAppAuthFailuresWrapSentinelDoNotRetryOrLeakKey(t *testing.T) {
	// R-DRQF-9I32
	tests := []struct {
		name          string
		statusPath    string
		statusCode    int
		wantInstCalls int
		wantMintCalls int
	}{
		{
			name:          "resolve unauthorized",
			statusPath:    "/orgs/acme/installation",
			statusCode:    http.StatusUnauthorized,
			wantInstCalls: 1,
		},
		{
			name:          "mint not found",
			statusPath:    "/app/installations/42/access_tokens",
			statusCode:    http.StatusNotFound,
			wantInstCalls: 1,
			wantMintCalls: 1,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			key := mustRSAKey(t)
			keyPEM := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(key)})
			var instCalls int
			var mintCalls int
			client := stubClient(func(req *http.Request) (*http.Response, error) {
				switch req.URL.Path {
				case "/orgs/acme/installation":
					instCalls++
					if tt.statusPath == req.URL.Path {
						return jsonResponse(tt.statusCode, `{"message":"bad app auth"}`), nil
					}
					return jsonResponse(http.StatusOK, `{"id":42}`), nil
				case "/app/installations/42/access_tokens":
					mintCalls++
					if tt.statusPath == req.URL.Path {
						return jsonResponse(tt.statusCode, `{"message":"bad app auth"}`), nil
					}
					return jsonResponse(http.StatusCreated, `{"token":"token","expires_at":"2026-07-04T12:10:00Z"}`), nil
				default:
					t.Fatalf("unexpected path %s", req.URL.Path)
					return nil, nil
				}
			})
			withAPIBase(t, "https://stub.github.test")
			src := &tokenSource{
				appID:      "12345",
				org:        "acme",
				signer:     key,
				httpClient: client,
				now:        func() time.Time { return time.Date(2026, 7, 4, 12, 0, 0, 0, time.UTC) },
			}

			_, err := src.token(context.Background(), false)
			if !errors.Is(err, ErrAppAuth) {
				t.Fatalf("token() error = %v, want ErrAppAuth", err)
			}
			if !strings.Contains(err.Error(), http.StatusText(tt.statusCode)) {
				t.Fatalf("token() error = %q, want status text", err)
			}
			if strings.Contains(err.Error(), string(keyPEM)) || strings.Contains(err.Error(), "RSA PRIVATE KEY") {
				t.Fatalf("token() error leaked private key material: %q", err)
			}
			if instCalls != tt.wantInstCalls || mintCalls != tt.wantMintCalls {
				t.Fatalf("instCalls = %d, mintCalls = %d; want %d and %d", instCalls, mintCalls, tt.wantInstCalls, tt.wantMintCalls)
			}
		})
	}
}

func decodeJWTPart(t *testing.T, encoded string, out any) {
	t.Helper()
	data, err := base64.RawURLEncoding.DecodeString(encoded)
	if err != nil {
		t.Fatalf("decode JWT part: %v", err)
	}
	if err := json.Unmarshal(data, out); err != nil {
		t.Fatalf("unmarshal JWT part: %v", err)
	}
}

func mustRSAKey(t *testing.T) *rsa.PrivateKey {
	t.Helper()
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("GenerateKey: %v", err)
	}
	return key
}

type roundTripFunc func(*http.Request) (*http.Response, error)

func (f roundTripFunc) RoundTrip(req *http.Request) (*http.Response, error) {
	return f(req)
}

func stubClient(fn func(*http.Request) (*http.Response, error)) *http.Client {
	var mu sync.Mutex
	return &http.Client{Transport: roundTripFunc(func(req *http.Request) (*http.Response, error) {
		mu.Lock()
		defer mu.Unlock()
		return fn(req)
	})}
}

func jsonResponse(status int, body string) *http.Response {
	return &http.Response{
		StatusCode: status,
		Status:     http.StatusText(status),
		Header:     make(http.Header),
		Body:       io.NopCloser(strings.NewReader(body)),
	}
}

func withAPIBase(t *testing.T, base string) {
	t.Helper()
	old := apiBase
	apiBase = base
	t.Cleanup(func() { apiBase = old })
}
