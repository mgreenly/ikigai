package anthropic

import (
	"bytes"
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"

	"agentkit/provider"
)

// completeTranscript is a minimal successful Messages SSE stream: usage in
// message_start, a stop_reason + output usage in message_delta, then
// message_stop.
const completeTranscript = `event: message_start
data: {"type":"message_start","message":{"usage":{"input_tokens":12,"output_tokens":1,"cache_read_input_tokens":4}}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"hi"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":7}}

event: message_stop
data: {"type":"message_stop"}

`

func decodeOneLog(t *testing.T, buf *bytes.Buffer) map[string]any {
	t.Helper()
	dec := json.NewDecoder(bytes.NewReader(buf.Bytes()))
	var recs []map[string]any
	for dec.More() {
		var m map[string]any
		if err := dec.Decode(&m); err != nil {
			t.Fatalf("decode log record: %v (buf=%q)", err, buf.String())
		}
		recs = append(recs, m)
	}
	if len(recs) != 1 {
		t.Fatalf("want exactly one accounting record, got %d (buf=%q)", len(recs), buf.String())
	}
	return recs[0]
}

// TestP0cAnthropicAccountingRecord pins that one Stream completion emits
// exactly one JSON accounting record carrying the pre-bound call_site, a
// non-zero cost_usd from the registry pricing, and the provider/model/effort.
func TestP0cAnthropicAccountingRecord(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("content-type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(completeTranscript))
	}))
	defer srv.Close()

	c, err := New("sk-ant-test", "claude-sonnet-4-6")
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	c.baseURL = srv.URL

	var buf bytes.Buffer
	c.SetLogger(slog.New(slog.NewJSONHandler(&buf, nil)).With(slog.String("call_site", "merge")))

	ch, err := c.Stream(context.Background(), provider.Request{Model: "claude-sonnet-4-6", Effort: "high"})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	for range ch {
	}

	rec := decodeOneLog(t, &buf)
	if rec["call_site"] != "merge" {
		t.Errorf("call_site = %v, want merge", rec["call_site"])
	}
	if rec["provider"] != "anthropic" {
		t.Errorf("provider = %v, want anthropic", rec["provider"])
	}
	if rec["model"] != "claude-sonnet-4-6" {
		t.Errorf("model = %v, want claude-sonnet-4-6", rec["model"])
	}
	if rec["effort"] != "high" {
		t.Errorf("effort = %v, want high", rec["effort"])
	}
	if rec["stop_reason"] != "end_turn" {
		t.Errorf("stop_reason = %v, want end_turn", rec["stop_reason"])
	}
	if got := rec["cache_read_tokens"]; got != float64(4) {
		t.Errorf("cache_read_tokens = %v, want 4", got)
	}
	if cost, _ := rec["cost_usd"].(float64); cost <= 0 {
		t.Errorf("cost_usd = %v, want > 0", rec["cost_usd"])
	}
}

// TestP0cAnthropicNilLogger pins that without a logger no record is emitted
// and the stream still completes.
func TestP0cAnthropicNilLogger(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("content-type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(completeTranscript))
	}))
	defer srv.Close()

	c, err := New("sk-ant-test", "claude-sonnet-4-6")
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	c.baseURL = srv.URL

	ch, err := c.Stream(context.Background(), provider.Request{Model: "claude-sonnet-4-6"})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	for range ch {
	}
}

// TestP0cAnthropicEffortAdaptiveThinking pins that a request carrying an
// effort sends an enabled extended-thinking block on the wire (effort parity),
// and that an empty effort sends no thinking block.
func TestP0cAnthropicEffortAdaptiveThinking(t *testing.T) {
	var gotBody map[string]any
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		w.Header().Set("content-type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(completeTranscript))
	}))
	defer srv.Close()

	c, err := New("sk-ant-test", "claude-sonnet-4-6")
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	c.baseURL = srv.URL

	// With effort: thinking enabled with a positive budget.
	ch, err := c.Stream(context.Background(), provider.Request{Model: "claude-sonnet-4-6", Effort: "high"})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	for range ch {
	}
	thinking, ok := gotBody["thinking"].(map[string]any)
	if !ok {
		t.Fatalf("expected a thinking block on the wire with effort set, got %v", gotBody["thinking"])
	}
	if thinking["type"] != "enabled" {
		t.Errorf("thinking.type = %v, want enabled", thinking["type"])
	}
	if b, _ := thinking["budget_tokens"].(float64); b <= 0 {
		t.Errorf("thinking.budget_tokens = %v, want > 0", thinking["budget_tokens"])
	}

	// Without effort: no thinking block.
	gotBody = nil
	ch, err = c.Stream(context.Background(), provider.Request{Model: "claude-sonnet-4-6"})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	for range ch {
	}
	if _, present := gotBody["thinking"]; present {
		t.Errorf("expected no thinking block without effort, got %v", gotBody["thinking"])
	}
}
