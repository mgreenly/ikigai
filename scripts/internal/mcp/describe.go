package mcp

// describeText is the on-demand deep overview returned by the
// ikigenba_scripts_describe tool. It is intentionally NOT loaded into the
// initialize `instructions` field (which every client pays on every connection)
// — callers load it only when they choose to call ikigenba_scripts_describe.
//
// Source of truth for the concepts below is README.md / ARCHITECTURE.md in this
// module; this is a concise restatement, not a substitute.
const describeText = `Scripts runs Python scripts on your behalf — manually or on an event trigger.

WHAT IT IS
- A *script* is a durable object: a name, a Python body, and a minimal config.
- A *run* is one execution of the script's body by python3. Runs are unbounded
  (no single-flight): many runs of one script can be in flight at once. Each run
  has its own persistent dir (main.py, config.json, stdout.log, stderr.log, and
  any files the script wrote).
- A *trigger* binds a script to an upstream event (e.g. crm contact.created):
  when a matching event fires, scripts starts a run with the event payload on
  stdin and in $EVENT_JSON.

RUNTIME CONTRACT
- python3 >= 3.11, bash >= 5.0, network access, Python standard library only
  (no third-party packages day-one).

LIFECYCLE
  1. ikigenba_scripts_create {name, body}        -> {script_id}
  2. ikigenba_scripts_run {script_id}            -> starts a run, returns {run_id}
  3. poll ikigenba_scripts_run_get {run_id}      -> status until succeeded|failed|cancelled
  4. ikigenba_scripts_run_output {run_id}        -> stdout/stderr logs
  5. ikigenba_scripts_run_fs_list / ikigenba_scripts_run_fs_read -> files the run wrote

TRIGGERS
  ikigenba_scripts_set_trigger {script_id, source, event_filter} binds the script
  to a producer (cron|crm|ledger|dropbox|prompts|scripts). On a matching event a
  run starts automatically. Completion emits scripts.succeeded / scripts.failed
  on this service's own /feed, so scripts can chain off each other.

ikigenba_scripts_health proves the auth chain and reports the runtime contract.`

// toolDescribe returns the on-demand overview. Takes no inputs.
func toolDescribe() (map[string]any, error) {
	return toolResultText(describeText), nil
}
