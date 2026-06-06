package mcp

// describeText is the on-demand deep overview returned by the ikigenba_ralph_describe
// tool. It is intentionally NOT loaded into the initialize `instructions`
// field (which every client pays on every connection) — agents load it only
// when they choose to call ikigenba_ralph_describe, conserving context for callers that
// already know the surface.
//
// Source of truth for the concepts below is README.md / ARCHITECTURE.md in this
// module; this is a concise restatement, not a substitute. Keep it conceptual —
// do not re-list every tool's schema here (that is what tools/list is for).
const describeText = `Ralph runs sandboxed Claude agent sessions on your behalf.

WHAT IT IS
- A *session* is a durable object: a prompt, a model config, and a persistent
  sandbox folder. The folder is the session's only memory between runs.
- A *run* is one execution of the session's prompt by a Claude agent. Runs are
  conversationally stateless — anything that must persist is written to the
  sandbox folder. A session is single-flight: only one run at a time.
- The agent inside a run has bash/read/write/edit/glob/grep, confined to the
  sandbox folder, with no network. Its deliverables are the files it leaves
  behind. Sessions are isolated per caller.

LIFECYCLE
  1. ikigenba_ralph_session_create {prompt, config} -> {session_id, status:"idle"}
  2. ikigenba_ralph_session_run {session_id}        -> starts a run, returns immediately
  3. poll ikigenba_ralph_session_get {session_id}   -> watch last_run.status until it is
     succeeded | failed | cancelled
  4. ikigenba_ralph_session_output {session_id}     -> the run's narrated log
  5. ikigenba_ralph_session_fs_list / ikigenba_ralph_session_fs_read -> the files the agent wrote

OUTPUT FORMAT
- ikigenba_ralph_session_output returns the latest run's log as append-only stream-json:
  one JSON event per line (the agent's turn-by-turn event stream). offset is
  1-based; limit caps lines — tail a long run by advancing offset.
- The *answer* is usually a file in the sandbox (e.g. report.md), not the
  agent's final message — read it with ikigenba_ralph_session_fs_read.

WORKED EXAMPLE
  ikigenba_ralph_session_create {"prompt":"Summarize X into report.md",
                        "config":{"model":"claude-sonnet-4-6","effort":"low"}}
    -> {"session_id":"S","status":"idle"}
  ikigenba_ralph_session_run {"session_id":"S"}
  ikigenba_ralph_session_get {"session_id":"S"}   # repeat until last_run.status=="succeeded"
  ikigenba_ralph_session_fs_list {"session_id":"S"}                    # -> report.md
  ikigenba_ralph_session_fs_read {"session_id":"S","path":"report.md"}

CONFIG
  model is required (e.g. claude-sonnet-4-6 / claude-haiku-4-5, or aliases
  opus|sonnet|haiku); optional effort (low|medium|high|xhigh|max, model-
  dependent), max_tokens, temperature. ikigenba_ralph_health proves the auth chain.`

// toolDescribe returns the on-demand overview. Takes no inputs.
func toolDescribe() (map[string]any, error) {
	return toolResultText(describeText), nil
}
