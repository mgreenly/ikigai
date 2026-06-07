// Package consume is agent's event-plane CONSUMER domain: it turns cron's
// time events into agent runs. It is the mirror of notify's internal/push —
// where notify reacts to crm's contact.created with a best-effort push, agent
// reacts to cron's cron.<name> by firing a run on every session that declared
// that trigger.
//
// The model is **in-memory fire-and-run** (event-triggering decisions §3, the
// SIMPLIFIED model — there is deliberately NO run_intents table, no drain
// worker, no crash-recovery sweep beyond agent's existing orphaned-run sweep).
// On a cron.<name> event the handler:
//
//   - looks up every session whose trigger matches the event type (the fan-out),
//   - per session, applies a staleness guard (skip if now-scheduled_for exceeds
//     the session's max_staleness — this also coalesces a replay storm down to
//     the freshest tick),
//   - serializes via session.status: if the session is already running it is
//     skipped (ErrBusy from the run path),
//   - otherwise starts a run, retrying a transient start failure with a FIXED
//     delay up to the session's max_attempts (default 3),
//   - and **always returns nil** — fire-and-forget. An individual session's run
//     failure must never stall the cron feed (it would block every other
//     session's trigger). Failures are logged loudly. A malformed cron payload
//     is semantic poison → ErrSkip (log loud + advance).
//
// Accepted trade-off: a crash mid-fire loses the in-memory run/retry state and
// the cron event was already consumed, so that occurrence is missed — the next
// cron tick recovers. A rare duplicate run is tolerated.
package consume

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"time"

	"eventplane/consumer"

	"prompts/internal/session"
)

// retryDelay is the FIXED in-memory delay between fire attempts (decisions §3:
// "read and wait", a constant — NOT exponential backoff). It is a package var so
// tests can shrink it; production never touches it.
var retryDelay = 2 * time.Second

// FireFunc is the testable seam: it starts a run for a session by id and returns
// the run's terminal-start error (nil on a successful start). The production
// wiring is session.Service.RunByID; tests inject a stub so no real Claude
// session is ever created. A FireFunc returning session.ErrBusy means the
// session is already running (the serialization guard) — not retried.
//
// triggerEvent / scheduledFor are the cron event's type and matched slot; they
// are carried through to the run so its terminal outcome event (run.succeeded /
// run.failed) reports the trigger context that started it (event-triggering
// decisions §3). A manual run carries empty strings instead.
type FireFunc func(ctx context.Context, sessionID, triggerEvent, scheduledFor string) error

// triggerLookup is the fan-out query seam: event type → matching triggers. The
// production wiring is session.Store.TriggersForEvent.
type triggerLookup func(ctx context.Context, eventType string) ([]session.Trigger, error)

// cronPayload is the slice of cron's cron.<name> payload (docs/event-protocol +
// cron decisions §2) agent needs: scheduled_for drives the staleness guard.
type cronPayload struct {
	Name         string `json:"name"`
	ScheduledFor string `json:"scheduled_for"` // UTC RFC3339, the matched slot
	FiredAt      string `json:"fired_at"`      // UTC RFC3339, the actual emit time
}

// Subscription is agent's declared event-plane in-edge: it listens to every
// cron.* type and fires the matching sessions' runs. The fan-out is per-session
// (a session's trigger names a specific cron.<name>), so the subscription glob
// is broad — "cron.*" — and the per-session match is the exact trigger_event in
// the DB. This is the ONE source of truth the reflection tool reports.
func Subscription() consumer.Subscription {
	return consumer.Subscription{
		Source:      "cron",
		Filter:      "cron.*",
		Description: "starts a run for every agent session whose trigger matches the fired cron event (in-memory fire-and-run)",
	}
}

// Handler returns the consumer.Handler agent hands to the engine. fire starts a
// run for a session id; lookup fans an event type out to its matching triggers.
// The handler ALWAYS returns nil for a matched event (fire-and-forget) and
// ErrSkip only for a structurally-unprocessable payload — it never returns a
// stalling error, so a slow or failing run never blocks the cron feed.
func Handler(fire FireFunc, lookup triggerLookup, logger *slog.Logger) consumer.Handler {
	if logger == nil {
		logger = slog.Default()
	}
	sub := Subscription()
	return func(ctx context.Context, ev consumer.Event) error {
		if !sub.Match(ev.Type) {
			return nil // not ours — the engine advances the cursor anyway (§7.3)
		}
		var p cronPayload
		if err := json.Unmarshal(ev.Payload, &p); err != nil {
			// Malformed payload is semantic poison: it can never decode, so
			// retrying would stall the feed forever. Skip + advance.
			return fmt.Errorf("consume: decode %s %s: %w: %w", ev.Type, ev.ID, err, consumer.ErrSkip)
		}

		triggers, err := lookup(ctx, ev.Type)
		if err != nil {
			// A lookup failure is a transient DB read, NOT poison. But decisions
			// §3 is explicit: the handler returns nil and never stalls the cron
			// feed. Log loudly and advance — the next tick recovers.
			logger.Error("consume: trigger lookup failed (advancing)", "event", ev.Type, "err", err)
			return nil
		}
		if len(triggers) == 0 {
			return nil // no session listens to this type — advance
		}

		scheduledFor, parseErr := time.Parse(time.RFC3339, p.ScheduledFor)
		for _, t := range triggers {
			// Per-session staleness guard at receipt (decisions §3): skip an
			// occurrence older than the session's max_staleness — start nothing,
			// emit nothing. A bad/empty scheduled_for can't be evaluated, so it is
			// treated as fresh (do not silently drop a fire on a parse quirk).
			if parseErr == nil {
				age := time.Since(scheduledFor)
				if age > time.Duration(t.MaxStalenessSecs)*time.Second {
					logger.Warn("consume: skipping stale occurrence",
						"session", t.SessionID, "event", ev.Type,
						"scheduled_for", p.ScheduledFor, "age_secs", int(age.Seconds()),
						"max_staleness_secs", t.MaxStalenessSecs)
					continue
				}
			}
			// Each session's fire runs on its own goroutine so one session's
			// fixed-delay retry never blocks the rest of the fan-out (and the
			// handler returns promptly, never holding the cursor open). The cron
			// event type + matched slot ride through to the run's outcome payload.
			go fireWithRetry(ctx, fire, t, ev.Type, p.ScheduledFor, logger)
		}
		return nil
	}
}

// fireWithRetry starts a run for one session, retrying a transient start failure
// with the FIXED retryDelay up to t.MaxAttempts. session.ErrBusy (the session is
// already running — the serialization guard) is NOT retried: it is the intended
// skip, logged at debug. A clean start ends the loop. After MaxAttempts the
// fire is given up on, logged loudly — the next cron tick is the recovery.
func fireWithRetry(ctx context.Context, fire FireFunc, t session.Trigger, eventType, scheduledFor string, logger *slog.Logger) {
	attempts := t.MaxAttempts
	if attempts <= 0 {
		attempts = session.DefaultMaxAttempts
	}
	for attempt := 1; attempt <= attempts; attempt++ {
		err := fire(ctx, t.SessionID, eventType, scheduledFor)
		if err == nil {
			logger.Info("consume: fired triggered run", "session", t.SessionID, "event", eventType, "attempt", attempt)
			return
		}
		if isBusy(err) {
			// Serialization guard: the session is already running. Not a failure
			// to retry — the staleness guard plus this skip together bound the
			// fan-out to one in-flight run per session.
			logger.Debug("consume: session busy, skipping fire", "session", t.SessionID, "event", eventType)
			return
		}
		logger.Error("consume: fire attempt failed", "session", t.SessionID, "event", eventType, "attempt", attempt, "max_attempts", attempts, "err", err)
		if attempt < attempts {
			select {
			case <-ctx.Done():
				return
			case <-time.After(retryDelay):
			}
		}
	}
	logger.Error("consume: gave up firing triggered run", "session", t.SessionID, "event", eventType, "attempts", attempts)
}

// isBusy reports whether err is the single-flight ErrBusy (session already
// running) — the serialization skip, distinct from a retriable start failure.
func isBusy(err error) bool {
	return errors.Is(err, session.ErrBusy)
}
