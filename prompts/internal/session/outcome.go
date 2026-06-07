package session

import (
	"encoding/json"
	"fmt"

	"eventplane/outbox"
)

// Outcome event types (event-triggering decisions §3). agent emits exactly two
// terminal, STATIC, compile-time-known outcome types (source = agent) — two
// distinct types, NOT one type with a status field, so a consumer can filter at
// the type level. They match agent's runs.status vocabulary
// (running|succeeded|failed|cancelled): run.succeeded on a clean run,
// run.failed on a terminal failure. A cancelled run emits NEITHER (it is an
// operator-initiated stop, not a run outcome a downstream consumer announces).
const (
	EventRunSucceeded = "run.succeeded"
	EventRunFailed    = "run.failed"
)

// Events is agent's published-event Registry, wired via Spec.Events (STATIC —
// the outcome types are fixed at build time, unlike cron's dynamic Publishes).
// It is the single source of truth for both the reflection tool and Append-time
// validation: the runner can only Append a type that appears here. Each entry
// carries a filled-in Sample of its real payload struct so schema, example, and
// wire shape cannot diverge (mirrors crm.Events).
var Events = outbox.Registry{
	{
		Type:        EventRunSucceeded,
		Description: "A agent run finished successfully. Carries the session identity, the human-readable task name, and the trigger context (the cron event + scheduled slot that started it, empty for a manual run).",
		Sample:      sampleOutcomeSuccess,
	},
	{
		Type:        EventRunFailed,
		Description: "A agent run terminated in failure. Same shape as run.succeeded plus an error string describing the terminal failure.",
		Sample:      sampleOutcomeFailure,
	},
}

// outcomePayload is the run.succeeded / run.failed payload
// (event-triggering decisions §3): the run's session identity, the
// human-readable task name (session_name), and the trigger context
// (trigger_event + scheduled_for — the cron event and matched slot that started
// the run, both empty/omitted for a manual run). error is present ONLY on
// run.failed (omitempty drops it on the success path). The full report stays in
// agent (read via MCP); this is the minimal-for-now outcome shape.
type outcomePayload struct {
	SessionID    string `json:"session_id"`
	SessionName  string `json:"session_name"`
	TriggerEvent string `json:"trigger_event"`
	ScheduledFor string `json:"scheduled_for"`
	Error        string `json:"error,omitempty"`
}

var sampleOutcomeSuccess = outcomePayload{
	SessionID:    "01J9Z2K7P3QC8M4R6T0V2X5YA",
	SessionName:  "nightly market scan",
	TriggerEvent: "cron.nightly",
	ScheduledFor: "2026-06-06T08:00:00Z",
}

var sampleOutcomeFailure = outcomePayload{
	SessionID:    "01J9Z2K7P3QC8M4R6T0V2X5YA",
	SessionName:  "nightly market scan",
	TriggerEvent: "cron.nightly",
	ScheduledFor: "2026-06-06T08:00:00Z",
	Error:        "run TTL exceeded",
}

// outcomeEvent builds the outbox event for a run's terminal state. status is the
// run's terminal runs.status; only RunSucceeded / RunFailed produce an event
// (the caller passes through other terminal states without emitting). errMsg is
// carried only on the failed event.
func outcomeEvent(status, sessionID, sessionName, triggerEvent, scheduledFor, errMsg string) (outbox.Event, bool, error) {
	var typ string
	switch status {
	case RunSucceeded:
		typ = EventRunSucceeded
		errMsg = "" // success never carries an error
	case RunFailed:
		typ = EventRunFailed
	default:
		// cancelled (or any non-outcome terminal state) emits no event.
		return outbox.Event{}, false, nil
	}
	raw, err := json.Marshal(outcomePayload{
		SessionID:    sessionID,
		SessionName:  sessionName,
		TriggerEvent: triggerEvent,
		ScheduledFor: scheduledFor,
		Error:        errMsg,
	})
	if err != nil {
		return outbox.Event{}, false, fmt.Errorf("session: marshal %s payload: %w", typ, err)
	}
	return outbox.Event{Type: typ, Payload: raw}, true, nil
}
