---
name: e2e-testing
description: JSON-based end-to-end test format for running tests against mock or live providers
---

# End-to-End Testing

End-to-end tests verify ikigai behavior through its control socket (`ikigai-ctl`). Each test is a self-contained JSON file. Tests run sequentially — they share a single ikigai instance.

## Test File Location

Tests live in `tests/e2e/`. Run order is defined by `tests/e2e/index.json` — a JSON array of test filenames in execution order. When asked to "run the e2e tests", read `index.json` and execute each listed test file sequentially.

## Execution Modes

| Mode | Backend | Steps | Assertions |
|------|---------|-------|------------|
| **mock** | `bin/mock-provider` | all steps including `mock_expect` | `assert` + `assert_mock` |
| **live** | real provider (Anthropic, OpenAI, Google) | `mock_expect` steps skipped | `assert` only |

Every test always includes `mock_expect` steps and `assert_mock` assertions — tests are written once and run in either mode. In live mode, `mock_expect` steps are skipped and `assert_mock` is not evaluated.

## JSON Schema

```json
{
  "name": "human-readable test name",
  "steps": [
    ...
  ],
  "assert": [
    ...
  ],
  "assert_mock": [
    ...
  ]
}
```

- **`name`** — describes what the test verifies
- **`steps`** — ordered list of actions to execute
- **`assert`** — assertions checked in ALL modes
- **`assert_mock`** — assertions checked only in mock mode

## Step Types

### `send_keys`

Send keystrokes to ikigai via `ikigai-ctl send_keys`.

```json
{"send_keys": "/model gpt-5-mini\\r"}
```

Include `\\r` at the end of the string to submit. Use `ikigai-ctl send_keys` escaping conventions.

### `read_framebuffer`

Capture the current screen contents via `ikigai-ctl read_framebuffer`. The captured state is what assertions run against.

```json
{"read_framebuffer": true}
```

Always `read_framebuffer` before asserting. Each `read_framebuffer` replaces the previous capture.

### `wait`

Pause for N seconds. Use after `send_keys` to allow UI updates or LLM responses.

```json
{"wait": 0.5}
```

- After UI commands (`/model`, `/clear`): 0.5 seconds
- After sending a prompt to the LLM: 3-5 seconds

### `wait_idle`

Wait until the current agent becomes idle (ready for input), or until the timeout elapses.
Calls `ikigai-ctl wait_idle <timeout_ms>`.

```json
{"wait_idle": 10000}
```

- Value is `timeout_ms` (integer milliseconds)
- Exit code 0 = agent became idle; exit code 1 = timed out (report FAIL)
- Use instead of `{"wait": N}` after sending prompts to the LLM
- Keep `{"wait": 0.5}` for UI-only commands (`/clear`, `/model`) that don't trigger LLM

### `mock_expect`

Configure the mock provider's next response queue. Sends a POST to `/_mock/expect`. **Skipped in live mode.**

```json
{"mock_expect": {"responses": [{"content": "The capital of France is Paris."}]}}
```

The object is sent as the JSON body to `/_mock/expect`. The `responses` array is a FIFO queue — each LLM request pops the next entry. Entries contain either `content` (text) or `tool_calls` (array), never both. Must appear before the `send_keys` that triggers the LLM call.

## Assertion Types

Assertions run against the most recent `read_framebuffer` capture. The framebuffer response contains a `lines` array; each line has `spans` with `text` fields. Concatenate span texts per row to reconstruct screen content.

### `contains`

At least one row contains the given substring.

```json
{"contains": "gpt-5-mini"}
```

### `not_contains`

No row contains the given substring.

```json
{"not_contains": "error"}
```

### `line_prefix`

At least one row starts with the given prefix (after trimming leading whitespace).

```json
{"line_prefix": "●"}
```

## Running Tests (LLM-Driven)

When asked to run tests, execute this procedure for each test file:

1. Read the JSON file
2. Determine mode (mock or live) from context — mock if ikigai is connected to `mock-provider`, live otherwise
3. Execute each step in order:
   - `send_keys`: run `ikigai-ctl send_keys "<value>"`
   - `wait`: `sleep N`
   - `wait_idle`: run `ikigai-ctl wait_idle <value>`, fail if exit code is 1
   - `read_framebuffer`: run `ikigai-ctl read_framebuffer`, store result
   - `mock_expect`: in mock mode, `curl -s 127.0.0.1:<port>/_mock/expect -d '<json>'`; in live mode, skip
4. After all steps, evaluate assertions:
   - Always evaluate `assert`
   - In mock mode, also evaluate `assert_mock`
5. Report **PASS** or **FAIL** with evidence (cite relevant framebuffer rows)

## Running Large Live Test Batches with Sub-Agents

When asked to run a **large number of live e2e tests** (more than 20), divide the work across sub-agents running **serially** (one after the next, never in parallel):

1. Read `tests/e2e/index.json` to get the full ordered list of test files
2. Partition the list into chunks of at most **20 tests each**
3. Launch one sub-agent per chunk, **sequentially** — wait for each to complete before launching the next
4. Each sub-agent receives: its assigned test files (in order), the ikigai socket path, and instructions to run in live mode. **Do not pre-read the test files yourself** — pass only the filenames and let the sub-agent read them.
5. Collect pass/fail results from each sub-agent and summarize at the end

**Why serially:** Tests share a single ikigai instance. Running sub-agents concurrently would interleave keystrokes and framebuffer reads across tests, corrupting results.

**Why chunked:** Live tests are slow (LLM round-trips). Chunking prevents any single agent from exhausting its context window mid-run.

## Key Rules

- **Never start ikigai** — the user manages the instance
- **Never use a script to run live mode tests** — live mode tests exist precisely because they are not run by code. Execute each step individually using tool calls so every response is visible and any crash or unexpected behavior can be observed and explained.
- **One test file = one test** — self-contained, no dependencies on other test files
- **Steps execute in order** — sequential, never parallel
- **Always read_framebuffer before asserting** — assertions reference the last capture
- **Never chain anything after wait_idle** — `wait_idle` must always be the last command in a Bash tool call. If it succeeds (exit code 0) and a subsequent command fails, the overall exit code 1 is indistinguishable from `wait_idle` timing out. Run `read_framebuffer` in a separate Bash tool call after `wait_idle` completes.

## Example: UI-only test

```json
{
  "name": "no model indicator on fresh start",
  "steps": [
    {"read_framebuffer": true}
  ],
  "assert": [
    {"contains": "(no model)"}
  ]
}
```

## Example: mock provider test

```json
{
  "name": "basic chat completion via mock provider",
  "steps": [
    {"mock_expect": {"responses": [{"content": "The capital of France is Paris."}]}},
    {"send_keys": "What is the capital of France?\\r"},
    {"wait": 3},
    {"read_framebuffer": true}
  ],
  "assert": [
    {"line_prefix": "●"}
  ],
  "assert_mock": [
    {"contains": "The capital of France is Paris."}
  ]
}
```

## Example: model switching test

```json
{
  "name": "set model to gpt-5-mini with low reasoning",
  "steps": [
    {"send_keys": "/clear\\r"},
    {"wait": 0.5},
    {"send_keys": "/model gpt-5-mini/low\\r"},
    {"wait": 0.5},
    {"mock_expect": {"responses": [{"content": "Mock response from gpt-5-mini."}]}},
    {"send_keys": "Hello\\r"},
    {"wait_idle": 10000},
    {"read_framebuffer": true}
  ],
  "assert": [
    {"contains": "gpt-5-mini/low"},
    {"contains": "low effort"},
    {"line_prefix": "●"}
  ],
  "assert_mock": [
    {"contains": "Mock response from gpt-5-mini."}
  ]
}
```
