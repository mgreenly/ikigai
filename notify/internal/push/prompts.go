package push

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"

	"eventplane/consumer"
)

// promptsSource is the upstream source prompts presents on its outbox envelopes (§8.3) and
// the key for notify's SECOND feed_offset row (the crm loop owns the "crm" row,
// this loop owns the "prompts" row — independent cursors, never shared).
const promptsSource = "prompts"

// Outcome event types prompts emits on its /feed (event-triggering decisions §3,
// P8): a run finishing successfully or in terminal failure. notify pushes on
// BOTH so the owner is told about failures too.
const (
	eventRunSucceeded = "run.succeeded"
	eventRunFailed    = "run.failed"
)

// runOutcome is the slice of prompts's run.succeeded / run.failed payload
// (event-triggering decisions §3) that notify needs for the push: the
// human-readable task name and, on a failure, the error string. The other fields
// (session_id, trigger_event, scheduled_for) are decoded only to validate the
// envelope is well-formed; they do not shape the notification.
type runOutcome struct {
	SessionName string `json:"session_name"`
	Error       string `json:"error"`
}

// PromptsSubscriptions are notify's two declared in-edges on prompts's feed
// (event-triggering decisions §4): run.succeeded and run.failed. As with the crm
// Subscription, these are the ONE source of truth — the prompts consumer Handler
// matches each event against them and the reflection tool reports them via
// Spec.Subscriptions, so the runtime filter and what reflection advertises cannot
// drift (decision 10). The Handler field is left unset; the engine wiring uses
// the Subscription only as a declared graph edge.
func PromptsSubscriptions() []consumer.Subscription {
	return []consumer.Subscription{
		{
			Source:      promptsSource,
			Filter:      "prompts:" + eventRunSucceeded + "/**",
			Description: "fires a best-effort ntfy.sh push (Title \"Run succeeded\", body = session_name) for the outcome of any prompt run, named or not",
		},
		{
			Source:      promptsSource,
			Filter:      "prompts:" + eventRunFailed + "/**",
			Description: "fires a best-effort ntfy.sh push (Title \"Run failed\", body = session_name + error) for the outcome of any prompt run, named or not",
		},
	}
}

// PromptsHandler returns the consumer.Handler notify hands to the prompts consumer
// loop. It mirrors the crm Handler's classification exactly (event-triggering
// decisions §1/§4): it runs the effect only for run.succeeded / run.failed
// (consumer-side filtering, §7.3) and ignores every other type — the engine still
// commits the cursor for those, so they do not re-arrive. A matched event fires
// the push ASYNCHRONOUSLY in a detached, timeout-bound goroutine (decision 16) so
// the handler returns immediately and the engine commits the cursor without
// waiting; a slow or dead ntfy therefore never stalls prompts's feed (best-effort —
// a push failure is logged and dropped, never surfaced). A malformed payload is
// semantic poison → ErrSkip (log loud + advance), never a stalling error.
func PromptsHandler(c *Client, logger *slog.Logger) consumer.Handler {
	if logger == nil {
		logger = slog.Default()
	}
	return func(ctx context.Context, ev consumer.Event) error {
		var title string
		switch ev.Kind {
		case eventRunSucceeded:
			title = "Run succeeded"
		case eventRunFailed:
			title = "Run failed"
		default:
			return nil // not ours — the engine advances the cursor anyway (§7.3)
		}
		var p runOutcome
		if err := json.Unmarshal(ev.Payload, &p); err != nil {
			// A malformed payload can never decode, so retrying it would stall the
			// feed forever. Wrap ErrSkip so the engine logs it loud and advances the
			// cursor past it (event-triggering decisions §1).
			return fmt.Errorf("push: decode %s %s: %w: %w", ev.Kind, ev.ID, err, consumer.ErrSkip)
		}
		body := p.SessionName
		if ev.Kind == eventRunFailed && p.Error != "" {
			body = p.SessionName + ": " + p.Error
		}
		go func(title, body string) {
			// Detached from the engine's request context (the handler has already
			// returned) but bounded by pushTimeout via the client and this ctx, so the
			// goroutine always terminates (decision 16).
			ctx, cancel := context.WithTimeout(context.Background(), pushTimeout)
			defer cancel()
			c.Send(ctx, title, body)
		}(title, body)
		return nil
	}
}
