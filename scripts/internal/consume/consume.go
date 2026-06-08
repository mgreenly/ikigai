// Package consume is scripts' event-plane CONSUMER domain: it fans events from
// every upstream producer (cron, crm, ledger, dropbox, prompts) out to the
// scripts whose triggers match, firing a run per match. It is the multi-upstream
// mirror of notify's internal/push and prompts' internal/consume — where those
// react to a single upstream, scripts wires one Handler per upstream source and
// fans each event out to N matching scripts.
//
// The model is fire-and-run: on each event the handler looks up the subscribed
// scripts via ScriptsForEvent(source, type) and starts a run per script on its
// own goroutine (unbounded, non-blocking). It ALWAYS returns nil for a matched
// event (fire-and-forget — never stall the feed) and ErrSkip only for a
// structurally-malformed envelope (poison → log loud + advance). The cursor
// advances for every event. See PLAN.md §A9 and ARCHITECTURE.md §5.3.
package consume

import (
	"context"
	"fmt"
	"log/slog"

	"eventplane/consumer"
)

// FireFunc starts a run for one script in reaction to an event (the consumer
// path; not owner-scoped). Production wiring is script.Service.RunForEvent.
type FireFunc func(ctx context.Context, scriptID, source, evType, eventID string, payload []byte) error

// LookupFunc fans (source, type) out to the subscribed script_ids. Production
// wiring is script.Service.ScriptsForEvent.
type LookupFunc func(ctx context.Context, source, evType string) ([]string, error)

// Subscriptions returns one Subscription per upstream — Filter "*" (scripts
// fires on any type and decides via ScriptsForEvent). Source is the producer's
// source id. These are the LIVE in-edges the reflection tool reports. The
// per-script trigger glob does the real filtering downstream, so the engine-level
// subscription is deliberately broad.
func Subscriptions(sources []string) []consumer.Subscription {
	subs := make([]consumer.Subscription, 0, len(sources))
	for _, src := range sources {
		subs = append(subs, consumer.Subscription{
			Source:      src,
			Filter:      "*", // fire on any type; ScriptsForEvent decides the real fan-out
			Description: fmt.Sprintf("fires a run for every script whose trigger matches a %s event", src),
		})
	}
	return subs
}

// Handler returns the consumer.Handler scripts hands to one upstream's engine
// (one Handler per Subscription, with that upstream's source baked in). Per
// event: ScriptsForEvent(source, ev.Type) -> RunForEvent per script on its own
// goroutine (unbounded, non-blocking). Returns nil for a well-formed event
// (matched-zero is still success — fire-and-forget, never stall); ErrSkip only on
// a structurally-malformed envelope (no type / no id — semantic poison that can
// never be processed). The cursor advances for every event.
func Handler(fire FireFunc, lookup LookupFunc, source string, logger *slog.Logger) consumer.Handler {
	if logger == nil {
		logger = slog.Default()
	}
	return func(ctx context.Context, ev consumer.Event) error {
		// A well-formed event frame carries a dotted type and an id (§8.1/§8.3);
		// without either it is structurally unprocessable poison. Skip + advance so
		// it never stalls the feed forever (event-triggering decisions §1). This is
		// the handler's only error path — a fired run never returns a stalling error.
		if ev.Type == "" || ev.ID == "" {
			return fmt.Errorf("consume: malformed envelope from %s (type=%q id=%q): %w", source, ev.Type, ev.ID, consumer.ErrSkip)
		}

		scriptIDs, err := lookup(ctx, source, ev.Type)
		if err != nil {
			// A lookup failure is a transient DB read, NOT poison. The fire-and-forget
			// contract is explicit: never stall the feed. Log loudly and advance — a
			// later event re-exercises the trigger set.
			logger.Error("consume: trigger lookup failed (advancing)", "source", source, "type", ev.Type, "event_id", ev.ID, "err", err)
			return nil
		}
		if len(scriptIDs) == 0 {
			// No script listens to this (source, type). Matched-zero is success: the
			// engine advances the cursor so the event does not re-arrive (§7.3).
			logger.Debug("consume: no scripts match event", "source", source, "type", ev.Type, "event_id", ev.ID)
			return nil
		}

		logger.Debug("consume: dispatching event", "source", source, "type", ev.Type, "event_id", ev.ID, "scripts", len(scriptIDs))
		for _, scriptID := range scriptIDs {
			// Each script's run starts on its own goroutine so one slow/failing start
			// never blocks the rest of the fan-out and the handler returns promptly,
			// never holding the cursor open. Detached from the engine's request
			// context (the handler has already returned) via context.Background().
			go func(scriptID string) {
				if err := fire(context.Background(), scriptID, source, ev.Type, ev.ID, ev.Payload); err != nil {
					logger.Error("consume: fire run failed (dropped)", "script", scriptID, "source", source, "type", ev.Type, "event_id", ev.ID, "err", err)
				}
			}(scriptID)
		}
		return nil
	}
}
