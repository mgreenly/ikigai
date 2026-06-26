// Package script is scripts' domain: the scripts + runs + triggers state and
// the python3-exec run lifecycle that fronts it. The MCP handler talks only to
// Service; Service is the only thing that mutates script/run/trigger rows. See
// ARCHITECTURE.md §4 (data model), §5.1 (responsibilities), §6 (MCP→Service
// mapping) and PLAN.md Part A.
package script

import "errors"

// Run status values (runs.status).
const (
	RunRunning   = "running"
	RunSucceeded = "succeeded"
	RunFailed    = "failed"
	RunCancelled = "cancelled"
)

// Config is the normalized config blob stored as scripts.config_json. It is
// validated/normalized at create time and stored verbatim as JSON so the schema
// stays stable as the set of tunables grows. Minimal day-one.
type Config struct {
	// Day-one minimal. Reserved for later; validated/normalized at create.
	Interpreter string `json:"interpreter,omitempty"`  // reserved; "python3" only day-one
	TimeoutSecs int    `json:"timeout_secs,omitempty"` // reserved; SCRIPTS_RUN_TTL is the backstop
}

type Script struct {
	ID, OwnerEmail, Name, Body string
	Config                     Config
	// SourcePath is the originating Dropbox mirror path for an import-managed
	// script ("" ⇒ SQL NULL, the hand-authored case). Re-importing the same
	// path upserts the same row (the partial unique index on
	// (owner_email, source_path) enforces it).
	SourcePath           string
	CreatedAt, UpdatedAt string
}

type Run struct {
	ID             string `json:"id"`
	ScriptID       string `json:"script_id"`
	Status         string `json:"status"`
	ExitCode       *int   `json:"exit_code"` // nil while running / never-started
	StartedAt      string `json:"started_at"`
	EndedAt        string `json:"ended_at"`       // "" while running
	Error          string `json:"error"`          // failure / TTL / spawn reason
	TriggerSource  string `json:"trigger_source"` // "" for a manual run
	TriggerType    string `json:"trigger_type"`
	TriggerEventID string `json:"trigger_event_id"`
	StdoutPath     string `json:"stdout_path"`
	StderrPath     string `json:"stderr_path"`
	ElapsedSecs    int    `json:"elapsed_secs"` // computed in service, not stored
}

type Trigger struct {
	ScriptID, Source, EventFilter, CreatedAt string
}

// FileEntry is one node in a run's persisted dir tree (run_fs_list).
type FileEntry struct {
	Path  string `json:"path"` // relative to the run dir root
	IsDir bool   `json:"is_dir"`
	Size  int64  `json:"size"` // bytes; 0 for dirs
}

type ScriptDetail struct {
	Script
	RunningCount int // derived: COUNT(runs WHERE status='running')
	LastRun      *Run
}

// Sentinel errors. ErrNotFound on missing/foreign owner.
var (
	ErrNotFound   = errors.New("script: not found")
	ErrValidation = errors.New("script: validation")
)
