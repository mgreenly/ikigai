---
name: e2e-testing
description: JSON-based end-to-end test format, runner, and mock provider
---

# End-to-End Testing

End-to-end tests verify ikigai behavior through its control socket. For `ikigai-ctl` usage, see `/load ikigai-ctl`. For general headless interaction, see `/load headless`.

## Test Files

Tests live in `tests/e2e/` as self-contained JSON files. Run order is defined by `tests/e2e/index.json` — a JSON array of test filenames in execution order.

## Execution Modes

| Mode | Backend | Steps | Assertions |
|------|---------|-------|------------|
| **mock** | `bin/mock-provider` | all steps including `mock_expect` | `assert` + `assert_mock` |
| **live** | real provider (Anthropic, OpenAI, Google) | `mock_expect` steps skipped | `assert` only |

Tests are written once and run in either mode. In live mode, `mock_expect` steps are skipped and `assert_mock` is not evaluated.

## JSON Schema

```json
{
  "name": "human-readable test name",
  "steps": [ ... ],
  "assert": [ ... ],
  "assert_mock": [ ... ]
}
```

- **`name`** — describes what the test verifies
- **`steps`** — ordered list of actions to execute
- **`assert`** — assertions checked in ALL modes
- **`assert_mock`** — assertions checked only in mock mode

## Step Types

### `send_keys`

```json
{"send_keys": "/model gpt-5-mini\\r"}
```

Include `\\r` to submit. See `/load ikigai-ctl` for escaping conventions.

### `read_framebuffer`

```json
{"read_framebuffer": true}
```

Always `read_framebuffer` before asserting. Each capture replaces the previous one.

### `wait`

```json
{"wait": 0.5}
```

- After UI commands (`/model`, `/clear`): 0.5 seconds
- After sending a prompt to the LLM: 3-5 seconds (prefer `wait_idle`)

### `wait_idle`

Wait until the agent becomes idle or timeout elapses.

```json
{"wait_idle": 10000}
```

- Value is `timeout_ms` (integer milliseconds)
- Exit code 0 = idle; exit code 1 = timed out (report FAIL)
- Use instead of `wait` after sending prompts to the LLM

### `mock_expect`

Configure the mock provider's response queue. **Skipped in live mode.**

```json
{"mock_expect": {"responses": [{"content": "The capital of France is Paris."}]}}
```

The `responses` array is a FIFO queue — each LLM request pops the next entry. Entries contain either `content` (text) or `tool_calls` (array), never both. Must appear before the `send_keys` that triggers the LLM call.

## Assertion Types

Assertions run against the most recent `read_framebuffer` capture.

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

## Running Tests

**Direct execution, one tool call per step.** Never use scripts or programmatic wrappers when the user asks you to run e2e tests. The scripted runner (`tests/e2e/runner`) exists for CI — when the user asks you to run tests, they want direct execution so they can observe every response.

### Procedure per test file:

1. Read the JSON file
2. Determine mode — mock if ikigai is connected to `mock-provider`, live otherwise
3. Execute each step in order, one tool call per step:
   - `send_keys`: run `ikigai-ctl send_keys "<value>"`
   - `wait`: `sleep N`
   - `wait_idle`: run `ikigai-ctl wait_idle <value>`, fail if exit code is 1
   - `read_framebuffer`: run `ikigai-ctl read_framebuffer`, store result
   - `mock_expect`: in mock mode, `curl -s 127.0.0.1:<port>/_mock/expect -d '<json>'`; in live mode, skip
4. Evaluate assertions (`assert` always, `assert_mock` in mock mode only)
5. Report **PASS** or **FAIL** with evidence (cite relevant framebuffer rows)

### Large batches (20+ tests)

Divide into chunks of 20, run sub-agents **serially** (shared instance — never parallel). Each sub-agent receives filenames and the full contents of this skill. Don't pre-read test files yourself.

## Key Rules

- **Never start ikigai** — the user manages the instance
- **Never use the runner script** — direct execution only
- **One test file = one test** — self-contained, no dependencies
- **Steps execute in order** — sequential, never parallel
- **Always read_framebuffer before asserting**
- **Never chain after wait_idle** — run `read_framebuffer` in a separate tool call

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
