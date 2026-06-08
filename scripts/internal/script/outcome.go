package script

import (
	"encoding/json"
	"fmt"

	"eventplane/outbox"
)

// Completion event types (PLAN.md §A7). scripts emits exactly two terminal,
// STATIC, compile-time-known completion types (source = scripts) — two distinct
// types, NOT one type with a status field, so a consumer can filter at the type
// level. They match scripts' runs.status vocabulary: scripts.succeeded on a
// clean run (exit 0), scripts.failed on a terminal failure (non-zero / TTL /
// spawn error). A cancelled run emits NEITHER (operator-initiated stop, not a
// run outcome a downstream consumer announces).
const (
	EventSucceeded = "scripts.succeeded" // exit 0
	EventFailed    = "scripts.failed"    // non-zero / TTL / spawn error
)

// completionPayload is the scripts.succeeded / scripts.failed payload (README
// JSON block). error is present ONLY on the failed event (omitempty).
type completionPayload struct {
	ScriptID        string         `json:"script_id"`
	ScriptName      string         `json:"script_name"`
	RunID           string         `json:"run_id"`
	Status          string         `json:"status"`
	ExitCode        *int           `json:"exit_code"`
	Trigger         triggerPayload `json:"trigger"`
	Stdout          string         `json:"stdout"`
	StdoutTruncated bool           `json:"stdout_truncated"`
	Stderr          string         `json:"stderr"`
	StderrTruncated bool           `json:"stderr_truncated"`
	Error           string         `json:"error,omitempty"`
}

type triggerPayload struct {
	Source  string `json:"source"`
	Type    string `json:"type"`
	EventID string `json:"event_id"`
}

var sampleSuccess = completionPayload{
	ScriptID:   "01J9Z2K7P3QC8M4R6T0V2X5YA",
	ScriptName: "nightly export",
	RunID:      "01J9Z2K7P3QC8M4R6T0V2X5YB",
	Status:     RunSucceeded,
	Trigger:    triggerPayload{Source: "cron", Type: "cron.nightly", EventID: "01J9Z2K7P3QC8M4R6T0V2X5YC"},
	Stdout:     "exported 42 rows\n",
}

var sampleFailure = completionPayload{
	ScriptID:   "01J9Z2K7P3QC8M4R6T0V2X5YA",
	ScriptName: "nightly export",
	RunID:      "01J9Z2K7P3QC8M4R6T0V2X5YB",
	Status:     RunFailed,
	Trigger:    triggerPayload{Source: "cron", Type: "cron.nightly", EventID: "01J9Z2K7P3QC8M4R6T0V2X5YC"},
	Stderr:     "Traceback ...\n",
	Error:      "run TTL exceeded",
}

// Events is scripts' published-event Registry, wired via Spec.Events (STATIC —
// the completion types are fixed at build time). Single source of truth for both
// the reflection tool and Append-time validation: the runner can only Append a
// type that appears here.
var Events = outbox.Registry{
	{
		Type:        EventSucceeded,
		Description: "A scripts run finished successfully (exit 0). Carries the script identity, the captured output tails, and the trigger context that started it (empty for a manual run).",
		Sample:      sampleSuccess,
	},
	{
		Type:        EventFailed,
		Description: "A scripts run terminated in failure (non-zero exit / TTL / spawn error). Same shape as scripts.succeeded plus an error string.",
		Sample:      sampleFailure,
	},
}

// completionEvent builds the outbox.Event from a FinishRunInput. Returns
// (event, shouldEmit, err). shouldEmit=false ONLY for status==cancelled.
func completionEvent(in FinishRunInput) (outbox.Event, bool, error) {
	var typ string
	errMsg := in.ErrMsg
	switch in.Status {
	case RunSucceeded:
		typ = EventSucceeded
		errMsg = "" // success never carries an error
	case RunFailed:
		typ = EventFailed
	default:
		// cancelled (or any non-outcome terminal state) emits no event — an
		// operator abort is not a script outcome and must not fire chains.
		return outbox.Event{}, false, nil
	}
	raw, err := json.Marshal(completionPayload{
		ScriptID:   in.ScriptID,
		ScriptName: in.ScriptName,
		RunID:      in.RunID,
		Status:     in.Status,
		ExitCode:   in.ExitCode,
		Trigger: triggerPayload{
			Source:  in.TriggerSource,
			Type:    in.TriggerType,
			EventID: in.TriggerEventID,
		},
		Stdout:          in.StdoutTail,
		StdoutTruncated: in.StdoutTrunc,
		Stderr:          in.StderrTail,
		StderrTruncated: in.StderrTrunc,
		Error:           errMsg,
	})
	if err != nil {
		return outbox.Event{}, false, fmt.Errorf("script: marshal %s payload: %w", typ, err)
	}
	return outbox.Event{Type: typ, Payload: raw}, true, nil
}
