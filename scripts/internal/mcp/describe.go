package mcp

// describeText is the on-demand deep overview returned by the
// describe tool. It is intentionally NOT loaded into the
// initialize `instructions` field (which every client pays on every connection)
// — callers load it only when they choose to call describe.
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
- A *trigger* binds a script to a canonical upstream routing key (e.g.
  dropbox:create/bills/**/*.pdf):
  when a matching event fires, scripts starts a run with the event payload on
  stdin and in $EVENT_JSON.

RUNTIME CONTRACT
- python3 >= 3.11, bash >= 5.0, and network access. The Python standard library
  and the preinstalled suite module are available; no other third-party packages
  are installed day-one.
- The suite module is importable in every run with: import suite
- suite.event() returns the trigger payload verbatim as a dict, or {} for a
  manual run.
    event = suite.event()
- suite.mcp(service, tool, args) calls any suite service's MCP tool and returns
  its structured result as a dict, or text for a prose tool.
    contact = suite.mcp("crm", "contact_get", {"contact_id": contact_id})
- suite.fetch(content_url, dest) writes the bytes behind a content URL from an
  event payload or tool result to a local file.
    suite.fetch(invoice["content_url"], "invoice.pdf")
- suite.files.* accesses the account's file share: list, stat, get, put, delete,
  move, and mkdir. This is the durable, shared file store: its files persist and
  sync, and are what the owner and other workflows see; the run dir is private
  working space. Put a product in the file share to publish it durably and let
  watching workflows trigger.
  Share paths are absolute and /-rooted; relative spellings are accepted and treated as rooted.
    suite.files.put("summary.pdf", "/reports/summary.pdf")
- Suite-service failures raise suite.ToolError; its .code is one of validation,
  not_found, conflict, too_large, source_unavailable, or internal. Catch it and
  branch on .code, or let it crash the run: the
  failure is written to stderr.log and the run is marked failed.
    try: suite.mcp(service, tool, args)
    except suite.ToolError as err: print(err.code)
- Products travel by reference. Non-directory run_fs_list entries carry a
  content_url that other services can fetch after the run, so hand results
  onward by writing a file instead of printing its bytes.

LIFECYCLE
  1. create {name, body}        -> {script_id}
  2. run {script_id}            -> starts a run, returns {run_id}
  3. poll run_get {run_id}      -> status until succeeded|failed|cancelled
  4. run_output {run_id}        -> stdout/stderr logs
  5. run_fs_list / run_fs_read -> files the run wrote

TRIGGERS
  set_trigger {script_id, filter} binds the script to a canonical
  source:kind<subject> glob. The source is one of cron|crm|ledger|dropbox|prompts;
  ** matches across subject paths. On a matching event a
  run starts automatically. Completion emits succeeded / failed
  on this service's own /feed for other services (e.g. prompts) to consume.

health proves the auth chain and reports the runtime contract.`

// toolDescribe returns the on-demand overview. Takes no inputs.
func toolDescribe() (map[string]any, error) {
	return toolResultText(describeText), nil
}
