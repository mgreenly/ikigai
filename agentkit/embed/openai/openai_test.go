package openai_test

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	embedopenai "agentkit/embed/openai"
	"agentkit/provider"
)

func newTestClient(t *testing.T, handler http.HandlerFunc) *embedopenai.Client {
	t.Helper()
	srv := httptest.NewServer(handler)
	t.Cleanup(srv.Close)
	c, err := embedopenai.New("test-key")
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	c.SetBaseURL(srv.URL)
	return c
}

// TestEmbedRoundTrip verifies a unary POST returns correct-dimension vectors
// in input order and surfaces usage.prompt_tokens, fully offline.
func TestEmbedRoundTrip(t *testing.T) {
	var gotMethod, gotPath, gotAuth string
	var gotBody map[string]any

	c := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		gotMethod = r.Method
		gotPath = r.URL.Path
		gotAuth = r.Header.Get("Authorization")
		if err := json.NewDecoder(r.Body).Decode(&gotBody); err != nil {
			t.Errorf("decode body: %v", err)
		}
		// Emit rows out of order to prove index-based reordering.
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(`{
			"data":[
				{"index":1,"embedding":[0.4,0.5,0.6]},
				{"index":0,"embedding":[0.1,0.2,0.3]}
			],
			"usage":{"prompt_tokens":42}
		}`))
	})

	res, err := c.Embed(context.Background(), "text-embedding-3-large", 3, []string{"alpha", "beta"})
	if err != nil {
		t.Fatalf("Embed: %v", err)
	}

	if gotMethod != http.MethodPost {
		t.Errorf("method = %q, want POST", gotMethod)
	}
	if gotPath != "/v1/embeddings" {
		t.Errorf("path = %q, want /v1/embeddings", gotPath)
	}
	if gotAuth != "Bearer test-key" {
		t.Errorf("auth = %q, want Bearer test-key", gotAuth)
	}
	if gotBody["model"] != "text-embedding-3-large" {
		t.Errorf("model = %v", gotBody["model"])
	}
	if d, _ := gotBody["dimensions"].(float64); int(d) != 3 {
		t.Errorf("dimensions = %v, want 3", gotBody["dimensions"])
	}
	if len(res.Vectors) != 2 {
		t.Fatalf("got %d vectors, want 2", len(res.Vectors))
	}
	if got := res.Vectors[0]; len(got) != 3 || got[0] != 0.1 {
		t.Errorf("vector[0] = %v, want [0.1 0.2 0.3]", got)
	}
	if got := res.Vectors[1]; len(got) != 3 || got[0] != 0.4 {
		t.Errorf("vector[1] = %v, want [0.4 0.5 0.6]", got)
	}
	if res.InputTokens != 42 {
		t.Errorf("InputTokens = %d, want 42", res.InputTokens)
	}
}

// TestEmptyTextsNoNetwork verifies an empty input slice short-circuits without
// any HTTP call.
func TestEmptyTextsNoNetwork(t *testing.T) {
	called := false
	c := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		called = true
	})
	res, err := c.Embed(context.Background(), "text-embedding-3-large", 3, nil)
	if err != nil {
		t.Fatalf("Embed: %v", err)
	}
	if called {
		t.Error("empty texts should not make a network call")
	}
	if len(res.Vectors) != 0 {
		t.Errorf("got %d vectors, want 0", len(res.Vectors))
	}
}

// TestEmptyKeyRefused verifies an absent key is a clean construction refusal,
// not a panic — so the composition root can fall back to lexical-only.
func TestEmptyKeyRefused(t *testing.T) {
	if _, err := embedopenai.New(""); err == nil {
		t.Fatal("New(\"\") = nil error, want refusal")
	}
}

// TestErrorMapping verifies a non-2xx response becomes a typed provider.Error.
func TestErrorMapping(t *testing.T) {
	c := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusUnauthorized)
		_, _ = w.Write([]byte(`{"error":{"type":"authentication_error"}}`))
	})
	_, err := c.Embed(context.Background(), "text-embedding-3-large", 3, []string{"x"})
	if err == nil {
		t.Fatal("Embed: want error")
	}
	var perr *provider.Error
	if !errors.As(err, &perr) {
		t.Fatalf("err = %T, want *provider.Error", err)
	}
	if perr.Kind != provider.ErrAuth {
		t.Errorf("kind = %v, want ErrAuth", perr.Kind)
	}
}

// TestMissingVectorRejected verifies a response that omits a row for an input
// index is a server error, not a silent nil vector.
func TestMissingVectorRejected(t *testing.T) {
	c := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"data":[{"index":0,"embedding":[0.1]}],"usage":{"prompt_tokens":1}}`))
	})
	_, err := c.Embed(context.Background(), "text-embedding-3-large", 1, []string{"a", "b"})
	if err == nil {
		t.Fatal("Embed: want error for missing vector")
	}
}
