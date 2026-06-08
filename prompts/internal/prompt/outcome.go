package prompt

import (
	"encoding/json"
	"fmt"

	"eventplane/outbox"
)

// Outcome event types (event-triggering decisions §3). prompts emits exactly two
// terminal, STATIC, compile-time-known outcome types (source = prompts) — two
// distinct types, NOT one type with a status field, so a consumer can filter at
// the type level. They match prompts' runs.status vocabulary
// (running|succeeded|failed|cancelled): run.succeeded on a clean run,
// run.failed on a terminal failure. A cancelled run emits NEITHER (it is an
// operator-initiated stop, not a run outcome a downstream consumer announces).
const (
	EventRunSucceeded = "run.succeeded"
	EventRunFailed    = "run.failed"
)

// Events is prompts' published-event Registry, wired via Spec.Events (STATIC —
// the outcome types are fixed at build time, unlike cron's dynamic Publishes).
// It is the single source of truth for both the reflection tool and Append-time
// validation: the runner can only Append a type that appears here. Each entry
// carries a filled-in Sample of its real payload struct so schema, example, and
// wire shape cannot diverge (mirrors crm.Events).
var Events = outbox.Registry{
	{
		Type:        EventRunSucceeded,
		Description: "A prompts run finished successfully. Carries the prompt identity, the human-readable task name, and the trigger context (the upstream source, fired event type, and upstream event id that started it, empty for a manual run).",
		Sample:      sampleOutcomeSuccess,
	},
	{
		Type:        EventRunFailed,
		Description: "A prompts run terminated in failure. Same shape as run.succeeded plus an error string describing the terminal failure.",
		Sample:      sampleOutcomeFailure,
	},
}

// outcomePayload is the run.succeeded / run.failed payload: the run's prompt
// identity, the human-readable task name (prompt_name), the run id, and the
// trigger context (trigger_source + trigger_type + trigger_event_id — the
// upstream source, fired event type, and upstream event id that started the run,
// all empty/omitted for a manual run). error is present ONLY on run.failed
// (omitempty drops it on the success path). The full report stays in prompts
// (read via MCP); this is the minimal-for-now outcome shape.
type outcomePayload struct {
	PromptID       string `json:"prompt_id"`
	PromptName     string `json:"prompt_name"`
	RunID          string `json:"run_id"`
	TriggerSource  string `json:"trigger_source"`
	TriggerType    string `json:"trigger_type"`
	TriggerEventID string `json:"trigger_event_id"`
	Error          string `json:"error,omitempty"`
}

var sampleOutcomeSuccess = outcomePayload{
	PromptID:       "01J9Z2K7P3QC8M4R6T0V2X5YA",
	PromptName:     "nightly market scan",
	RunID:          "01J9Z2M0XB4D7F9H1K3N5Q7RZ",
	TriggerSource:  "cron",
	TriggerType:    "cron.nightly",
	TriggerEventID: "01J9Z2J5W8R3T6V9Y2B4D6F8GH",
}

var sampleOutcomeFailure = outcomePayload{
	PromptID:       "01J9Z2K7P3QC8M4R6T0V2X5YA",
	PromptName:     "nightly market scan",
	RunID:          "01J9Z2M0XB4D7F9H1K3N5Q7RZ",
	TriggerSource:  "cron",
	TriggerType:    "cron.nightly",
	TriggerEventID: "01J9Z2J5W8R3T6V9Y2B4D6F8GH",
	Error:          "run TTL exceeded",
}

// outcomeEvent builds the outbox event for a run's terminal state. status is the
// run's terminal runs.status; only RunSucceeded / RunFailed produce an event
// (the caller passes through other terminal states without emitting). errMsg is
// carried only on the failed event.
func outcomeEvent(status, promptID, promptName, runID, triggerSource, triggerType, triggerEventID, errMsg string) (outbox.Event, bool, error) {
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
		PromptID:       promptID,
		PromptName:     promptName,
		RunID:          runID,
		TriggerSource:  triggerSource,
		TriggerType:    triggerType,
		TriggerEventID: triggerEventID,
		Error:          errMsg,
	})
	if err != nil {
		return outbox.Event{}, false, fmt.Errorf("prompt: marshal %s payload: %w", typ, err)
	}
	return outbox.Event{Type: typ, Payload: raw}, true, nil
}
