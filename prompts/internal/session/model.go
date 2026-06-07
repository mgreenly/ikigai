// Package session is agent's domain: the sessions + runs state machine and
// the single-flight gate that fronts the async run lifecycle. The MCP handler
// talks only to Service; Service is the only thing that mutates session/run
// rows. See ARCHITECTURE.md §4 (data model), §5.1 (responsibilities), §7
// (MCP→Service mapping).
package session

import "errors"

// Session status values (sessions.status).
const (
	StatusIdle    = "idle"
	StatusRunning = "running"
)

// Run status values (runs.status).
const (
	RunRunning   = "running"
	RunSucceeded = "succeeded"
	RunFailed    = "failed"
	RunCancelled = "cancelled"
)

// Sentinel errors for MCP mapping.
var (
	// ErrNotFound — session missing, or owned by another caller.
	ErrNotFound = errors.New("session: not found")
	// ErrBusy — Run requested while a run is already in flight.
	ErrBusy = errors.New("session: run already in flight")
	// ErrRunning — Update/Delete requested while the session is running.
	ErrRunning = errors.New("session: session is running")
)

// Config is the normalized config blob stored as sessions.config_json. It is
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

// Session mirrors the sessions table; Config is carried parsed.
type Session struct {
	ID           string `json:"id"`
	OwnerEmail   string `json:"owner_email"`
	Name         string `json:"name,omitempty"`
	Prompt       string `json:"prompt"`
	SystemPrompt string `json:"system_prompt,omitempty"`
	Config       Config `json:"config"`
	Status       string `json:"status"`
	CreatedAt    string `json:"created_at"`
	UpdatedAt    string `json:"updated_at"`
}

// Run mirrors the runs table. Nullable columns (ended_at, usage_json, error)
// are exposed as plain strings; "" means SQL NULL.
//
// TriggerEvent / ScheduledFor are NOT persisted columns — they are the trigger
// context carried in-memory from the event-triggered fire path (the cron event
// that started the run) through to the runner's terminal write, where they
// become fields of the run.succeeded / run.failed outcome payload. Both are
// empty for a manual (MCP-initiated) run, which is the documented
// manual-run representation (event-triggering decisions §3 payload).
type Run struct {
	ID        string `json:"id"`
	SessionID string `json:"session_id"`
	Status    string `json:"status"`
	StartedAt string `json:"started_at"`
	EndedAt   string `json:"ended_at,omitempty"`
	UsageJSON string `json:"usage_json,omitempty"`
	Error     string `json:"error,omitempty"`
	LogPath   string `json:"log_path"`

	// Trigger context (in-memory only; not stored). Empty for a manual run.
	TriggerEvent string `json:"-"`
	ScheduledFor string `json:"-"`
}

// SessionDetail is a Session plus its latest run (nil if it has never run);
// returned by Get for the MCP last_run surface.
type SessionDetail struct {
	Session
	LastRun *Run `json:"last_run"`
}
