# Claude CLI Drop-In Replacement Reference

This document catalogues every way `ralph-runs` interacts with the `claude` CLI binary. A developer who reads this document should have enough information to build a drop-in replacement binary that ralph-runs can invoke without modification.

**Source investigated:** `/home/ai4mgreenly/projects/ralph-runs/` — specifically `scripts/ralph/run` (the agent loop).

---

## 1. CLI Invocation Patterns

There is **exactly one call site** in `scripts/ralph/run` at the `invoke_claude` method (line 453). It is used in two contexts:

### 1.1 Worker Agent Invocation (per iteration)

```ruby
# scripts/ralph/run:454-455
env = thinking_budget.positive? ? "MAX_THINKING_TOKENS=#{thinking_budget} " : ''
cmd = "#{env}ANTHROPIC_API_KEY= claude -p --model #{model} --tools \"#{tools}\" --verbose --input-format stream-json --output-format stream-json --json-schema '#{schema}'"
```

**Concrete example with default settings (sonnet, med reasoning):**

```sh
MAX_THINKING_TOKENS=16384 ANTHROPIC_API_KEY= claude \
  -p \
  --model sonnet \
  --tools "Read,Write,Edit,Glob,Grep,Bash,Skill,StructuredOutput" \
  --verbose \
  --input-format stream-json \
  --output-format stream-json \
  --json-schema '{"type":"object","properties":{"summary":{"type":"string"}},"required":["summary"]}'
```

### 1.2 Summarizer Agent Invocation (every 10 iterations)

```sh
MAX_THINKING_TOKENS=16384 ANTHROPIC_API_KEY= claude \
  -p \
  --model haiku \
  --tools "Read,Glob,Grep,StructuredOutput" \
  --verbose \
  --input-format stream-json \
  --output-format stream-json \
  --json-schema '{"type":"object","properties":{"summary":{"type":"string"}},"required":["summary"]}'
```

The summarizer always uses `haiku` model with `high` reasoning (full thinking budget for haiku = 16,384 tokens).

### 1.3 How the Subprocess Is Started

```ruby
IO.popen(cmd, 'r+') do |io|
  io.puts(initial_message.to_json)
  io.flush
  # ... reads output lines ...
  io.close_write
end
```

- Launched via `IO.popen` with read+write (`'r+'`)
- Initial prompt sent as a single JSON line to stdin, then stdin is flushed
- Additional user messages may be written to stdin during the session (retry logic)
- `io.close_write` is called when conversation is done
- No PTY — pure pipe I/O

---

## 2. Flag and Option Reference

All flags come from `scripts/ralph/run:455`. Only these flags are used:

| Flag | Value | Official Meaning |
|------|-------|-----------------|
| `-p` / `--print` | (boolean) | Non-interactive mode: print response and exit. Workspace trust dialog is skipped. Required for pipe-based usage. |
| `--model MODEL` | `haiku`, `sonnet`, or `opus` | Model alias or full model ID to use for the session. |
| `--tools "..."` | Quoted comma-separated list | Override the set of built-in tools available. `""` disables all tools; `"default"` enables all. |
| `--verbose` | (boolean) | Override verbose mode setting. In stream-json mode this causes tool use/result events to be emitted. |
| `--input-format stream-json` | `stream-json` | Read input as a stream of JSON messages (newline-delimited) from stdin. |
| `--output-format stream-json` | `stream-json` | Emit output as a stream of JSON events (newline-delimited) to stdout. |
| `--json-schema SCHEMA` | JSON string | JSON Schema for structured output validation. The final `result` event includes `structured_output` validated against this schema. |

---

## 3. Environment Variables

### 3.1 Variables Set by ralph-runs Before Invoking claude

| Variable | Value | Purpose |
|----------|-------|---------|
| `ANTHROPIC_API_KEY` | `""` (empty string) | Cleared so that `claude` uses its own stored authentication (login-based auth) rather than an API key. |
| `MAX_THINKING_TOKENS` | Integer (e.g., `16384`, `8192`, `0`) | Controls the extended thinking budget. Only set when `thinking_budget > 0`. When set, claude enables extended thinking up to this many tokens. When not set, extended thinking is disabled. |

**Thinking budget calculation** (`scripts/ralph/run:1131-1135`):

```ruby
MAX_THINKING_BUDGET = {
  'haiku'  => 16_384,
  'sonnet' => 32_768,
  'opus'   => 16_384
}

REASONING_MULTIPLIERS = {
  'none' => 0.0,
  'low'  => 0.25,
  'med'  => 0.5,
  'high' => 1.0
}

# thinking_budget = MAX_THINKING_BUDGET[model] * REASONING_MULTIPLIERS[reasoning]
# e.g., sonnet + med => 32768 * 0.5 = 16384
# e.g., sonnet + none => 32768 * 0.0 = 0 (env var NOT set)
# e.g., haiku + high (summarizer) => 16384 * 1.0 = 16384
```

### 3.2 Variables Passed Through from Host Environment

ralph-runs runs `direnv export json` and merges the result into `ENV` before spawning claude. The clone's `.envrc` may set any arbitrary variables. Of interest for the claude CLI:

| Variable | Source | Effect |
|----------|--------|--------|
| `ANTHROPIC_API_KEY` | Typically `~/.secrets/` | Overridden to `""` by ralph-runs before invocation |
| `PATH` | `.envrc` | Must include `claude` binary location |

### 3.3 Variables NOT Used by ralph-runs but Relevant to claude CLI

| Variable | Purpose |
|----------|---------|
| `ANTHROPIC_BASE_URL` | Override API endpoint (e.g., for proxies) |
| `CLAUDE_CODE_USE_BEDROCK` | Route through AWS Bedrock |
| `CLAUDE_CODE_USE_VERTEX` | Route through GCP Vertex AI |

---

## 4. Input/Output Contract

### 4.1 stdin → claude: Stream-JSON Input Format

Input is sent as **newline-delimited JSON messages** (one JSON object per line).

#### Initial Prompt Message

```json
{
  "type": "user",
  "message": {
    "role": "user",
    "content": [
      {
        "type": "text",
        "text": "<full prompt as rendered by prompt.md.erb>"
      }
    ]
  }
}
```

This is the only message sent at the start. It contains the entire prompt (goal + progress context + AGENTS.md + guidance).

#### Correction / Retry Messages

If claude returns a result without a valid `structured_output` (missing field or schema violation), ralph-runs sends additional user messages over the still-open stdin pipe:

```json
{
  "type": "user",
  "message": {
    "role": "user",
    "content": [
      {
        "type": "text",
        "text": "You must use the StructuredOutput tool. Return: {\"summary\": \"what you accomplished\"}"
      }
    ]
  }
}
```

Up to 3 retry messages may be sent before the session is considered failed.

#### stdin Close

After sending the initial message (or retry messages), `io.close_write` is called when the session is logically complete, signaling EOF to claude.

### 4.2 stdout ← claude: Stream-JSON Output Format

Claude emits **newline-delimited JSON events**. ralph-runs reads them via `io.each_line`.

#### Event Types Consumed

**`assistant` events** — emitted when the model produces a response turn:

```json
{
  "type": "assistant",
  "message": {
    "role": "assistant",
    "content": [
      { "type": "text", "text": "..." },
      {
        "type": "tool_use",
        "id": "tool_use_abc123",
        "name": "Bash",
        "input": { "command": "ls", "description": "List files" }
      }
    ],
    "usage": {
      "input_tokens": 1234,
      "output_tokens": 56,
      "cache_creation_input_tokens": 789,
      "cache_read_input_tokens": 0
    }
  }
}
```

ralph-runs extracts:
- `message.content[].type == "text"` → displayed to user
- `message.content[].type == "tool_use"` → logged (name, input); ID stored for result correlation
- `message.usage` → used to compute context percentage: `(input + cache_create + cache_read) / 200_000 * 100`

**`user` events** — emitted when tool results are returned to the model:

```json
{
  "type": "user",
  "message": {
    "role": "user",
    "content": [
      {
        "type": "tool_result",
        "tool_use_id": "tool_use_abc123",
        "content": "..."
      }
    ]
  },
  "tool_use_result": { "content": "..." }
}
```

ralph-runs correlates `tool_use_id` back to the tool name (stored from `assistant` event).

**`result` events** — the final event in a session:

```json
{
  "type": "result",
  "structured_output": {
    "summary": "What the agent accomplished this step"
  },
  "usage": {
    "input_tokens": 5000,
    "output_tokens": 200,
    "cache_read_input_tokens": 3000,
    "cache_creation_input_tokens": 1000
  }
}
```

ralph-runs:
- Reads `structured_output.summary` as the iteration result
- Reads `usage.*` for cost/token tracking
- `result` event triggers a `break` — no further lines are read from that session

**Unknown events** — any other `type` value is displayed in subdued style and ignored.

### 4.3 Exit Codes

| Exit Code | Meaning |
|-----------|---------|
| `0` | Success — ralph continues with the next iteration |
| Non-zero | Failure — ralph-runs treats this as a failed attempt and may retry |
| `130` | Interrupted (SIGINT) — ralph exits cleanly |

### 4.4 Retry Loop Within a Session

After receiving a `result` event, if `structured_output` is missing or invalid, ralph-runs:
1. Sends a correction user message over stdin (session stays open)
2. Calls `read_until_result` again to get the next `result` event
3. Repeats up to 3 times (`max_retries = 3`)
4. After 3 failed retries, synthesizes an error result and closes the session

---

## 5. Tools Used

### 5.1 Worker Agent Tool Set

```
Read,Write,Edit,Glob,Grep,Bash,Skill,StructuredOutput
```

These are passed as the `--tools` argument. These are Claude Code's built-in tool names.

| Tool | Purpose in ralph |
|------|-----------------|
| `Read` | Read file contents |
| `Write` | Write/create files |
| `Edit` | Edit existing files |
| `Glob` | File pattern matching |
| `Grep` | Content search |
| `Bash` | Execute shell commands |
| `Skill` | Load skill documents (Claude Code slash command system) |
| `StructuredOutput` | Emit the `{"summary": "..."}` result required by `--json-schema` |

### 5.2 Summarizer Agent Tool Set

```
Read,Glob,Grep,StructuredOutput
```

The summarizer only needs read access — no write or execute capabilities.

---

## 6. Structured Output Schema

Both the worker and summarizer use the same schema:

```json
{
  "type": "object",
  "properties": {
    "summary": { "type": "string" }
  },
  "required": ["summary"]
}
```

This is passed as the `--json-schema` argument. The `result` event from claude must include `structured_output.summary` as a non-null string, or ralph will send a correction message.

**Special value:** When `summary == "DONE"`, ralph terminates the goal loop and marks the goal complete.

---

## 7. Configuration Files and Directories

### 7.1 CLAUDE.md / AGENTS.md

- ralph-runs reads the **target repo's** `AGENTS.md` file and injects it into the prompt template (`load_agents_md` method)
- If `AGENTS.md` does not exist, a fallback instruction is used telling the agent to explore the repo
- `CLAUDE.md` in the target repo is read by claude itself (not by ralph) as project-level configuration

### 7.2 .claude/ Directory (in the Target Repo)

Claude Code automatically reads and applies:
- `.claude/settings.json` — project-level settings
- `.claude/settings.local.json` — local overrides (gitignored)
- `.claude/commands/` — slash command definitions
- `.claude/library/` — skill documents
- `.claude/skillsets/` — composite skill bundles

These are loaded by claude without ralph-runs doing anything explicit — they're part of claude's project configuration discovery.

### 7.3 .ralph/ Directory (in the Target Repo)

This is ralph-runs' own convention (not a Claude Code concept):
- `.ralph/check` — post-merge check script run by the orchestrator
- ralph-runs instructs claude (via the prompt) never to modify `.ralph/`

---

## 8. Session and State Behavior

### 8.1 No Session Persistence

ralph-runs does **not** use `--continue`, `--resume`, or `--session-id`. Each `invoke_claude` call starts a **fresh session** with no history from previous calls.

All conversation history is managed externally by ralph-runs via the prompt template, which injects:
- A summary of older iterations (generated by the summarizer agent)
- The most recent N iteration progress messages

### 8.2 Working Directory

`claude` is invoked via `IO.popen(cmd, 'r+')` with no explicit `chdir`. However, ralph-runs changes into the clone directory before spawning via the `source_envrc` call and the overall execution context. The `ralph-runs` orchestrator uses `chdir: clone_dir` when spawning `ralph` (the agent loop), so by the time `invoke_claude` is called, `cwd` is the clone directory.

### 8.3 Permissions

No explicit permission flags are passed. Since `-p --input-format stream-json` is non-interactive, permission prompts cannot be answered interactively. The target repo's `.claude/settings.json` must pre-approve tools (e.g., `"dangerouslySkipPermissions": true`) or claude must have global bypass configured.

---

## 9. Model Aliases

ralph-runs uses short model aliases (`haiku`, `sonnet`, `opus`) passed to `--model`. Claude Code maps these to the latest versioned model IDs:

| Alias | Maps To (approximately) |
|-------|------------------------|
| `haiku` | `claude-haiku-4-5-20251001` (or latest Haiku) |
| `sonnet` | `claude-sonnet-4-6` (or latest Sonnet) |
| `opus` | `claude-opus-4-6` (or latest Opus) |

A drop-in replacement must accept these short aliases.

---

## 10. Extended Thinking

When `MAX_THINKING_TOKENS` is set in the environment, claude enables extended thinking (also called "extended reasoning" or "budget tokens"). The model may emit thinking blocks in its response content:

```json
{
  "type": "thinking",
  "thinking": "Let me analyze this problem..."
}
```

ralph-runs does not explicitly handle thinking blocks — it only extracts `type == "text"` and `type == "tool_use"` content blocks. Thinking blocks are silently ignored in the output processing.

A drop-in replacement must:
1. Accept `MAX_THINKING_TOKENS` as an env var
2. When set and > 0, enable extended thinking with that budget
3. Emit thinking blocks in the stream (they are filtered out by ralph-runs, but must be valid JSON)

---

## 11. Token Usage Tracking

ralph-runs parses the `usage` field from `result` events for cost tracking. Required fields: `input_tokens`, `output_tokens`, `cache_read_input_tokens`, `cache_creation_input_tokens`. Also reads `usage` from `assistant` events to compute context percentage: `(input + cache_create + cache_read) / 200_000 * 100`.

---

## 12. Summary of Interface Requirements

To build a drop-in replacement for `claude` that satisfies ralph-runs:

### Must Implement

1. **Flag**: `-p` — non-interactive mode (required for all ralph invocations)
2. **Flag**: `--model haiku|sonnet|opus` — model selection via alias
3. **Flag**: `--tools "Tool1,Tool2,..."` — restrict available built-in tools
4. **Flag**: `--verbose` — emit tool use/result events in output stream
5. **Flag**: `--input-format stream-json` — read newline-delimited JSON from stdin
6. **Flag**: `--output-format stream-json` — write newline-delimited JSON to stdout
7. **Flag**: `--json-schema '...'` — validate final output against JSON schema and include in `result` event
8. **Env var**: `MAX_THINKING_TOKENS=N` — enable extended thinking with token budget
9. **Env var**: `ANTHROPIC_API_KEY=""` — handle empty/cleared API key (use stored auth)
10. **Input message format**: `{"type":"user","message":{"role":"user","content":[{"type":"text","text":"..."}]}}`
11. **Output event types**: `assistant`, `user`, `result` (and optionally others)
12. **Result event**: `{"type":"result","structured_output":{...},"usage":{...}}`
13. **Usage fields**: `input_tokens`, `output_tokens`, `cache_read_input_tokens`, `cache_creation_input_tokens`
14. **Tool names**: `Read`, `Write`, `Edit`, `Glob`, `Grep`, `Bash`, `Skill`, `StructuredOutput`
15. **Exit codes**: 0 for success, non-zero for failure
16. **Multi-turn in same session**: Accept additional user messages on stdin after initial prompt (for retry logic)

### Not Required (for ralph-runs compatibility)

- Interactive mode (no TTY required)
- Session persistence / resume
- MCP servers
- Permission prompts
- Any flag not listed in Section 2 "Flags Used"
