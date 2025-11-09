# OpenAI Client (`openai.c/h`)

[← Back to Phase 1 Details](phase-1-details.md)

Handles HTTP streaming requests to OpenAI Chat Completions API. Uses libcurl for HTTP client and implements SSE (Server-Sent Events) parsing.

**IMPORTANT UPDATE:** Must use curl multi interface instead of `curl_easy_perform()` to support immediate abort on shutdown.

## API

```c
// Stream callback - called for each parsed JSON chunk from OpenAI
// json_chunk is a complete JSON object string (without "data: " prefix)
// Will NOT be called for [DONE] marker or error responses
typedef void (*ik_openai_stream_cb_t)(const char *json_chunk, void *user_data);

// Make streaming request to OpenAI Chat Completions API
ik_result_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                     const ik_cfg_t *config,
                                     json_t *req_payload,
                                     ik_openai_stream_cb_t callback,
                                     void *cb_data,
                                     volatile sig_atomic_t *abort_flag);
```

**Parameters:**
- `ctx` - talloc context for error allocation only
- `config` - server config (contains `openai_api_key`)
- `req_payload` - JSON object with OpenAI request (borrowed, caller retains ownership)
- `callback` - function called for each SSE chunk containing JSON
- `cb_data` - passed through to callback (typically `ik_task_t*` from handler module)
- `abort_flag` - pointer to abort flag (from task->abort_flag which points to conn->abort_flag)

**Returns:**
- `OK(NULL)` - stream completed successfully (received `[DONE]`)
- `ERR(..., NETWORK, ...)` - HTTP errors (connection failed, non-2xx status)
- `ERR(..., AUTH, ...)` - authentication failure (401, invalid API key)
- `ERR(..., PARSE, ...)` - malformed SSE or JSON from OpenAI
- `ERR(..., OOM, ...)` - memory allocation failure

## HTTP Request Details

**Endpoint:** `https://api.openai.com/v1/chat/completions`

**Headers:**
```
Authorization: Bearer <config->openai_api_key>
Content-Type: application/json
```

**Request Body:**
Serialized JSON from `req_payload` parameter. The WebSocket handler passes through the client's payload, which should have:
```json
{
  "model": "gpt-4o-mini",
  "messages": [...],
  "stream": true
}
```

The server does NOT modify the payload - it forwards exactly what the client sent (except for injecting credentials).

## SSE Stream Parsing

OpenAI responds with `Content-Type: text/event-stream`:

```
data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"content":" there"},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]

```

**SSE Format:**
- Each message starts with `data: `
- Message ends with `\n\n` (double newline)
- Final message is `data: [DONE]\n\n`

**Parsing Algorithm:**

1. libcurl write callback accumulates data in buffer
2. Scan buffer for complete SSE messages (delimited by `\n\n`)
3. For each complete message:
   - Strip `data: ` prefix
   - If content is `[DONE]`, set done flag and stop
   - Validate JSON (use `json_loads` to check)
   - Call `callback(json_chunk, cb_data)`
   - Remove processed message from buffer
4. Keep partial message in buffer for next callback
5. Continue until `[DONE]` or stream ends

## SSE Parser State

```c
typedef struct {
  TALLOC_CTX *ctx;                 // For buffer allocations
  char *buffer;                    // Accumulated SSE data (talloc)
  size_t buffer_len;               // Bytes currently in buffer
  size_t buffer_capacity;          // Allocated buffer size
  ik_openai_stream_cb_t callback;
  void *cb_data;
  bool done;                       // Received [DONE] marker
  ik_result_t error;               // Stores error if parsing fails
} sse_parser_t;
```

**Buffer Management:**
- Initial size: 4096 bytes
- Growth strategy: double capacity when full (`capacity * 2`)
- Maximum size: 1MB (1,048,576 bytes) - prevents unbounded growth from malicious/buggy responses
- If doubling would exceed 1MB, reject growth and abort transfer (buffer capacity never exceeds 1MB)
- This means SSE responses must fit in under 1MB of accumulated data before processing
- Automatic cleanup via talloc (allocated on temporary context)

## libcurl Write Callback

```c
size_t sse_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  sse_parser_t *parser = (sse_parser_t *)userdata;
  size_t bytes = nmemb;  // size is always 1

  // Grow buffer if needed
  if (parser->buffer_len + bytes > parser->buffer_capacity) {
    size_t new_cap = parser->buffer_capacity * 2;

    // Check 1MB limit - ensure new capacity doesn't exceed limit
    if (new_cap > 1048576) {
      parser->error = ERR(parser->ctx, PARSE, "SSE buffer growth would exceed 1MB limit");
      return 0;  // Abort transfer
    }

    char *new_buf = talloc_realloc(parser->ctx, parser->buffer, char, new_cap);
    if (!new_buf) {
      parser->error = ERR(parser->ctx, OOM, "Failed to grow SSE buffer");
      return 0;  // Abort transfer
    }
    parser->buffer = new_buf;
    parser->buffer_capacity = new_cap;
  }

  // Append data to buffer
  memcpy(parser->buffer + parser->buffer_len, ptr, bytes);
  parser->buffer_len += bytes;

  // Process complete SSE messages
  while (!parser->done) {
    char *delim = strstr(parser->buffer, "\n\n");
    if (!delim) break;  // No complete message yet

    size_t msg_len = delim - parser->buffer;

    // Extract message (skip "data: " prefix - 6 chars)
    if (msg_len < 6 || memcmp(parser->buffer, "data: ", 6) != 0) {
      parser->error = ERR(parser->ctx, PARSE, "Invalid SSE format");
      return 0;
    }

    char *content = parser->buffer + 6;
    size_t content_len = msg_len - 6;

    // Check for [DONE]
    if (content_len == 6 && memcmp(content, "[DONE]", 6) == 0) {
      parser->done = true;
      break;
    }

    // Validate JSON and call callback
    // (null-terminate temporarily, validate, restore, call callback)
    // Implementation details omitted for brevity

    // Remove processed message from buffer (shift remaining data)
    memmove(parser->buffer, delim + 2, parser->buffer_len - (msg_len + 2));
    parser->buffer_len -= (msg_len + 2);
  }

  return bytes;
}
```

## Error Handling

**OpenAI API Errors:**

When OpenAI returns an error (e.g., invalid API key, rate limit), the response is JSON instead of SSE:

```json
{
  "error": {
    "message": "Incorrect API key provided",
    "type": "invalid_request_error",
    "code": "invalid_api_key"
  }
}
```

**Detection:**
- Check HTTP status code (401 = auth error, 429 = rate limit, etc.)
- For non-2xx status, read response body
- Parse as JSON, extract error message
- Return appropriate error code:
  - 401 → `IK_ERR_AUTH`
  - 429, 500, 503 → `IK_ERR_NETWORK`
  - Other → `IK_ERR_NETWORK`

**Do NOT call the stream callback for error responses.** Return an error result and let the WebSocket handler construct an error message envelope.

**Parsing Errors:**

If SSE parsing fails:
- Invalid SSE format (missing `data: ` prefix) → `IK_ERR_PARSE`
- Invalid JSON in chunk → `IK_ERR_PARSE`
- Buffer growth failure → `IK_ERR_OOM`

## Abort Support

**Requirement:** Must abort in-flight requests immediately when abort flag is set (client disconnect or server shutdown).

**Implementation:** Use curl multi interface instead of blocking `curl_easy_perform()`. The abort flag parameter can be either:
- `&conn->abort_flag` - set by close callback on client disconnect
- `&g_httpd_shutdown` - set by signal handler on server shutdown

Handler module passes `&conn->abort_flag` for proper per-connection abort semantics.

**Event loop with abort check:**
```c
CURLM *multi = curl_multi_init();
curl_multi_add_handle(multi, curl);

int running;
while (running && !*abort_flag) {
    curl_multi_perform(multi, &running);

    // Poll with short timeout for responsive abort detection
    // Actual value chosen to meet shutdown requirements
    curl_multi_poll(multi, NULL, 0, 50, NULL);
}

if (*abort_flag) {
    // Abort: remove handle immediately
    curl_multi_remove_handle(multi, curl);
    // Return OK(NULL) - abort is not an error, just early termination
}
```

## Implementation Flow (curl multi)

```c
ik_result_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                     const ik_cfg_t *config,
                                     json_t *req_payload,
                                     ik_openai_stream_cb_t callback,
                                     void *cb_data,
                                     volatile sig_atomic_t *shutdown_flag) {
  TALLOC_CTX *tmp_ctx = talloc_new(ctx);
  if (!tmp_ctx) {
    return ERR(ctx, OOM, "Failed to create temporary context");
  }

  // 1. Serialize request payload to JSON string
  char *json_body = json_dumps(req_payload, JSON_COMPACT);
  if (!json_body) {
    talloc_free(tmp_ctx);
    return ERR(ctx, OOM, "Failed to serialize request");
  }

  // 2. Initialize SSE parser
  sse_parser_t *parser = talloc_zero(tmp_ctx, sse_parser_t);
  parser->ctx = tmp_ctx;
  parser->buffer = talloc_array(tmp_ctx, char, 4096);
  parser->buffer_capacity = 4096;
  parser->callback = callback;
  parser->cb_data = cb_data;
  parser->error = OK(NULL);

  // 3. Setup libcurl easy handle
  CURL *curl = curl_easy_init();
  if (!curl) {
    free(json_body);
    talloc_free(tmp_ctx);
    return ERR(ctx, NETWORK, "Failed to initialize curl");
  }

  // Build Authorization header
  char *auth_header = talloc_asprintf(tmp_ctx, "Authorization: Bearer %s",
                                      config->openai_api_key);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, auth_header);
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, parser);

  // 4. Create multi handle and add easy handle
  CURLM *multi = curl_multi_init();
  if (!multi) {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(json_body);
    talloc_free(tmp_ctx);
    return ERR(ctx, NETWORK, "Failed to initialize curl multi");
  }

  curl_multi_add_handle(multi, curl);

  // 5. Perform request with shutdown polling
  int running = 1;
  bool aborted = false;

  while (running && !*shutdown_flag) {
    curl_multi_perform(multi, &running);

    // Poll with 50ms timeout for quick abort detection
    curl_multi_poll(multi, NULL, 0, 50, NULL);
  }

  if (*shutdown_flag) {
    aborted = true;
  }

  // 6. Check HTTP status
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  // 7. Cleanup
  curl_multi_remove_handle(multi, curl);
  curl_multi_cleanup(multi);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(json_body);

  // 8. Handle errors
  ik_result_t result = OK(NULL);

  if (aborted) {
    result = OK(NULL);  // Shutdown is not an error, just early termination
  } else if (parser->error.is_err) {
    result = parser->error;  // Parsing error occurred
  } else if (http_code == 401) {
    result = ERR(ctx, AUTH, "OpenAI authentication failed (invalid API key)");
  } else if (http_code != 200) {
    result = ERR(ctx, NETWORK, "OpenAI API error (HTTP %ld)", http_code);
  }

  talloc_free(tmp_ctx);
  return result;
}
```

## Memory Management

- **req_payload**: Borrowed (caller owns, we don't incref/decref)
- **shutdown_flag**: Borrowed pointer (owned by httpd module)
- **Temporary context**: Created with `talloc_new(ctx)`, freed before return
- **SSE parser**: Allocated on temporary context, auto-freed
- **JSON serialization**: Uses jansson's malloc, must call `free(json_body)`
- **curl easy handle**: Created with `curl_easy_init()`, freed with `curl_easy_cleanup()`
- **curl multi handle**: Created with `curl_multi_init()`, freed with `curl_multi_cleanup()`
- **curl headers**: Allocated by libcurl, freed with `curl_slist_free_all()`
- **Error results**: Allocated on `ctx` parameter (only if returning error)

## Test Coverage

`tests/unit/openai_test.c`:
- Successful streaming request (use real API during development)
- Parse multiple SSE chunks correctly
- Handle [DONE] terminator
- HTTP error (401 authentication)
- HTTP error (500 server error)
- Malformed SSE (missing `data:` prefix)
- Malformed JSON in chunk
- Buffer growth (send large chunks that exceed 4KB)
- OOM during buffer growth (inject allocation failure)
- Connection failure (network error)
- **Shutdown abort**: Set shutdown flag during request, verify abort within shutdown requirement
- **Shutdown abort**: Verify callback not called after shutdown flag set

**Note:** Tests will call real OpenAI API during development. Capture responses and create mocked tests for release builds.
