# Mock Provider: OpenAI Responses API

## Preconditions

Reset the database and start the mock provider and ikigai pointing at it.
Use `127.0.0.1` (not `localhost`) to avoid IPv6 resolution issues.

```sh
share/ikigai/scripts/reset-database.sh
bin/mock-provider --port 9100 &
OPENAI_BASE_URL=http://127.0.0.1:9100 bin/ikigai --headless &
```

Wait for both to start, then set the model:

```sh
ikigai-ctl send_keys "/model gpt-5-mini\r"
```

## Test: basic chat completion

### Steps
1. Configure mock: `curl -s 127.0.0.1:9100/_mock/expect -d '{"responses": [{"content": "The capital of France is Paris."}]}'`
2. `ikigai-ctl send_keys "What is the capital of France?\r"`
3. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains "The capital of France is Paris."

## Cleanup

```sh
kill %2   # ikigai
kill %1   # mock-provider
```
