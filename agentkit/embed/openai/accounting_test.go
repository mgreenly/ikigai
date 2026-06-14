package openai_test

import (
	"bytes"
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"testing"
)

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

// TestP0cEmbedAccountingRecord pins that one Embed call emits exactly one
// JSON accounting record with the pre-bound call_site, a non-zero cost_usd
// from the embeddings pricing rate, and the input-token count — and no
// stop_reason (embeddings have none).
func TestP0cEmbedAccountingRecord(t *testing.T) {
	c := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(`{"data":[{"index":0,"embedding":[0.1,0.2]}],"usage":{"prompt_tokens":1000}}`))
	})

	var buf bytes.Buffer
	c.SetLogger(slog.New(slog.NewJSONHandler(&buf, nil)).With(slog.String("call_site", "embed")))

	if _, err := c.Embed(context.Background(), "text-embedding-3-large", 2, []string{"x"}); err != nil {
		t.Fatalf("Embed: %v", err)
	}

	rec := decodeOneLog(t, &buf)
	if rec["call_site"] != "embed" {
		t.Errorf("call_site = %v, want embed", rec["call_site"])
	}
	if rec["provider"] != "openai" {
		t.Errorf("provider = %v, want openai", rec["provider"])
	}
	if rec["model"] != "text-embedding-3-large" {
		t.Errorf("model = %v, want text-embedding-3-large", rec["model"])
	}
	if got := rec["input_tokens"]; got != float64(1000) {
		t.Errorf("input_tokens = %v, want 1000", got)
	}
	// text-embedding-3-large: $0.13/M input. 1000 tokens => 1000/1e6*0.13 = 1.3e-4.
	if cost, _ := rec["cost_usd"].(float64); cost <= 0 {
		t.Errorf("cost_usd = %v, want > 0", rec["cost_usd"])
	}
	if _, present := rec["stop_reason"]; present {
		t.Errorf("embed record must not carry a stop_reason, got %v", rec["stop_reason"])
	}
}

// TestP0cEmbedNilLogger pins that with no logger no record is emitted and the
// call still succeeds.
func TestP0cEmbedNilLogger(t *testing.T) {
	c := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(`{"data":[{"index":0,"embedding":[0.1,0.2]}],"usage":{"prompt_tokens":5}}`))
	})
	if _, err := c.Embed(context.Background(), "text-embedding-3-large", 2, []string{"x"}); err != nil {
		t.Fatalf("Embed: %v", err)
	}
}

// TestP0cEmbedEmptyTextsNoRecord pins that an empty-texts call makes no API
// call and therefore emits no accounting record.
func TestP0cEmbedEmptyTextsNoRecord(t *testing.T) {
	c := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		t.Errorf("server must not be hit for empty texts")
	})
	var buf bytes.Buffer
	c.SetLogger(slog.New(slog.NewJSONHandler(&buf, nil)))
	if _, err := c.Embed(context.Background(), "text-embedding-3-large", 2, nil); err != nil {
		t.Fatalf("Embed: %v", err)
	}
	if buf.Len() != 0 {
		t.Errorf("empty-texts call must emit no record, got %q", buf.String())
	}
}
