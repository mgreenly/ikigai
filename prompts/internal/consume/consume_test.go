package consume

import (
	"context"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"sync"
	"testing"
	"time"

	"eventplane/consumer"
)

func discardLogger() *slog.Logger {
	return slog.New(slog.NewJSONHandler(io.Discard, nil))
}

// fireRecorder is a fake FireFunc that records every (prompt,source,type,id,payload)
// call. Because fire runs on a detached goroutine, callers use expect/await to
// settle exactly n dispatches deterministically before asserting.
type fireRecorder struct {
	mu    sync.Mutex
	calls []fireCall
	wg    sync.WaitGroup
	err   error // returned by every fire (nil = success)
}

type fireCall struct {
	promptID, source, evType, subject, eventID string
	payload                                    []byte
}

func (f *fireRecorder) fn(ctx context.Context, promptID, source, evType, subject, eventID string, payload []byte) error {
	f.mu.Lock()
	f.calls = append(f.calls, fireCall{promptID, source, evType, subject, eventID, append([]byte(nil), payload...)})
	f.mu.Unlock()
	f.wg.Done()
	return f.err
}

func (f *fireRecorder) snapshot() []fireCall {
	f.mu.Lock()
	defer f.mu.Unlock()
	out := make([]fireCall, len(f.calls))
	copy(out, f.calls)
	return out
}

func (f *fireRecorder) expect(n int) { f.wg.Add(n) }

func (f *fireRecorder) await(t *testing.T) {
	t.Helper()
	done := make(chan struct{})
	go func() { f.wg.Wait(); close(done) }()
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("timed out waiting for fire goroutines")
	}
}

// staticLookup returns a fixed list of prompt ids for any (source,type).
func staticLookup(ids []string, err error) LookupFunc {
	return func(ctx context.Context, source, evType string) ([]string, error) {
		return ids, err
	}
}

// TestHandlerFanOut: a well-formed event whose lookup returns 2 prompt ids fires
// exactly twice (once per prompt, with the event context forwarded) and the
// handler returns nil.
func TestHandlerFanOut(t *testing.T) {
	// R-6PH9-8QNA
	fire := &fireRecorder{}
	fire.expect(2)
	var lookedUp string
	h := Handler(fire.fn, func(_ context.Context, source, key string) ([]string, error) {
		if source != "crm" {
			t.Errorf("lookup source = %q, want crm", source)
		}
		lookedUp = key
		return []string{"p1", "p2"}, nil
	}, "crm", discardLogger())

	payload := json.RawMessage(`{"id":"c1"}`)
	ev := consumer.Event{Kind: "contact.created", Subject: "/contacts/c1", ID: "01EVENT", Source: "crm", Payload: payload}
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("Handler returned %v, want nil", err)
	}
	if lookedUp != ev.Key() {
		t.Fatalf("lookup key = %q, want event key %q", lookedUp, ev.Key())
	}
	fire.await(t)

	calls := fire.snapshot()
	if len(calls) != 2 {
		t.Fatalf("fired %d runs, want 2", len(calls))
	}
	seen := map[string]bool{}
	for _, c := range calls {
		seen[c.promptID] = true
		if c.source != "crm" || c.evType != "contact.created" || c.subject != "/contacts/c1" || c.eventID != "01EVENT" {
			t.Errorf("fire got wrong context: %+v", c)
		}
		if string(c.payload) != string(payload) {
			t.Errorf("fire payload = %q, want %q", c.payload, payload)
		}
	}
	if !seen["p1"] || !seen["p2"] {
		t.Errorf("expected fires for p1 and p2, got %v", seen)
	}
}

// TestHandlerNonCronUpstream: an event from a NON-cron upstream (dropbox)
// dispatches to the matching prompt with the full (source, type, id) context —
// proving the multi-upstream fan-in, not just cron.
func TestHandlerNonCronUpstream(t *testing.T) {
	fire := &fireRecorder{}
	fire.expect(1)
	h := Handler(fire.fn, staticLookup([]string{"p-drop"}, nil), "dropbox", discardLogger())

	ev := consumer.Event{Kind: "file.created", ID: "01FILE", Source: "dropbox", Payload: json.RawMessage(`{"path":"/x"}`)}
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("Handler returned %v, want nil", err)
	}
	fire.await(t)

	calls := fire.snapshot()
	if len(calls) != 1 {
		t.Fatalf("fired %d runs, want 1", len(calls))
	}
	c := calls[0]
	if c.promptID != "p-drop" || c.source != "dropbox" || c.evType != "file.created" || c.eventID != "01FILE" {
		t.Fatalf("non-cron fire context wrong: %+v", c)
	}
}

// TestHandlerNoMatch: a well-formed event whose lookup returns 0 ids fires
// nothing and returns nil (matched-zero is success).
func TestHandlerNoMatch(t *testing.T) {
	fire := &fireRecorder{}
	h := Handler(fire.fn, staticLookup(nil, nil), "ledger", discardLogger())

	ev := consumer.Event{Kind: "transaction.recorded", ID: "01EVENT", Source: "ledger", Payload: json.RawMessage(`{}`)}
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("Handler returned %v, want nil", err)
	}
	time.Sleep(20 * time.Millisecond)
	if got := fire.snapshot(); len(got) != 0 {
		t.Fatalf("no-match fired %d runs, want 0", len(got))
	}
}

// TestHandlerMalformedSkips: a structurally-malformed envelope (missing type or
// id) returns an ErrSkip-wrapped error and fires nothing.
func TestHandlerMalformedSkips(t *testing.T) {
	cases := []struct {
		name string
		ev   consumer.Event
	}{
		{"no type", consumer.Event{Kind: "", ID: "01EVENT", Source: "crm", Payload: json.RawMessage(`{}`)}},
		{"no id", consumer.Event{Kind: "contact.created", ID: "", Source: "crm", Payload: json.RawMessage(`{}`)}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			fire := &fireRecorder{}
			lookup := func(ctx context.Context, source, evType string) ([]string, error) {
				t.Fatal("lookup called for malformed envelope")
				return nil, nil
			}
			h := Handler(fire.fn, lookup, "crm", discardLogger())

			err := h(context.Background(), tc.ev)
			if err == nil {
				t.Fatal("malformed envelope returned nil, want an ErrSkip-wrapped error")
			}
			if !errors.Is(err, consumer.ErrSkip) {
				t.Fatalf("error does not satisfy errors.Is(err, ErrSkip): %v", err)
			}
			time.Sleep(20 * time.Millisecond)
			if got := fire.snapshot(); len(got) != 0 {
				t.Fatalf("malformed envelope fired %d runs, want 0", len(got))
			}
		})
	}
}

// TestHandlerLookupErrorAdvances: a transient lookup error must NOT stall — the
// handler returns nil (and fires nothing), so the cursor advances.
func TestHandlerLookupErrorAdvances(t *testing.T) {
	fire := &fireRecorder{}
	h := Handler(fire.fn, staticLookup(nil, errors.New("db down")), "dropbox", discardLogger())

	ev := consumer.Event{Kind: "file.created", ID: "01EVENT", Source: "dropbox", Payload: json.RawMessage(`{}`)}
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("lookup error returned %v, want nil (never stall)", err)
	}
	time.Sleep(20 * time.Millisecond)
	if got := fire.snapshot(); len(got) != 0 {
		t.Fatalf("lookup error fired %d runs, want 0", len(got))
	}
}

// TestHandlerFireErrorDoesNotStall: a fire that returns an error is swallowed
// (logged + dropped) — the handler already returned nil.
func TestHandlerFireErrorDoesNotStall(t *testing.T) {
	fire := &fireRecorder{err: errors.New("spawn failed")}
	fire.expect(1)
	h := Handler(fire.fn, staticLookup([]string{"p1"}, nil), "crm", discardLogger())

	ev := consumer.Event{Kind: "contact.created", ID: "01EVENT", Source: "crm", Payload: json.RawMessage(`{}`)}
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("Handler returned %v, want nil", err)
	}
	fire.await(t)
	if got := fire.snapshot(); len(got) != 1 {
		t.Fatalf("fired %d runs, want 1", len(got))
	}
}

// TestSubscriptions: one Subscription per source, each Filter "*" with the
// source carried through.
func TestSubscriptions(t *testing.T) {
	subs := Subscriptions([]string{"cron", "crm", "ledger", "dropbox", "scripts"})
	if len(subs) != 5 {
		t.Fatalf("got %d subscriptions, want 5", len(subs))
	}
	want := map[string]bool{"cron": true, "crm": true, "ledger": true, "dropbox": true, "scripts": true}
	for _, s := range subs {
		if !want[s.Source] {
			t.Errorf("unexpected source %q", s.Source)
		}
		delete(want, s.Source)
		if s.Filter != "**" {
			t.Errorf("source %q Filter = %q, want %q", s.Source, s.Filter, "**")
		}
		if s.Description == "" {
			t.Errorf("source %q has empty Description", s.Source)
		}
	}
	if len(want) != 0 {
		t.Errorf("missing subscriptions for %v", want)
	}
}
