// Package consume is prompts' event-plane CONSUMER domain: it fans events from
// every upstream producer (cron, crm, ledger, dropbox, scripts, and prompts'
// own /feed for self-chaining) out to the prompts whose triggers match, firing
// a run per match. It is the multi-upstream
// mirror of notify's internal/push and scripts' internal/consume — where notify
// reacts to a single upstream, prompts wires one Handler per upstream source and
// fans each event out to N matching prompts.
//
// The model is fire-and-run: on each event the handler looks up the subscribed
// prompts via PromptsForEvent(source, type) and starts a run per prompt on its
// own goroutine (unbounded, non-blocking). It ALWAYS returns nil for a matched
// event (fire-and-forget — never stall the feed) and ErrSkip only for a
// structurally-malformed envelope (poison → log loud + advance). The cursor
// advances for every event. The old cron-only single subscription, the
// staleness guard, and the fixed-delay retry are GONE (they died with the
// max_staleness_secs / max_attempts trigger knobs).
package consume

import (
	"context"
	"fmt"
	"log/slog"

	"eventplane/consumer"
)

// FireFunc starts a run for one prompt in reaction to an event (the consumer
// path; not owner-scoped). Production wiring is prompt.Service.RunByEvent.
type FireFunc func(ctx context.Context, promptID, source, evType, eventID string, payload []byte) error

// LookupFunc fans (source, type) out to the subscribed prompt ids. Production
// wiring is prompt.Service.PromptsForEvent.
type LookupFunc func(ctx context.Context, source, evType string) ([]string, error)

// Subscriptions returns one Subscription per upstream — Filter "*" (prompts
// fires on any type and decides via PromptsForEvent). Source is the producer's
// source id. These are the LIVE in-edges the reflection tool reports. The
// per-prompt trigger glob does the real filtering downstream, so the
// engine-level subscription is deliberately broad.
func Subscriptions(sources []string) []consumer.Subscription {
	subs := make([]consumer.Subscription, 0, len(sources))
	for _, src := range sources {
		subs = append(subs, consumer.Subscription{
			Source:      src,
			Filter:      "*", // fire on any type; PromptsForEvent decides the real fan-out
			Description: fmt.Sprintf("fires a run for every prompt whose trigger matches a %s event", src),
		})
	}
	return subs
}

// Handler returns the consumer.Handler prompts hands to one upstream's engine
// (one Handler per Subscription, with that upstream's source baked in). Per
// event: PromptsForEvent(source, ev.Type) -> RunByEvent per prompt on its own
// goroutine (unbounded, non-blocking). Returns nil for a well-formed event
// (matched-zero is still success — fire-and-forget, never stall); ErrSkip only on
// a structurally-malformed envelope (no type / no id — semantic poison that can
// never be processed). The cursor advances for every event.
func Handler(fire FireFunc, lookup LookupFunc, source string, logger *slog.Logger) consumer.Handler {
	if logger == nil {
		logger = slog.Default()
	}
	return func(ctx context.Context, ev consumer.Event) error {
		// A well-formed event frame carries a dotted type and an id; without either
		// it is structurally unprocessable poison. Skip + advance so it never stalls
		// the feed forever. This is the handler's only error path — a fired run never
		// returns a stalling error.
		if ev.Type == "" || ev.ID == "" {
			return fmt.Errorf("consume: malformed envelope from %s (type=%q id=%q): %w", source, ev.Type, ev.ID, consumer.ErrSkip)
		}

		promptIDs, err := lookup(ctx, source, ev.Type)
		if err != nil {
			// A lookup failure is a transient DB read, NOT poison. The fire-and-forget
			// contract is explicit: never stall the feed. Log loudly and advance — a
			// later event re-exercises the trigger set.
			logger.Error("consume: trigger lookup failed (advancing)", "source", source, "type", ev.Type, "event_id", ev.ID, "err", err)
			return nil
		}
		if len(promptIDs) == 0 {
			// No prompt listens to this (source, type). Matched-zero is success: the
			// engine advances the cursor so the event does not re-arrive.
			logger.Debug("consume: no prompts match event", "source", source, "type", ev.Type, "event_id", ev.ID)
			return nil
		}

		logger.Debug("consume: dispatching event", "source", source, "type", ev.Type, "event_id", ev.ID, "prompts", len(promptIDs))
		for _, promptID := range promptIDs {
			// Each prompt's run starts on its own goroutine so one slow/failing start
			// never blocks the rest of the fan-out and the handler returns promptly,
			// never holding the cursor open. Detached from the engine's request context
			// (the handler has already returned) via context.Background().
			go func(promptID string) {
				if err := fire(context.Background(), promptID, source, ev.Type, ev.ID, ev.Payload); err != nil {
					logger.Error("consume: fire run failed (dropped)", "prompt", promptID, "source", source, "type", ev.Type, "event_id", ev.ID, "err", err)
				}
			}(promptID)
		}
		return nil
	}
}
