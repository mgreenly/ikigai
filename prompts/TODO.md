# prompts — UX / roadmap notes

Running, observed pain points and ideas for making sandboxed agent sessions
pleasant to drive. Captured 2026-06 after the first real end-to-end runs (the
go-vs-rust comparison test). Ordered roughly by impact, not commitment.

Context for everything below: a *session* is a prompt + config + a persistent
sandbox folder; a *run* executes the prompt and leaves files behind. The caller
drives it through MCP (`ikigenba_prompts_session_*`). The friction is concentrated at the
two seams: (1) telling the agent enough about its world to do the right thing,
and (2) getting the results back out without babysitting.

---

## 1. A proper system prompt — make the agent understand its environment

**Problem (observed).** On its first real document-writing run the agent's first
`Write` failed with `path escapes sandbox: "/sandbox/comparison.md"` — it *guessed*
its working directory, guessed wrong, and only recovered by running `pwd` to
discover the real root (`/opt/agent/data/data/sandboxes/<session>/`). That wasted
a full turn re-emitting ~30 KB of content (a real slice of the run's token cost).
The framing prompt (`internal/engine/agent/prompt.go`) doesn't tell the agent
where it is or how its tools resolve paths.

**What the agent should be told up front:**
- Its **absolute sandbox root**, and that it is the only writable location.
- The **path convention**: relative paths resolve against the sandbox root; an
  absolute path outside the root is rejected by confinement. (The `ikigenba_prompts_describe`
  example already models relative paths like `report.md` — the agent prompt should
  make that the explicit rule, not folklore.)
- That it has **no network**, so it shouldn't try to fetch/install anything.
- The **deliverable contract**: the answer is the file(s) it leaves behind, not
  its final chat message. (We already tell callers this in `ikigenba_prompts_describe`; the
  agent should hear the mirror image.)
- Its toolset and any per-tool limits (e.g. bash output is capped at 30 KB —
  `tools/bash/bash.go`; grep/read caps too) so it can plan around them.
- Optionally: the model + effort it's running under, and the output-token ceiling,
  so it can decide between "write it all at once" vs. "build it incrementally."

**Design questions:**
- Static framing prompt vs. a templated one that interpolates the live sandbox
  path / model / caps per run? (Leaning templated — the path is per-session.)
- Keep it terse. A long system prompt eats context budget every turn.

---

## 2. Getting large files out — a download / fetch story

**Problem.** Today the only way to read a deliverable is `ikigenba_prompts_session_fs_read`,
which streams the file back **through the MCP tool result** as text. We already
hit the ceiling: the go-vs-rust run's `ikigenba_prompts_session_output` log (~72 KB) blew the
tool-result token limit and had to be spilled to a temp file and chunked. A 29 KB
markdown file is fine; a multi-MB artifact (a generated dataset, an image, a zip,
a binary) is not — and `fs_read` is line/text oriented, so binaries are a
non-starter.

**Ideas to think through:**
- **Range reads** already exist (`offset`/`limit`) — lean on them, and have
  callers/agents chunk by default for anything large. Document a recommended
  chunk size.
- **A real download channel**: a short-lived, authenticated HTTP GET for a
  sandbox file (`GET /srv/prompts/sessions/{id}/fs/{path}`) that streams bytes
  directly, bypassing the MCP text-result envelope entirely. This is the clean
  answer for big or binary outputs.
- **Manifests**: `fs_list` could carry size + a content-type guess so a caller
  knows *before* reading whether to `fs_read` (small/text) or download (large/
  binary). Flag files that exceed the MCP-result limit.
- **Egress to object storage**: optionally let a run publish a named artifact to
  the per-account bucket (the platform already has per-account backup buckets)
  and hand back a URL. Keeps huge results out of the request path.
- Decide the **retention / cleanup** policy for sandbox files and run logs so the
  box doesn't fill up (`/opt/prompts/data/...`).

---

## 3. Completion notifications — stop polling

**Problem.** Driving a run today is: `run` → poll `session_get` until
`last_run.status` is terminal → read. We polled ~6× over ~6 minutes for one run.
That's wasteful for the caller and gives no signal mid-run.

**Ideas to think through:**
- **Reuse the event plane.** The suite already has an outbox/SSE event-plane
  (`eventplane/`, crm is a producer, notify is a consumer + push). Prompts emitting
  `run.started` / `run.succeeded` / `run.failed` events onto that plane would let
  `notify` push a completion notification with zero polling — and it's the
  blessed pattern, not a bespoke one.
- **SSE stream per run**: a `GET …/sessions/{id}/runs/{rid}/stream` that tails the
  run's stream-json live (the wire layer already produces append-only JSONL).
  Callers watch instead of poll; also fixes the "no progress until the turn
  flushes" blind spot we hit.
- **Webhook callback**: session config gains an optional callback URL hit on
  terminal state. Simple for non-event-plane consumers.
- **MCP-side**: investigate whether the MCP transport can deliver a notification /
  long-poll so an agent client gets woken instead of spinning on `session_get`.
- Minimum bar regardless: surface **progress** (current turn #, tokens spent,
  last tool used) in `session_get` so a poll is at least informative.

---

## 4. Honest run outcomes — don't launder truncation as success

**Problem (latent bug).** The agent loop (`internal/engine/agent/loop.go`) only
continues on a `tool_use` stop reason; a `max_tokens` stop falls straight through
to the success path and is reported `is_error:false`. With the output cap now
defaulting to the model max this rarely fires, but if a run *does* hit the
ceiling it'll silently return truncated output as a success.

**Fix:** treat a `max_tokens` (or otherwise-incomplete) stop as a distinguished
terminal state — either surface it on the run (`status:"truncated"` /
`stop_reason` field) or continue the turn. Either way the caller must be able to
tell "finished" from "ran out of room." Pairs naturally with exposing
`stop_reason` in the run record.

---

## 5. Smaller papercuts / things to decide

- **`max_tokens` semantics are now "model max by default."** Good, but document
  it (in `ikigenba_prompts_describe` config notes) so callers know an unset value isn't a
  small cap. Consider whether a per-session hard ceiling is ever wanted for cost
  control.
- **Cost visibility.** `usage_json` already carries `total_cost_usd` — consider
  surfacing a running cost estimate during the run, and/or a per-session budget
  cap that aborts a runaway loop.
- **Run cancellation UX.** We have `session_cancel`; make sure it's discoverable
  and that a cancelled run's partial files are still readable.
- **Multi-file deliverables.** `fs_list` + `fs_read` already handle this, but the
  "the answer is a file" contract should generalize to "the answer is the file
  *tree*" — a caller shouldn't have to guess filenames. (Manifest from #2 helps.)
- **Idempotency / re-runs.** A session's sandbox persists between runs (it's the
  only memory). Document what a second `run` sees, and whether callers should
  expect to clean the sandbox between unrelated tasks.
- **Observability for the operator.** Trace logging exists (`engine/trace`);
  make sure run failures land somewhere greppable (`journalctl -u prompts`) with
  enough context to diagnose without re-running.

---

## Guiding principle

The two experiences to optimize for:
1. **The agent inside the run** should never have to *discover* its environment by
   trial and error (#1) — tell it everything it needs once, clearly.
2. **The caller driving the run** should never have to *poll or babysit* (#3),
   and should be able to get results back regardless of size or type (#2), with
   an honest account of whether the work actually completed (#4).
