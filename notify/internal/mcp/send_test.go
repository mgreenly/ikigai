package mcp

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"

	"notify/internal/push"
)

// capturedSend is one request the mock ntfy server received for a send test.
type capturedSend struct {
	method   string
	path     string
	title    string
	priority string
	tags     string
	click    string
	auth     string
	body     string
}

type ntfyMock struct {
	mu     sync.Mutex
	got    []capturedSend
	status int // status to return (0 → 200)
	srv    *httptest.Server
}

func newNtfyMock(t *testing.T) *ntfyMock {
	t.Helper()
	m := &ntfyMock{}
	m.srv = httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		m.mu.Lock()
		m.got = append(m.got, capturedSend{
			method:   r.Method,
			path:     r.URL.Path,
			title:    r.Header.Get("Title"),
			priority: r.Header.Get("Priority"),
			tags:     r.Header.Get("Tags"),
			click:    r.Header.Get("Click"),
			auth:     r.Header.Get("Authorization"),
			body:     string(body),
		})
		status := m.status
		m.mu.Unlock()
		if status != 0 {
			w.WriteHeader(status)
			return
		}
		w.WriteHeader(http.StatusOK)
	}))
	t.Cleanup(m.srv.Close)
	return m
}

func (m *ntfyMock) snapshot() []capturedSend {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make([]capturedSend, len(m.got))
	copy(out, m.got)
	return out
}

func (m *ntfyMock) setStatus(s int) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.status = s
}

// handlerForMock builds a Handler whose push client targets the mock ntfy server.
func handlerForMock(t testing.TB, m *ntfyMock) http.Handler {
	t.Helper()
	return newHandlerWithClient(t, push.NewClient(m.srv.URL, "mytopic", "secret-token", discardLogger()))
}

// TestSendHappyPath: a full message/title/priority/tags/click call yields exactly
// one correctly-shaped POST to /<topic> with the mapped headers and bearer auth,
// and returns {ok:true}.
func TestSendHappyPath(t *testing.T) {
	// R-A918-YY6H
	m := newNtfyMock(t)
	h := handlerForMock(t, m)

	res := callOK(t, h, "send", map[string]any{
		"message":  "build failed on main",
		"title":    "CI",
		"priority": "high",
		"tags":     []string{"warning", "ci"},
		"click":    "https://example.com/run/123",
	})
	if ok, _ := res["ok"].(bool); !ok {
		t.Fatalf("send did not return {ok:true}: %+v", res)
	}

	got := m.snapshot()
	if len(got) != 1 {
		t.Fatalf("expected exactly one POST, got %d", len(got))
	}
	p := got[0]
	if p.method != http.MethodPost {
		t.Errorf("method = %q, want POST", p.method)
	}
	if p.path != "/mytopic" {
		t.Errorf("path = %q, want /mytopic", p.path)
	}
	if p.body != "build failed on main" {
		t.Errorf("body = %q, want the message", p.body)
	}
	if p.title != "CI" {
		t.Errorf("Title = %q, want CI", p.title)
	}
	if p.priority != "4" {
		t.Errorf("Priority = %q, want 4 (high)", p.priority)
	}
	if p.tags != "warning,ci" {
		t.Errorf("Tags = %q, want warning,ci", p.tags)
	}
	if p.click != "https://example.com/run/123" {
		t.Errorf("Click = %q, want the URL", p.click)
	}
	if p.auth != "Bearer secret-token" {
		t.Errorf("Authorization = %q, want bearer", p.auth)
	}
}

// TestSendMinimal: a bare message omits the optional headers entirely (no Title /
// Priority / Tags / Click), proving the defaults are "unset", not empty strings.
func TestSendMinimal(t *testing.T) {
	m := newNtfyMock(t)
	h := handlerForMock(t, m)

	callOK(t, h, "send", map[string]any{"message": "hello"})

	got := m.snapshot()
	if len(got) != 1 {
		t.Fatalf("expected one POST, got %d", len(got))
	}
	p := got[0]
	if p.body != "hello" {
		t.Errorf("body = %q, want hello", p.body)
	}
	for name, v := range map[string]string{"Title": p.title, "Priority": p.priority, "Tags": p.tags, "Click": p.click} {
		if v != "" {
			t.Errorf("%s header set to %q on a minimal send, want absent", name, v)
		}
	}
}

// TestSendValidation: each malformed call returns a structured `validation`
// error and fires NO POST.
func TestSendValidation(t *testing.T) {
	// R-ACOY-49EK
	cases := []struct {
		name string
		args map[string]any
	}{
		{"missing message", map[string]any{}},
		{"empty message", map[string]any{"message": "   "}},
		{"bad priority", map[string]any{"message": "x", "priority": "loud"}},
		{"relative click", map[string]any{"message": "x", "click": "/not/absolute"}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			m := newNtfyMock(t)
			h := handlerForMock(t, m)

			e := callErr(t, h, "send", c.args)
			if e["code"] != "validation" {
				t.Fatalf("code = %v, want validation: %+v", e["code"], e)
			}
			if _, exists := e["field"]; exists {
				t.Errorf("structured validation error retains retired field key: %+v", e)
			}
			if got := m.snapshot(); len(got) != 0 {
				t.Fatalf("validation failure still fired %d POSTs, want 0", len(got))
			}
		})
	}
}

// TestSendUpstreamNon2xx: when ntfy rejects the publish, send returns an
// `source_unavailable` result whose message leaks neither topic nor token.
func TestSendUpstreamNon2xx(t *testing.T) {
	// R-ADWU-I159
	m := newNtfyMock(t)
	m.setStatus(http.StatusInternalServerError)
	h := handlerForMock(t, m)

	e := callErr(t, h, "send", map[string]any{"message": "x"})
	if e["code"] != "source_unavailable" {
		t.Fatalf("code = %v, want source_unavailable: %+v", e["code"], e)
	}
	msg, _ := e["message"].(string)
	if strings.Contains(msg, "mytopic") || strings.Contains(msg, "secret-token") {
		t.Fatalf("upstream error leaked a secret: %q", msg)
	}
}

// TestSendUpstreamUnreachable: a dead ntfy server (transport error) also surfaces
// as a non-leaking `source_unavailable` result.
func TestSendUpstreamUnreachable(t *testing.T) {
	// R-ADWU-I159
	// A client pointed at a closed port: NewClient against an unroutable address.
	h := newHandlerWithClient(t, push.NewClient("http://127.0.0.1:1", "mytopic", "secret-token", discardLogger()))

	e := callErr(t, h, "send", map[string]any{"message": "x"})
	if e["code"] != "source_unavailable" {
		t.Fatalf("code = %v, want source_unavailable: %+v", e["code"], e)
	}
	msg, _ := e["message"].(string)
	if strings.Contains(msg, "mytopic") || strings.Contains(msg, "secret-token") {
		t.Fatalf("upstream error leaked a secret: %q", msg)
	}
}
