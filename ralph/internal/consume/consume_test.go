package consume

import (
	"context"
	"encoding/json"
	"errors"
	"sync"
	"testing"
	"time"

	"eventplane/consumer"

	"ralph/internal/session"
)

// shrink the fixed retry delay so retry tests run fast.
func init() { retryDelay = 1 * time.Millisecond }

// recorder is a FireFunc stub: it records every (session id) it is asked to fire,
// returns a programmed error per session, and signals a WaitGroup so a test can
// wait for the async fan-out to settle without sleeping.
type recorder struct {
	mu      sync.Mutex
	calls   map[string]int
	errs    map[string]error // error to return for a session (nil = success)
	failN   map[string]int   // fail the first N attempts for a session, then succeed
	wg      *sync.WaitGroup
}

func newRecorder() *recorder {
	return &recorder{calls: map[string]int{}, errs: map[string]error{}, failN: map[string]int{}}
}

func (r *recorder) fire(ctx context.Context, sessionID string) error {
	r.mu.Lock()
	r.calls[sessionID]++
	n := r.calls[sessionID]
	var err error
	if fn, ok := r.failN[sessionID]; ok {
		if n <= fn {
			err = errors.New("transient start failure")
		}
	} else {
		err = r.errs[sessionID]
	}
	wg := r.wg
	r.mu.Unlock()
	if wg != nil {
		wg.Done()
	}
	return err
}

func (r *recorder) count(sessionID string) int {
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.calls[sessionID]
}

func staticLookup(triggers ...session.Trigger) triggerLookup {
	return func(ctx context.Context, eventType string) ([]session.Trigger, error) {
		var out []session.Trigger
		for _, t := range triggers {
			if t.TriggerEvent == eventType {
				out = append(out, t)
			}
		}
		return out, nil
	}
}

func cronEvent(t *testing.T, name, scheduledFor string) consumer.Event {
	t.Helper()
	payload, err := json.Marshal(cronPayload{Name: name, ScheduledFor: scheduledFor, FiredAt: scheduledFor})
	if err != nil {
		t.Fatalf("marshal payload: %v", err)
	}
	return consumer.Event{Type: "cron." + name, ID: "ev1", Source: "cron", Payload: payload}
}

func trig(sessionID, event string) session.Trigger {
	return session.Trigger{SessionID: sessionID, TriggerEvent: event, MaxStalenessSecs: 300, MaxAttempts: 3}
}

// TestHandler_FanOut: a cron.<name> fires a run for every matching session and
// none for a non-matching one; the handler returns nil.
func TestHandler_FanOut(t *testing.T) {
	rec := newRecorder()
	var wg sync.WaitGroup
	wg.Add(2) // two matching sessions
	rec.wg = &wg

	lookup := staticLookup(
		trig("s-a", "cron.nightly"),
		trig("s-b", "cron.nightly"),
		trig("s-c", "cron.hourly"),
	)
	h := Handler(rec.fire, lookup, nil)

	// scheduled_for = now so it is fresh (no staleness skip).
	ev := cronEvent(t, "nightly", time.Now().UTC().Format(time.RFC3339))
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("handler returned non-nil: %v", err)
	}
	wg.Wait()

	if rec.count("s-a") != 1 || rec.count("s-b") != 1 {
		t.Fatalf("expected one fire each for s-a, s-b; got a=%d b=%d", rec.count("s-a"), rec.count("s-b"))
	}
	if rec.count("s-c") != 0 {
		t.Fatalf("non-matching session s-c should not fire; got %d", rec.count("s-c"))
	}
}

// TestHandler_StalenessGuard: an occurrence older than max_staleness is skipped
// (no fire), the handler returns nil.
func TestHandler_StalenessGuard(t *testing.T) {
	rec := newRecorder()
	// max_staleness 60s; scheduled_for 10m ago → stale → skip.
	tr := trig("s-stale", "cron.nightly")
	tr.MaxStalenessSecs = 60
	h := Handler(rec.fire, staticLookup(tr), nil)

	old := time.Now().UTC().Add(-10 * time.Minute).Format(time.RFC3339)
	if err := h(context.Background(), cronEvent(t, "nightly", old)); err != nil {
		t.Fatalf("handler returned non-nil: %v", err)
	}
	// Give any (erroneous) goroutine a moment; none should have fired.
	time.Sleep(20 * time.Millisecond)
	if rec.count("s-stale") != 0 {
		t.Fatalf("stale occurrence must not fire; got %d", rec.count("s-stale"))
	}
}

// TestHandler_BusySkip: a session already running (fire returns ErrBusy) is
// fired exactly once (the busy result is the serialization skip, NOT retried).
func TestHandler_BusySkip(t *testing.T) {
	rec := newRecorder()
	rec.errs["s-busy"] = session.ErrBusy
	var wg sync.WaitGroup
	wg.Add(1)
	rec.wg = &wg

	h := Handler(rec.fire, staticLookup(trig("s-busy", "cron.nightly")), nil)
	ev := cronEvent(t, "nightly", time.Now().UTC().Format(time.RFC3339))
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("handler returned non-nil: %v", err)
	}
	wg.Wait()
	time.Sleep(20 * time.Millisecond) // ensure no retry sneaks in
	if got := rec.count("s-busy"); got != 1 {
		t.Fatalf("busy session must be fired exactly once (no retry); got %d", got)
	}
}

// TestHandler_RetryUpToCap: a transient start failure is retried with the fixed
// delay up to max_attempts (3) then given up; the handler still returned nil.
func TestHandler_RetryUpToCap(t *testing.T) {
	rec := newRecorder()
	rec.failN["s-fail"] = 99 // always fail → exhausts the cap
	var wg sync.WaitGroup
	wg.Add(3) // max_attempts
	rec.wg = &wg

	h := Handler(rec.fire, staticLookup(trig("s-fail", "cron.nightly")), nil)
	ev := cronEvent(t, "nightly", time.Now().UTC().Format(time.RFC3339))
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("handler must return nil even when a run keeps failing: %v", err)
	}
	wg.Wait()
	time.Sleep(20 * time.Millisecond) // ensure no 4th attempt
	if got := rec.count("s-fail"); got != 3 {
		t.Fatalf("expected exactly 3 attempts (cap), got %d", got)
	}
}

// TestHandler_RetryThenSucceed: a fire that fails twice then succeeds stops
// retrying at the success (not the cap).
func TestHandler_RetryThenSucceed(t *testing.T) {
	rec := newRecorder()
	rec.failN["s-ok"] = 2 // fail first 2, succeed on 3rd
	var wg sync.WaitGroup
	wg.Add(3)
	rec.wg = &wg

	h := Handler(rec.fire, staticLookup(trig("s-ok", "cron.nightly")), nil)
	ev := cronEvent(t, "nightly", time.Now().UTC().Format(time.RFC3339))
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("handler returned non-nil: %v", err)
	}
	wg.Wait()
	time.Sleep(20 * time.Millisecond)
	if got := rec.count("s-ok"); got != 3 {
		t.Fatalf("expected 3 attempts (2 fail + 1 success), got %d", got)
	}
}

// TestHandler_MalformedPayloadSkips: a payload that cannot decode returns ErrSkip
// (log loud + advance), never a stalling error, and fires nothing.
func TestHandler_MalformedPayloadSkips(t *testing.T) {
	rec := newRecorder()
	h := Handler(rec.fire, staticLookup(trig("s", "cron.nightly")), nil)
	ev := consumer.Event{Type: "cron.nightly", ID: "ev1", Source: "cron", Payload: json.RawMessage(`{bad`)}
	err := h(context.Background(), ev)
	if !errors.Is(err, consumer.ErrSkip) {
		t.Fatalf("expected ErrSkip for malformed payload, got %v", err)
	}
}

// TestHandler_NonMatchingTypeAdvances: an unrelated event type is a no-op that
// returns nil (the engine still advances the cursor).
func TestHandler_NonMatchingTypeAdvances(t *testing.T) {
	rec := newRecorder()
	h := Handler(rec.fire, staticLookup(trig("s", "cron.nightly")), nil)
	ev := consumer.Event{Type: "contact.created", ID: "ev1", Source: "crm", Payload: json.RawMessage(`{}`)}
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("non-matching type must return nil, got %v", err)
	}
	if rec.count("s") != 0 {
		t.Fatalf("non-matching type must fire nothing")
	}
}

// TestHandler_LookupFailureAdvances: a trigger-lookup error never stalls the
// cron feed — the handler logs and returns nil (next tick recovers).
func TestHandler_LookupFailureAdvances(t *testing.T) {
	rec := newRecorder()
	failing := func(ctx context.Context, eventType string) ([]session.Trigger, error) {
		return nil, errors.New("db read failed")
	}
	h := Handler(rec.fire, failing, nil)
	ev := cronEvent(t, "nightly", time.Now().UTC().Format(time.RFC3339))
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("lookup failure must not stall the feed (return nil), got %v", err)
	}
}
