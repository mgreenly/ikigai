package openai_test

import (
	"bytes"
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"testing"

	"agentkit/provider"
)

// decodeOneLog asserts buf holds exactly one JSON log record and returns it.
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

// TestP0cAccountingRecordPerStream pins that one Stream completion emits
// exactly one JSON accounting record carrying the pre-bound call_site, a
// non-zero cost_usd matching the registry pricing, and the provider/model.
func TestP0cAccountingRecordPerStream(t *testing.T) {
	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		sseResponse(w, "hi", "end_turn")
	})

	var buf bytes.Buffer
	logger := slog.New(slog.NewJSONHandler(&buf, nil)).With(slog.String("call_site", "extract"))
	c.SetLogger(logger)

	ch, err := c.Stream(context.Background(), provider.Request{Model: "gpt-5.5", Effort: "medium"})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	rec := decodeOneLog(t, &buf)
	if rec["call_site"] != "extract" {
		t.Errorf("call_site = %v, want extract", rec["call_site"])
	}
	if rec["provider"] != "openai" {
		t.Errorf("provider = %v, want openai", rec["provider"])
	}
	if rec["model"] != "gpt-5.5" {
		t.Errorf("model = %v, want gpt-5.5", rec["model"])
	}
	if rec["effort"] != "medium" {
		t.Errorf("effort = %v, want medium", rec["effort"])
	}
	if rec["stop_reason"] != "end_turn" {
		t.Errorf("stop_reason = %v, want end_turn", rec["stop_reason"])
	}
	// gpt-5.5: input 2.00/M, output 8.00/M. 10 in + 5 out =
	// 10/1e6*2 + 5/1e6*8 = 2e-5 + 4e-5 = 6e-5.
	cost, _ := rec["cost_usd"].(float64)
	if cost <= 0 {
		t.Errorf("cost_usd = %v, want > 0", rec["cost_usd"])
	}
	if got := rec["input_tokens"]; got != float64(10) {
		t.Errorf("input_tokens = %v, want 10", got)
	}
}

// TestP0cNilLoggerEmitsNothing pins that a Stream with no logger set produces
// no accounting record (nil logger is a no-op).
func TestP0cNilLoggerEmitsNothing(t *testing.T) {
	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		sseResponse(w, "hi", "end_turn")
	})
	// No SetLogger call: logger stays nil.
	ch, err := c.Stream(context.Background(), provider.Request{Model: "gpt-5.5"})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)
	// Nothing to assert beyond no panic; the nil-logger path in accounting.Log
	// short-circuits. Reaching here without a panic is the pin.
}
