// Package prompt is prompts' domain: the prompts + runs state machine that
// fronts the async run lifecycle. The MCP handler talks only to Service;
// Service is the only thing that mutates prompt/run rows. Runs are fully
// concurrent — there is no prompt status and no single-flight gate. See
// ARCHITECTURE.md §4 (data model), §5.1 (responsibilities), §7
// (MCP→Service mapping).
package prompt

import "errors"

// Run status values (runs.status).
const (
	RunRunning   = "running"
	RunSucceeded = "succeeded"
	RunFailed    = "failed"
	RunCancelled = "cancelled"
)

// Sentinel errors for MCP mapping.
var (
	// ErrNotFound — prompt missing, or owned by another caller.
	ErrNotFound = errors.New("prompt: not found")
	// ErrValidation — an unsatisfiable trigger binding (unknown source, or an
	// event_filter that matches no event the producer publishes; A12). The MCP
	// layer maps it to an invalid-params response. ValidationError (below) plays
	// the same role for config validation.
	ErrValidation = errors.New("prompt: validation")
	// ErrBusy and ErrRunning are GONE — full concurrency, no single-flight,
	// no "rejected while running".
)

// Config is the normalized config blob stored as prompts.config_json. It is
// validated at create/update time (model resolves to an Anthropic model, key
// present) and stored verbatim as JSON so the schema stays stable as the set
// of tunables grows.
type Config struct {
	Provider    string   `json:"provider"`
	Model       string   `json:"model"`
	Effort      string   `json:"effort,omitempty"`
	MaxTokens   int      `json:"max_tokens,omitempty"`
	Temperature *float64 `json:"temperature,omitempty"`
}

// Prompt mirrors the prompts table; Config is carried parsed.
type Prompt struct {
	ID           string `json:"id"`
	OwnerEmail   string `json:"owner_email"`
	Name         string `json:"name,omitempty"`
	UserPrompt   string `json:"user_prompt"`
	SystemPrompt string `json:"system_prompt,omitempty"`
	Config       Config `json:"config"`
	CreatedAt    string `json:"created_at"`
	UpdatedAt    string `json:"updated_at"`
	// SourcePath is the originating Dropbox mirror path for an import-managed
	// prompt ("" ⇒ SQL NULL, a hand-authored prompt). It is the upsert key for
	// re-import (idempotency, enforced by idx_prompts_source) and marks the row as
	// import-managed. See docs/adr-dropbox-import-sync.md (Decision 2).
	SourcePath string `json:"source_path,omitempty"`
	// NOTE: no Status field — full concurrency, no prompt lifecycle.
}

// Run mirrors the runs table. Nullable columns (ended_at, usage_json, error)
// are exposed as plain strings; "" means SQL NULL.
//
// OwnerEmail is denormalized from the prompt at run start so the run stays
// owner-addressable. PromptName is captured at run start for the outcome event.
//
// TriggerSource / TriggerType / TriggerEventID are the trigger context PERSISTED
// on the run row. They are set from the event-triggered fire path (the upstream
// source, fired event type, and upstream event id that started the run) and
// become fields of the run.succeeded / run.failed outcome payload, which
// FinishRun reads back from the run row. All three are empty for a manual
// (MCP-initiated) run, the documented manual-run representation.
type Run struct {
	ID         string `json:"id"`
	PromptID   string `json:"prompt_id"`
	OwnerEmail string `json:"owner_email"`
	PromptName string `json:"prompt_name,omitempty"`
	Status     string `json:"status"`
	StartedAt  string `json:"started_at"`
	EndedAt    string `json:"ended_at,omitempty"`
	UsageJSON  string `json:"usage_json,omitempty"`
	Error      string `json:"error,omitempty"`
	LogPath    string `json:"log_path"`

	// Trigger context (persisted). Empty for a manual run.
	TriggerSource  string `json:"trigger_source,omitempty"`   // producer source id (cron|crm|ledger|dropbox|scripts|prompts)
	TriggerType    string `json:"trigger_type,omitempty"`     // the fired event type
	TriggerEventID string `json:"trigger_event_id,omitempty"` // the upstream event id
}

// Trigger is one (prompt, source, event_filter) binding. A prompt may hold
// many — one per upstream event it reacts to. The cron-only staleness/retry
// knobs are GONE (fire-and-forget, symmetric with scripts).
type Trigger struct {
	PromptID    string `json:"prompt_id"`
	Source      string `json:"source"`       // producer source id (cron|crm|ledger|dropbox|scripts|prompts)
	EventFilter string `json:"event_filter"` // the event type/glob, e.g. "cron.nightly"
	CreatedAt   string `json:"created_at"`
}

// TriggerSpec is the create-time sugar: triggers passed to Create are applied
// via SetTrigger after the prompt row is inserted (same validation).
type TriggerSpec struct {
	Source      string `json:"source"`
	EventFilter string `json:"event_filter"`
}

// PromptDetail is a Prompt plus its derived run summary: RunningCount (the
// number of its runs currently in 'running') and LastRun (its newest run, nil
// if it has never run). Returned by Get/List for the MCP last_run surface.
type PromptDetail struct {
	Prompt
	RunningCount int  `json:"running_count"`
	LastRun      *Run `json:"last_run"`
}
