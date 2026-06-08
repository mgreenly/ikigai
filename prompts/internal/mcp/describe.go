package mcp

// describeText is the on-demand deep overview returned by the describe
// tool. It is intentionally NOT loaded into the initialize `instructions`
// field (which every client pays on every connection) — callers load it only
// when they choose to call describe, conserving context for callers that
// already know the surface.
//
// Source of truth for the concepts below is README.md / ARCHITECTURE.md in this
// module; this is a concise restatement, not a substitute. Keep it conceptual —
// do not re-list every tool's schema here (that is what tools/list is for).
const describeText = `Prompts runs sandboxed Claude agent sessions on your behalf.

WHAT IT IS
- A *prompt* is a reusable definition: a user_prompt, an optional system_prompt,
  a model config, and an optional name. It holds no per-run state of its own.
- A *run* is one execution of a prompt by a Claude agent, and is the addressable
  unit of work — it has its own run_id, its own per-run sandbox folder, and its
  own append-only output log. Runs are fully concurrent: you may start any number
  of runs of one prompt at once, each isolated in its own sandbox.
- At run start the prompt's current user_prompt / system_prompt / config are
  *pinned to disk* for that run, the agent executes from that frozen input, and
  the sandbox + log are persisted afterward. Editing or deleting the prompt
  mid-run never changes what an in-flight run executes.
- The agent inside a run has bash/read/write/edit/glob/grep, confined to that
  run's sandbox folder, with no network. Its deliverables are the files it leaves
  behind. Everything is isolated per caller.

ADDRESSING — TWO KEYS
- Prompt-addressed tools take a prompt_id and are BARE verbs:
  create, list, get, update, delete, set_trigger, clear_trigger, run.
- Run-addressed tools take a run_id and live under the run_* namespace:
  run_list, run_get, run_output, run_cancel, run_fs_list, run_fs_read.
  run_list is the bridge (a prompt's runs); the rest key by run_id and stay
  readable even after the prompt is deleted.

LIFECYCLE
  1. create {user_prompt, config} -> {prompt_id}
  2. run {prompt_id}              -> {run_id, status:"running", started_at}
  3. poll run_get {run_id}        -> watch status until it is
     succeeded | failed | cancelled
  4. run_output {run_id}          -> the run's narrated log
  5. run_fs_list / run_fs_read {run_id} -> the files the agent wrote

OUTPUT FORMAT
- run_output returns a run's log as append-only stream-json:
  one JSON event per line (the agent's turn-by-turn event stream). offset is
  1-based; limit caps lines — tail a long run by advancing offset.
- The *answer* is usually a file in the run's sandbox (e.g. report.md), not the
  agent's final message — read it with run_fs_read.

TRIGGERS — MULTI-SOURCE
- A prompt can be wired to event triggers so it runs automatically. Each trigger
  is one (source, event_filter) binding; a prompt may hold several across several
  upstream producers (cron|crm|ledger|dropbox|scripts|prompts, the last being
  prompts' own run.succeeded/run.failed for self-chaining). Attach with
  set_trigger, remove with clear_trigger, or
  pass an inline "triggers" array to create.

EVENT-TRIGGERED RUNS
- When a trigger fires, the run's user message carries a SECOND block with the
  triggering event as JSON: {source, type, event_id, payload}. payload is the
  upstream producer's fact body — small by design (often just an id) — so write
  your user_prompt to read the event, then call the in-run suite tools to fetch
  any detail you need. e.g. "When triggered by crm contact.created, take
  payload.id, load the contact via the crm tool, then …".
- This block is present ONLY on event-triggered runs; a manual run has none.

DELETE
- delete is a tombstone: it removes the prompt and its triggers
  but leaves its runs and their on-disk artifacts in place — those stay readable
  by run_id via run_get / run_output / run_fs_*.

WORKED EXAMPLE
  create {"user_prompt":"Summarize X into report.md",
                           "config":{"model":"claude-sonnet-4-6","effort":"low"}}
    -> {"prompt_id":"P"}
  run {"prompt_id":"P"}            -> {"run_id":"R","status":"running",...}
  run_get {"run_id":"R"}           # repeat until status=="succeeded"
  run_fs_list {"run_id":"R"}                    # -> report.md
  run_fs_read {"run_id":"R","path":"report.md"}

CONFIG
  model is required (e.g. claude-sonnet-4-6 / claude-haiku-4-5, or aliases
  opus|sonnet|haiku); optional effort (low|medium|high|xhigh|max, model-
  dependent), max_tokens, temperature. health proves the auth chain.`

// toolDescribe returns the on-demand overview. Takes no inputs.
func toolDescribe() (map[string]any, error) {
	return toolResultText(describeText), nil
}
