package googleidp

import (
	"context"
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"math/big"
	"net/http"
	"net/http/httptest"
	"net/url"
	"testing"
	"time"
)

func TestAuthorizeURL(t *testing.T) {
	p := New(Credentials{ClientID: "client-123", WorkspaceDomain: "example.com"})
	raw := p.AuthorizeURL("STATE-XYZ", "https://int.ikigenba.com/oauth/google/callback")

	u, err := url.Parse(raw)
	if err != nil {
		t.Fatalf("AuthorizeURL produced unparseable URL %q: %v", raw, err)
	}
	if got := u.Scheme + "://" + u.Host + u.Path; got != "https://accounts.google.com/o/oauth2/v2/auth" {
		t.Errorf("endpoint = %q, want Google's authorize endpoint", got)
	}

	q := u.Query()
	want := map[string]string{
		"client_id":     "client-123",
		"hd":            "example.com",
		"state":         "STATE-XYZ",
		"redirect_uri":  "https://int.ikigenba.com/oauth/google/callback",
		"response_type": "code",
		"scope":         "openid email profile",
		"access_type":   "online",
		"prompt":        "login", // web sign-in forces fresh credentials
	}
	for k, v := range want {
		if got := q.Get(k); got != v {
			t.Errorf("query %s = %q, want %q", k, got, v)
		}
	}
}

// mintToken builds a JWT with the given header alg/kid and claims, signed with
// signKey using RS256. signKey can differ from the key seeded in the provider's
// JWKS cache, which is how the bad-signature case is exercised.
func mintToken(t *testing.T, signKey *rsa.PrivateKey, alg, kid string, claims map[string]any) string {
	t.Helper()
	header, err := json.Marshal(map[string]string{"alg": alg, "kid": kid})
	if err != nil {
		t.Fatalf("marshal header: %v", err)
	}
	body, err := json.Marshal(claims)
	if err != nil {
		t.Fatalf("marshal claims: %v", err)
	}
	signingInput := base64.RawURLEncoding.EncodeToString(header) + "." + base64.RawURLEncoding.EncodeToString(body)
	sum := sha256.Sum256([]byte(signingInput))
	sig, err := rsa.SignPKCS1v15(rand.Reader, signKey, crypto.SHA256, sum[:])
	if err != nil {
		t.Fatalf("sign: %v", err)
	}
	return signingInput + "." + base64.RawURLEncoding.EncodeToString(sig)
}

// googleWithKey returns a live provider whose JWKS cache holds pub under kid.
// Seeding through jwksOnce consumes the Once so fetchKey never hits the network.
func googleWithKey(kid string, pub *rsa.PublicKey, clientID string) *google {
	g := &google{clientID: clientID, issuer: "https://accounts.google.com"}
	g.jwksOnce.Do(func() {
		g.jwksCache = map[string]*rsa.PublicKey{kid: pub}
	})
	return g
}

func TestVerifyIDToken(t *testing.T) {
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}
	wrongKey, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("generate wrong key: %v", err)
	}

	const kid = "test-kid"
	const clientID = "client-123"
	g := googleWithKey(kid, &key.PublicKey, clientID)

	future := time.Now().Add(time.Hour).Unix()
	past := time.Now().Add(-time.Hour).Unix()

	// base returns a fresh, fully-valid claim set each call so cases can mutate
	// a single field without affecting the others.
	base := func() map[string]any {
		return map[string]any{
			"iss":            "https://accounts.google.com",
			"aud":            clientID,
			"exp":            future,
			"sub":            "user-sub-123",
			"email":          "alice@example.com",
			"hd":             "example.com",
			"email_verified": true,
		}
	}
	withClaim := func(m map[string]any, k string, v any) map[string]any {
		m[k] = v
		return m
	}

	t.Run("valid token yields identity", func(t *testing.T) {
		id, err := g.verifyIDToken(mintToken(t, key, "RS256", kid, base()))
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		want := Identity{Sub: "user-sub-123", Email: "alice@example.com", HostedDomain: "example.com", EmailVerified: true}
		if id != want {
			t.Errorf("identity = %+v, want %+v", id, want)
		}
	})

	t.Run("legacy bare issuer accepted", func(t *testing.T) {
		if _, err := g.verifyIDToken(mintToken(t, key, "RS256", kid, withClaim(base(), "iss", "accounts.google.com"))); err != nil {
			t.Errorf("bare issuer rejected: %v", err)
		}
	})

	t.Run("audience as array accepted", func(t *testing.T) {
		if _, err := g.verifyIDToken(mintToken(t, key, "RS256", kid, withClaim(base(), "aud", []any{"other", clientID}))); err != nil {
			t.Errorf("array audience rejected: %v", err)
		}
	})

	t.Run("email_verified string true coerced", func(t *testing.T) {
		id, err := g.verifyIDToken(mintToken(t, key, "RS256", kid, withClaim(base(), "email_verified", "true")))
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if !id.EmailVerified {
			t.Error("email_verified \"true\" did not coerce to true")
		}
	})

	t.Run("unverified email returned not rejected", func(t *testing.T) {
		// Policy is the caller's: verifyIDToken reports email_verified, it does
		// not reject on it.
		id, err := g.verifyIDToken(mintToken(t, key, "RS256", kid, withClaim(base(), "email_verified", false)))
		if err != nil {
			t.Fatalf("unverified email should not error here: %v", err)
		}
		if id.EmailVerified {
			t.Error("EmailVerified = true, want false")
		}
	})

	t.Run("rejects", func(t *testing.T) {
		cases := []struct {
			name  string
			token string
		}{
			{"bad signature", mintToken(t, wrongKey, "RS256", kid, base())},
			{"alg not RS256", mintToken(t, key, "HS256", kid, base())},
			{"wrong issuer", mintToken(t, key, "RS256", kid, withClaim(base(), "iss", "https://evil.example"))},
			{"audience mismatch", mintToken(t, key, "RS256", kid, withClaim(base(), "aud", "other-client"))},
			{"expired", mintToken(t, key, "RS256", kid, withClaim(base(), "exp", past))},
			{"malformed", "only.two"},
		}
		for _, tc := range cases {
			t.Run(tc.name, func(t *testing.T) {
				if _, err := g.verifyIDToken(tc.token); err == nil {
					t.Error("expected error, got nil")
				}
			})
		}
	})
}

// googleAtEndpoint returns a provider whose token endpoint is tokenURL. When
// pub is non-nil it is seeded into the JWKS cache under kid so the full
// exchange-and-verify path runs without a network JWKS fetch.
func googleAtEndpoint(tokenURL, kid string, pub *rsa.PublicKey, clientID, clientSecret string) *google {
	g := &google{
		clientID:      clientID,
		clientSecret:  clientSecret,
		issuer:        "https://accounts.google.com",
		tokenEndpoint: tokenURL,
		httpClient:    &http.Client{Timeout: httpTimeout},
	}
	if pub != nil {
		g.jwksOnce.Do(func() { g.jwksCache = map[string]*rsa.PublicKey{kid: pub} })
	}
	return g
}

func TestExchangeCode(t *testing.T) {
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}
	const kid = "tk"
	const clientID = "client-123"
	const clientSecret = "secret-xyz"
	idToken := mintToken(t, key, "RS256", kid, map[string]any{
		"iss":            "https://accounts.google.com",
		"aud":            clientID,
		"exp":            time.Now().Add(time.Hour).Unix(),
		"sub":            "sub-1",
		"email":          "bob@example.com",
		"hd":             "example.com",
		"email_verified": true,
	})
	tokenJSON := `{"id_token":"` + idToken + `"}`

	t.Run("success returns identity and posts the right form", func(t *testing.T) {
		var gotForm url.Values
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			if err := r.ParseForm(); err != nil {
				t.Errorf("parse form: %v", err)
			}
			gotForm = r.PostForm
			w.Header().Set("Content-Type", "application/json")
			_, _ = w.Write([]byte(tokenJSON))
		}))
		defer srv.Close()

		g := googleAtEndpoint(srv.URL, kid, &key.PublicKey, clientID, clientSecret)
		id, err := g.ExchangeCode(context.Background(), "auth-code-1", "https://app/cb")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if id.Sub != "sub-1" || id.Email != "bob@example.com" {
			t.Errorf("identity = %+v", id)
		}
		wantForm := map[string]string{
			"grant_type":    "authorization_code",
			"code":          "auth-code-1",
			"redirect_uri":  "https://app/cb",
			"client_id":     clientID,
			"client_secret": clientSecret,
		}
		for k, v := range wantForm {
			if got := gotForm.Get(k); got != v {
				t.Errorf("form %s = %q, want %q", k, got, v)
			}
		}
	})

	t.Run("recovers after one 5xx", func(t *testing.T) {
		var calls int
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			calls++
			if calls == 1 {
				w.WriteHeader(http.StatusServiceUnavailable)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			_, _ = w.Write([]byte(tokenJSON))
		}))
		defer srv.Close()
		g := googleAtEndpoint(srv.URL, kid, &key.PublicKey, clientID, clientSecret)
		if _, err := g.ExchangeCode(context.Background(), "c", "https://app/cb"); err != nil {
			t.Fatalf("expected recovery, got %v", err)
		}
		if calls != 2 {
			t.Errorf("calls = %d, want 2", calls)
		}
	})

	t.Run("fails after two 5xx", func(t *testing.T) {
		var calls int
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			calls++
			w.WriteHeader(http.StatusBadGateway)
		}))
		defer srv.Close()
		g := googleAtEndpoint(srv.URL, "", nil, clientID, clientSecret)
		if _, err := g.ExchangeCode(context.Background(), "c", "cb"); err == nil {
			t.Error("expected error after retries")
		}
		if calls != 2 {
			t.Errorf("calls = %d, want 2 (exactly one retry)", calls)
		}
	})

	t.Run("non-200 errors", func(t *testing.T) {
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.WriteHeader(http.StatusBadRequest)
			_, _ = w.Write([]byte("invalid_grant"))
		}))
		defer srv.Close()
		g := googleAtEndpoint(srv.URL, "", nil, clientID, clientSecret)
		if _, err := g.ExchangeCode(context.Background(), "c", "cb"); err == nil {
			t.Error("expected error on 400")
		}
	})

	t.Run("missing id_token errors", func(t *testing.T) {
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			_, _ = w.Write([]byte(`{}`))
		}))
		defer srv.Close()
		g := googleAtEndpoint(srv.URL, "", nil, clientID, clientSecret)
		if _, err := g.ExchangeCode(context.Background(), "c", "cb"); err == nil {
			t.Error("expected error on missing id_token")
		}
	})
}

func TestAudienceMatches(t *testing.T) {
	const clientID = "client-1"
	cases := []struct {
		name string
		aud  any
		want bool
	}{
		{"string match", clientID, true},
		{"string mismatch", "other", false},
		{"array contains", []any{"x", clientID}, true},
		{"array missing", []any{"x", "y"}, false},
		{"empty array", []any{}, false},
		{"nil", nil, false},
		{"wrong type", 42, false},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := audienceMatches(tc.aud, clientID); got != tc.want {
				t.Errorf("audienceMatches(%v) = %v, want %v", tc.aud, got, tc.want)
			}
		})
	}
}

func TestLoadJWKS(t *testing.T) {
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}
	const kid = "kid-1"
	nB64 := base64.RawURLEncoding.EncodeToString(key.PublicKey.N.Bytes())
	eB64 := base64.RawURLEncoding.EncodeToString(big.NewInt(int64(key.PublicKey.E)).Bytes())

	t.Run("builds RSA keys and skips non-RSA", func(t *testing.T) {
		jwks := `{"keys":[` +
			`{"kty":"RSA","kid":"` + kid + `","n":"` + nB64 + `","e":"` + eB64 + `"},` +
			`{"kty":"EC","kid":"ec-skip","n":"x","e":"y"}` +
			`]}`
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "application/json")
			_, _ = w.Write([]byte(jwks))
		}))
		defer srv.Close()

		g := &google{jwksEndpoint: srv.URL, httpClient: &http.Client{Timeout: httpTimeout}}
		keys, err := g.loadJWKS()
		if err != nil {
			t.Fatalf("loadJWKS: %v", err)
		}
		got, ok := keys[kid]
		if !ok {
			t.Fatalf("kid %q absent from key set", kid)
		}
		if got.N.Cmp(key.PublicKey.N) != 0 || got.E != key.PublicKey.E {
			t.Errorf("decoded key mismatch: got E=%d", got.E)
		}
		if _, ok := keys["ec-skip"]; ok {
			t.Error("non-RSA key was not skipped")
		}
	})

	t.Run("non-200 errors", func(t *testing.T) {
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.WriteHeader(http.StatusInternalServerError)
		}))
		defer srv.Close()
		g := &google{jwksEndpoint: srv.URL, httpClient: &http.Client{Timeout: httpTimeout}}
		if _, err := g.loadJWKS(); err == nil {
			t.Error("expected error on non-200 JWKS response")
		}
	})
}

func TestStubAuthorizeURLEchoesState(t *testing.T) {
	raw := NewStub().AuthorizeURL("S1", "https://app/cb")
	u, err := url.Parse(raw)
	if err != nil {
		t.Fatalf("stub URL unparseable %q: %v", raw, err)
	}
	if got := u.Query().Get("state"); got != "S1" {
		t.Errorf("stub state = %q, want S1", got)
	}
	if got := u.Query().Get("redirect_uri"); got != "https://app/cb" {
		t.Errorf("stub redirect_uri = %q, want https://app/cb", got)
	}
}
