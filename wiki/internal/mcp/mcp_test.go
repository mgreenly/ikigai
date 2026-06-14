package mcp

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"wiki/internal/events"
	"wiki/internal/inbox"
)

func newTestHandler() *Handler {
	return NewHandler("1", "wiki",
		func(ctx context.Context) (map[string]any, error) { return map[string]any{"ok": true}, nil },
		events.Registry, nil, nil)
}

// fakeIngester is a stand-in Ingester for the front-door dispatch tests.
type fakeIngester struct {
	lastOwner, lastTitle, lastSource, lastTags string
	lastText                                   []byte
	lastURL                                    string
	rec                                        inbox.Receipt
	statusState                                map[string]any
}

func (f *fakeIngester) IngestText(_ context.Context, owner, title, source, tags string, text []byte) (inbox.Receipt, error) {
	f.lastOwner, f.lastTitle, f.lastSource, f.lastTags, f.lastText = owner, title, source, tags, text
	return f.rec, nil
}

func (f *fakeIngester) IngestURL(_ context.Context, owner, url, tags string) (inbox.Receipt, error) {
	f.lastOwner, f.lastURL, f.lastTags = owner, url, tags
	return f.rec, nil
}

func (f *fakeIngester) StatusAny(_ context.Context, id string) (any, error) {
	return f.statusState, nil
}

func newHandlerWithIngest(in Ingester) *Handler {
	return NewHandler("1", "wiki",
		func(ctx context.Context) (map[string]any, error) { return map[string]any{"ok": true}, nil },
		events.Registry, nil, in)
}

func rpc(t *testing.T, h *Handler, body string) map[string]any {
	t.Helper()
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-1")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	var out map[string]any
	if err := json.NewDecoder(rec.Body).Decode(&out); err != nil {
		t.Fatalf("decode response: %v (body=%s)", err, rec.Body.String())
	}
	return out
}

// TestToolsList: the full surface is registered (ingest_text, ingest_url,
// status, search, ask, timeline, health, reflection).
func TestToolsList(t *testing.T) {
	h := newTestHandler()
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/list"}`)
	result := out["result"].(map[string]any)
	tools := result["tools"].([]any)
	got := map[string]bool{}
	for _, tl := range tools {
		got[tl.(map[string]any)["name"].(string)] = true
	}
	for _, want := range []string{"ingest_text", "ingest_url", "status", "search", "ask", "timeline", "health", "reflection"} {
		if !got[want] {
			t.Errorf("tools/list missing %q", want)
		}
	}
}

// TestHealthLive: health returns the envelope plus the authenticated identity.
func TestHealthLive(t *testing.T) {
	h := newTestHandler()
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"health","arguments":{}}}`)
	text := toolText(t, out)
	var env map[string]any
	if err := json.Unmarshal([]byte(text), &env); err != nil {
		t.Fatalf("health envelope: %v", err)
	}
	if env["service"] != "wiki" {
		t.Errorf("service = %v", env["service"])
	}
	if env["owner_email"] != "owner@example.com" {
		t.Errorf("owner_email = %v", env["owner_email"])
	}
}

// TestReflectionLive: reflection publishes the two wiki.* events.
func TestReflectionLive(t *testing.T) {
	h := newTestHandler()
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"reflection","arguments":{}}}`)
	text := toolText(t, out)
	if !strings.Contains(text, events.TypeRowDeadLettered) || !strings.Contains(text, events.TypeIngestRefused) {
		t.Errorf("reflection index missing the two wiki events: %s", text)
	}

	// Detail for a known type round-trips a schema+example.
	out = rpc(t, h, `{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"reflection","arguments":{"event_type":"wiki.ingest_refused"}}}`)
	if d := toolText(t, out); !strings.Contains(d, "wiki.ingest_refused") {
		t.Errorf("reflection detail wrong: %s", d)
	}
}

// TestDomainToolsStubbed: the domain tools return a not-implemented error result
// (isError) until their owning phases land.
func TestDomainToolsStubbed(t *testing.T) {
	h := newTestHandler()
	for _, name := range []string{"ingest_text", "ingest_url", "status", "search", "ask", "timeline"} {
		body := `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"` + name + `","arguments":{}}}`
		out := rpc(t, h, body)
		result := out["result"].(map[string]any)
		if result["isError"] != true {
			t.Errorf("%q: expected isError stub result, got %v", name, result)
		}
	}
}

// TestIngestTextReceipt: ingest_text dispatches to the Ingester with the
// authenticated owner and returns the receipt (id + sha256 + dup) — not a job id.
func TestIngestTextReceipt(t *testing.T) {
	fake := &fakeIngester{rec: inbox.Receipt{ID: "01ARR", SHA256: "abc123", Dup: false}}
	h := newHandlerWithIngest(fake)
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"ingest_text","arguments":{"text":"hello world","title":"T","tags":["a","b"]}}}`)
	var r map[string]any
	if err := json.Unmarshal([]byte(toolText(t, out)), &r); err != nil {
		t.Fatalf("receipt: %v", err)
	}
	if r["id"] != "01ARR" || r["sha256"] != "abc123" || r["dup"] != false {
		t.Errorf("receipt = %v", r)
	}
	if fake.lastOwner != "owner@example.com" {
		t.Errorf("owner = %q, want the authenticated caller", fake.lastOwner)
	}
	if string(fake.lastText) != "hello world" {
		t.Errorf("text = %q", fake.lastText)
	}
	if fake.lastTags != `["a","b"]` {
		t.Errorf("tags = %q, want a JSON array string", fake.lastTags)
	}
}

// TestIngestTextRequiresText: a missing 'text' is a tool error, not a panic.
func TestIngestTextRequiresText(t *testing.T) {
	h := newHandlerWithIngest(&fakeIngester{})
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"ingest_text","arguments":{}}}`)
	if out["result"].(map[string]any)["isError"] != true {
		t.Errorf("expected isError for missing text")
	}
}

// TestIngestURLReceipt: ingest_url forwards the url + owner and returns a receipt.
func TestIngestURLReceipt(t *testing.T) {
	fake := &fakeIngester{rec: inbox.Receipt{ID: "01URL", SHA256: "def456", Dup: true}}
	h := newHandlerWithIngest(fake)
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"ingest_url","arguments":{"url":"https://x.test/p"}}}`)
	var r map[string]any
	_ = json.Unmarshal([]byte(toolText(t, out)), &r)
	if r["id"] != "01URL" || r["dup"] != true {
		t.Errorf("receipt = %v", r)
	}
	if fake.lastURL != "https://x.test/p" {
		t.Errorf("url = %q", fake.lastURL)
	}
}

// TestStatusDispatch: status polls the Ingester by id.
func TestStatusDispatch(t *testing.T) {
	fake := &fakeIngester{statusState: map[string]any{"id": "01ARR", "state": "pending"}}
	h := newHandlerWithIngest(fake)
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"status","arguments":{"id":"01ARR"}}}`)
	if !strings.Contains(toolText(t, out), `"state":"pending"`) {
		t.Errorf("status: %s", toolText(t, out))
	}
}

func toolText(t *testing.T, out map[string]any) string {
	t.Helper()
	result, ok := out["result"].(map[string]any)
	if !ok {
		t.Fatalf("no result in %v", out)
	}
	content := result["content"].([]any)
	var b bytes.Buffer
	for _, c := range content {
		b.WriteString(c.(map[string]any)["text"].(string))
	}
	return b.String()
}
