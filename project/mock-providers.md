# Mock AI Providers

A single mock HTTP server that replaces all AI provider APIs during manual testing, enabling deterministic and repeatable test scenarios.

## Problem

Manual tests verify ikigai end-to-end via `ikigai-ctl`, but real LLM responses are unpredictable. Tests can only assert "a response appeared" rather than verifying specific behavior. The mock provider returns pre-scripted responses so tests can assert on exact content.

## Architecture

One binary serves all providers:

```
apps/mock-provider/     # Single binary, multiple provider routes
```

The mock exposes two interfaces:

- **Control interface** (`/_mock/*`) — configure response scenarios before each test
- **API interfaces** — provider-specific routes that serve pre-scripted responses to ikigai
  - `/v1/chat/completions` (OpenAI)
  - `/v1/messages` (Anthropic)
  - `/v1/models/*/generateContent` (Gemini)

The control payload is provider-agnostic — logical tool calls and text. Each API route uses a provider-specific serializer to render the response in the correct SSE wire format. The FIFO queue and HTTP server are shared.

ikigai connects to the mock instead of real APIs via environment variable overrides pointing all providers at the same host and port.

## Mock Server Behavior

### Control Endpoint

`POST /_mock/expect` loads an ordered queue of responses for the next conversation turn:

```json
{
  "responses": [
    {
      "tool_calls": [
        {"name": "read_file", "arguments": {"path": "foo.txt"}},
        {"name": "list_dir", "arguments": {"path": "."}},
        {"name": "read_file", "arguments": {"path": "bar.txt"}}
      ]
    },
    {
      "tool_calls": [
        {"name": "write_file", "arguments": {"path": "out.txt", "content": "hello"}}
      ]
    },
    {
      "content": "Done. I read the files and wrote the output."
    }
  ]
}
```

`POST /_mock/reset` clears the response queue.

### API Endpoints

Each POST to a provider's API endpoint pops the next response off the queue, serializes it into that provider's SSE wire format, and streams it back.

The mock does not inspect the incoming request. It serves responses strictly in FIFO order. The test author is responsible for ensuring the sequence is correct.

If the queue is empty when a request arrives, the mock returns an HTTP error — this indicates a test bug (more requests than expected).

### SSE Streaming

Each provider has its own serializer that converts the logical response into the correct SSE format. A single streamed response contains either:

- A batch of tool calls (one or more)
- A text completion

Never both in the same response. The mock handles chunking the content into SSE `data:` lines and terminating appropriately for the provider's format.

## Conversation Flow

A typical multi-turn tool-use scenario:

```
Test runner:  POST /_mock/expect  (load 3 responses)
ikigai:       POST /v1/chat/completions  (user message)
mock:         streams response 1 — tool_call_1, tool_call_2, tool_call_3
ikigai:       executes tools locally
ikigai:       POST /v1/chat/completions  (tool_result_1, tool_result_2, tool_result_3)
mock:         streams response 2 — tool_call_4
ikigai:       executes tool locally
ikigai:       POST /v1/chat/completions  (tool_result_4)
mock:         streams response 3 — final text
ikigai:       renders text with "●" prefix
Test runner:  read_framebuffer, assert expected content
```

## Environment Variable Overrides

Provider modules in ikigai check for base URL overrides:

| Provider | Environment Variable | Default |
|----------|---------------------|---------|
| OpenAI | `OPENAI_BASE_URL` | `https://api.openai.com` |
| Anthropic | `ANTHROPIC_BASE_URL` | `https://api.anthropic.com` |
| Gemini | `GEMINI_BASE_URL` | `https://generativelanguage.googleapis.com` |

When set, the provider module uses the override instead of the real API. All three can point at the same mock server. No other code changes — auth headers are still sent but the mock ignores them.

## Manual Test Integration

Test files in `tests/manual/` gain a setup step to configure the mock before each interaction:

```markdown
### Steps
1. Configure mock: `curl -s localhost:9100/_mock/expect -d '{"responses": [{"content": "Hello! How can I help?"}]}'`
2. `ikigai-ctl send_keys "Hello\r"`
3. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains "Hello! How can I help?"
```

## Build

The mock-provider binary lives in `apps/mock-provider/` and is built as part of the default `make all` target, following the same Makefile pattern as `apps/ikigai/`.

## Implementation Plan

### Phase 1: Mock server with OpenAI support

1. HTTP server that listens on a port and routes requests
2. `/_mock/expect` — accepts the JSON payload, stores the response queue
3. `/_mock/reset` — clears the queue
4. `/v1/chat/completions` — pops next response, streams it as OpenAI SSE

Each step is independently testable via `curl` before ikigai is involved.

5. Wire into Makefile as `apps/mock-provider/`

### Phase 2: Environment variable overrides

6. OpenAI provider module reads `OPENAI_BASE_URL` and uses it when set

### Phase 3: End-to-end validation

7. Start mock-provider, start ikigai with `OPENAI_BASE_URL` pointing at mock, run manual tests, verify deterministic results

### Future phases

- Anthropic serializer (`/v1/messages`)
- Gemini serializer (`/v1/models/*/generateContent`)
- Additional provider env var overrides
