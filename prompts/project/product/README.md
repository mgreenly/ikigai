# prompts — Product

**Authority: intent.** This document owns *why* this change exists, *for whom*, what is in and out of scope, and what we *promise* the user. It does not state mechanism, exact formats, exit codes, or test assertions — those belong to `project/design/README.md`. Where the two could overlap on behavior, this doc states the *promise*; design states the *exact, checkable proof of that promise*.

## Problem

The prompts service is hardcoded to a single provider (Anthropic) with a narrow set of generation controls (effort, max_tokens, temperature). Users cannot choose a different provider or model per prompt, and cannot tune the full set of generation and retry knobs that the underlying agentkit supports. The local agentkit package that backs the service is also a dead end — it is a private fork, not the shared library.

## Purpose

Replace the local `./agentkit` dependency with the published `github.com/ikigenba/agentkit` package, and expose its full provider and configuration surface through the prompts MCP API. A prompt now declares which provider and model it runs on, and optionally tunes any generation or retry setting the agentkit supports. The change is mechanical at the library boundary and additive at the API surface — existing behavior is preserved under a clean migration.

prompts' primary surface stays **MCP** (the owner works through an agent and tools, not screens), but it now also serves one small **human web page** — a landing page at its mount root (`…/srv/prompts/`) showing the service **name** and **version** — so a logged-in person who opens it in a browser sees a styled page, not a raw error. That page is the seed of prompts' own web surface; v1 is deliberately just name+version, gated by the dashboard browser session like any web page in the suite. Agents are unaffected — they keep working through MCP.

## Users

The box owner, operating through an agent connected to the prompts MCP service. The owner states intent in natural language ("run this on OpenAI with low temperature"); the agent maps that to the structured MCP parameters. The owner never writes raw JSON — that is the agent's job.

## Scope

**In scope:**

- **Multi-provider support.** Any prompt may target any of the four providers the published agentkit supports: `anthropic`, `openai`, `google`, `zai`. Provider and model are required on every prompt; there is no implicit default.
- **Full generation and retry tuning via a config object.** Alongside provider and model, a prompt may carry an optional structured config object with any of these keys: `temperature`, `top_p`, `max_tokens`, `effort`, `thinking_budget`, `thinking_level`, `thinking`, `max_attempts`, `base_delay`, `max_delay`, `max_elapsed`, `ignore_retry_after`, `tool_loop_limit`, `base_url`. Unset keys use the agentkit's defaults; keys that do not apply to the chosen model are silently ignored by the agentkit.
- **Config update with full-replacement semantics.** Updating a prompt's config replaces the entire config object. To keep a value, re-specify it; omitting a key removes it and reverts it to the agentkit default.
- **Validation at create/update time.** An unknown provider or a model not recognised for the chosen provider is rejected immediately with a clear error. A run is never started with a config that cannot be resolved.
- **Edits take effect on the next run.** The runner pins its execution inputs at spawn time, so in-flight runs are unaffected. Any edit to provider, model, or config is reflected starting with the next run.
- **Backfill migration for existing prompts.** Existing rows with no provider set are migrated to `anthropic` so they continue to work without user intervention.
- **Serve a landing page.** At the mount root, present a small styled web page showing the service **name** and **version** to a person who opens it in a browser. It is for **logged-in humans** (gated by the dashboard browser session, like any web page in the suite — any signed-in user may view it); agents continue to use MCP. v1 shows only name+version and reads nothing about the viewer; the page exists to be the foundation prompts' web surface grows from.
- **Loopback ports resolve from the shared registry, not from literals.** prompts learns its own loopback port and every peer's loopback address by **service name** from the suite's single authoritative `registry` table, instead of those port numbers being written into prompts's own source. The addresses prompts uses are unchanged (this is behavior-preserving); the only shift is *where the number is written down*: one authoritative table, so a renumber can no longer drift silently out of sync with prompts and surface only at deploy. The existing env overrides (`PROMPTS_<SRC>_FEED_URL`, `DROPBOX_BASE_URL`) still win where set; the registry only supplies the **default** the override falls back to.
- **Suite tools reach the in-run agent on demand, not front-loaded.** The in-run agent can still use every other suite service on the owner's behalf, exactly as before — but it no longer carries every service tool's full definition in its working context from the first moment of every run. It starts with a compact catalog of what the account's services offer and pulls in the specific tools a task actually needs, as it needs them. The observable outcome: runs behave the same, reach the same services, and produce the same kinds of results, while a run's context carries only the catalog plus the tools it actually used — so runs over a fully-populated box stay focused and spend less on tool definitions that were never touched.

**Out of scope (nothing else):** No renumbering of any service and no ownership of the registry table itself (prompts only *reads* it by name); the `registry` module and the repo-root wiring that publishes it (`go.work`) are provided from outside prompts. No new providers beyond the four agentkit already supports. No change to *which* services and tools a run can reach (only to how they are surfaced), and no change to how the run's own sandbox tools work. No changes to the trigger or event model. No new run management capabilities. The `system_prompt` field remains a dedicated top-level field and is not part of the config object. The on-demand loading mechanism itself is the agentkit's (owned and proven in its own project); prompts only adopts it.

## Contractual constants

The four valid provider names, exactly as the agentkit recognises them:

```
anthropic   openai   google   zai
```

The eleven optional config keys, exactly as named:

```
temperature   top_p   max_tokens   effort
thinking_budget   thinking_level   thinking
max_attempts   base_delay   max_delay   max_elapsed
ignore_retry_after   tool_loop_limit   base_url
```

These names are promises — the design must use them verbatim in the MCP tool schema and in stored JSON.

## What we promise (user-facing behavior)

**Creating a prompt** requires provider and model. Config keys are optional:

> "Create a prompt that runs on OpenAI's gpt-4o with temperature 0.3 and a max of 2000 output tokens."

The MCP tool accepts `provider: "openai"`, `model: "gpt-4o"`, and a config object `{"temperature": 0.3, "max_tokens": 2000}`. If the provider or model is unknown, the create call fails immediately with a descriptive error — no prompt is stored.

**Running a prompt** uses exactly the provider, model, and config stored at run-start time. A run triggered by an event and a manual run go through the same path. Provider selection, model selection, and all config values are applied; keys left at default have no effect.

**Updating a prompt's config** replaces the whole config object. An owner can change provider, model, or any config key and the next run will reflect it. The current run, if any, is unaffected.

**Existing prompts** continue to run without any owner action. They are silently migrated to `provider: "anthropic"` and behave identically to before.

**Opening prompts in a browser shows a page, not an error.** A logged-in dashboard user who navigates to the prompts mount root gets a small, on-brand page naming the service and its version; someone without a valid session is turned away. Agents are unaffected — they keep working through MCP.

**Unsupported keys for a model** (e.g. `thinking_budget` on a model that does not support extended thinking) are passed through and silently ignored by the agentkit — the run proceeds normally.

**A run discovers suite tools as it needs them.** The in-run agent starts each run knowing what the account's services offer — a compact, per-service catalog — and brings the specific tools a task needs into play on demand. An event-triggered run still follows its event's identifiers to the right service's tools; a run that touches no suite service carries no suite tool definitions at all. Which services a run may reach is unchanged (all of them except prompts itself, on the owner's behalf), and a service that is down simply doesn't appear, exactly as before.

## Success criteria (outcomes)

- A prompt created with `provider: "openai"` and a valid OpenAI model runs successfully against the OpenAI API.
- A prompt created with an unknown provider name is rejected at create time with a clear error message; no prompt row is created.
- A prompt created with a valid provider but an unrecognised model name is rejected at create time with a clear error message.
- A prompt created with `{"temperature": 0.5, "max_tokens": 500}` in its config runs with those values applied; a subsequent update omitting `temperature` causes the next run to use the agentkit default temperature.
- An existing prompt with no provider set in the database continues to run successfully after the migration, executing against Anthropic as before.
- A prompt running on the `zai` provider with `base_url` set in its config targets that URL.
- A config value not supported by the chosen model (e.g. `thinking_budget` on a non-reasoning model) does not cause the run to fail.
- Editing a prompt's provider, model, or config while a run is in flight does not affect that run; the change is reflected only in the next run.
- As a logged-in dashboard user I open the prompts mount root in a browser and see a styled page showing the service name and its version; without a valid session the page is refused, and the agent-facing MCP surface is unchanged.
- prompts serves on the same loopback port and reaches the same peer `/feed` and dropbox addresses as before, with every one of those addresses obtained from the shared registry by service name; no loopback port number appears as a literal anywhere in prompts's own (non-test) source.
- A run whose task needs a suite service pulls that service's tools in on demand and completes the task, end to end, the same as before.
- A run's working context carries a compact per-service catalog plus only the suite tools the run actually used — never the full definitions of every tool on the box.
- A suite service that is unreachable at run start is absent from the catalog and the run proceeds unaffected, exactly as discovery behaves today.
