# v1.0 LLM Integration Design

HTTP client design, streaming response handling, and event loop integration for LLM communication.

## HTTP Client Module

### Module Interface

```c
typedef struct ik_llm_client_t ik_llm_client_t;
typedef void (*ik_chunk_callback_t)(void *user_data, const char *chunk);

// Initialize LLM client
res_t ik_llm_init(void *parent, const char *api_key, ik_llm_client_t **out);

// Send streaming request
res_t ik_llm_send_streaming(ik_llm_client_t *client, ik_message_t **messages,
                            size_t message_count, ik_chunk_callback_t on_chunk, void *user_data);

// Non-blocking operations
res_t ik_llm_poll_chunk(ik_llm_client_t *client, char **chunk_out);
bool ik_llm_is_complete(ik_llm_client_t *client);
res_t ik_llm_cancel_stream(ik_llm_client_t *client);

// Get socket FD for select()
int ik_llm_get_socket_fd(ik_llm_client_t *client);
```

### Initial Implementation

**OpenAI-specific client using libcurl streaming**
- Later abstract to multi-provider interface
- Dependencies: libcurl (HTTP streaming), yyjson (JSON parsing)

---

## Streaming Response Handling

### OpenAI SSE Format

```
data: {"id":"chatcmpl-123","choices":[{"delta":{"content":"Hello"},"index":0}]}

data: {"id":"chatcmpl-123","choices":[{"delta":{"content":" world"},"index":0}]}

data: [DONE]
```

**Processing Steps:**
1. Receive raw bytes from libcurl
2. Parse SSE format (extract `data:` lines)
3. Parse JSON from each data line
4. Extract content delta from JSON (`choices[0].delta.content`)
5. Accumulate in streaming buffer
6. Display chunk immediately in scrollback

### Response Assembly

```c
struct ik_repl_ctx_t {
    // ... existing fields ...
    char *streaming_buffer;      // Accumulates current LLM response
    size_t streaming_buffer_len;
    bool is_streaming;
};

void on_llm_chunk(void *user_data, const char *chunk) {
    ik_repl_ctx_t *repl = user_data;
    append_to_streaming_buffer(repl, chunk);
    append_to_scrollback(repl->scrollback, chunk);
    ik_repl_render_frame(repl);
}

void on_llm_complete(void *user_data) {
    ik_repl_ctx_t *repl = user_data;

    // Create message from accumulated buffer
    ik_message_t *msg = create_message(repl, "assistant", repl->streaming_buffer);

    // Persist to DB, add to session_messages
    // Clear streaming state
}
```

---

## Event Loop Integration

### Current Pattern (rel-01)

```c
while (!repl->quit) {
    read_input();
    parse_input();
    apply_actions();
    render_frame();
}
```

### Recommended Approach: select() Polling

**Implementation:**
```c
while (!repl->quit) {
    // Set up file descriptors
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    int max_fd = STDIN_FILENO;
    if (waiting_for_llm) {
        int llm_fd = ik_llm_get_socket_fd(llm);
        FD_SET(llm_fd, &read_fds);
        max_fd = MAX(max_fd, llm_fd);
    }

    // Wait for activity (100ms timeout)
    struct timeval tv = {0, 100000};
    select(max_fd + 1, &read_fds, NULL, NULL, &tv);

    // Handle terminal input (even during streaming)
    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
        read_terminal_input();
        parse_to_actions();
        apply_to_input_buffer();
        // User can scroll, type, or cancel
    }

    // Handle LLM chunks
    if (waiting_for_llm && FD_ISSET(llm_fd, &read_fds)) {
        char *chunk = NULL;
        res_t result = ik_llm_read_chunk(llm, &chunk);
        if (is_ok(&result) && chunk != NULL) {
            append_to_streaming_buffer(repl, chunk);
            append_to_scrollback(repl->scrollback, chunk);
        } else if (stream_complete) {
            finalize_assistant_message(repl);
            waiting_for_llm = false;
        }
    }

    render_frame();
}
```

**Pros:**
- User can interact during streaming
- Can scroll through history
- Can cancel with Ctrl+C/ESC
- Still single-threaded
- Standard POSIX approach

**Alternative (Phase 1):** Blocking approach for simplicity - gets streaming working quickly, acceptable initial UX

**Future:** libcurl multi interface for advanced async (probably overkill for v1.0)

---

## Error Recovery

### Error Scenarios and Handling

**Network Timeout:**
- Display timeout message
- Save partial response if any
- Allow retry

**API Rate Limit (HTTP 429):**
- Display rate limit message
- Don't save partial response
- User can retry after waiting

**Malformed JSON:**
- Display parse error
- Log raw response for debugging
- Continue operation (don't crash)

**Connection Drop Mid-Stream:**
```c
if (is_err(&chunk_result)) {
    append_to_scrollback(repl->scrollback, "\n⚠ Error: LLM stream interrupted");
    append_to_scrollback(repl->scrollback, error_to_string(chunk_result.err));

    // Save partial response with marker
    if (repl->streaming_buffer_len > 0) {
        msg->content = talloc_asprintf(msg, "[INCOMPLETE] %s", msg->content);
        ik_db_message_insert(repl->db, repl->current_session_id, msg);
        append_to_session_messages(repl, msg);
    }

    cleanup_streaming_state(repl);
}
```

---

## Building API Requests

### Converting Session Messages to OpenAI Format

```c
res_t build_openai_request(ik_message_t **messages, size_t message_count,
                           const char *model, char **json_out) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    yyjson_mut_obj_add_str(doc, root, "model", model);
    yyjson_mut_obj_add_bool(doc, root, "stream", true);

    yyjson_mut_val *msg_array = yyjson_mut_arr(doc);
    for (size_t i = 0; i < message_count; i++) {
        // Skip mark and rewind messages (not sent to LLM)
        if (strcmp(msg->role, "mark") == 0 || strcmp(msg->role, "rewind") == 0)
            continue;

        yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, msg_obj, "role", msg->role);
        yyjson_mut_obj_add_str(doc, msg_obj, "content", msg->content);
        yyjson_mut_arr_append(msg_array, msg_obj);
    }
    yyjson_mut_obj_add_val(doc, root, "messages", msg_array);

    *json_out = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
}
```

**Example Request:**
```json
{
    "model": "gpt-4",
    "stream": true,
    "messages": [
        {"role": "user", "content": "Explain talloc"},
        {"role": "assistant", "content": "Talloc is..."},
        {"role": "user", "content": "Show an example"}
    ]
}
```

---

## Parsing Streaming Responses

### SSE Parser

```c
typedef struct {
    char *buffer;          // Accumulates partial lines
    size_t buffer_len;
    bool is_complete;
} sse_parser_t;

res_t sse_parse_chunk(sse_parser_t *parser, const char *chunk, size_t chunk_len,
                      char **content_out) {
    // Append chunk to buffer
    // Look for complete lines (ending with \n)
    // Check for "data: " prefix
    // Check for [DONE] marker
    // Parse JSON and extract content
    // Move buffer forward (remove processed line)
}
```

### JSON Content Extraction

```c
res_t extract_content_from_json(const char *json_str, char **content_out) {
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) return ERR("Malformed JSON");

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Navigate: choices[0].delta.content
    yyjson_val *choices = yyjson_obj_get(root, "choices");
    yyjson_val *choice = yyjson_arr_get_first(choices);
    yyjson_val *delta = yyjson_obj_get(choice, "delta");
    yyjson_val *content = yyjson_obj_get(delta, "content");

    if (content && yyjson_is_str(content)) {
        *content_out = talloc_strdup(NULL, yyjson_get_str(content));
    } else {
        *content_out = NULL;  // No content in this chunk
    }

    yyjson_doc_free(doc);
    return OK(NULL);
}
```

---

## libcurl Integration

### Setup for Non-blocking

```c
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, chunk_callback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, repl);

// Get socket for select()
long sockfd;
curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);
```

### Chunk Callback

```c
size_t chunk_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    ik_repl_ctx_t *repl = userdata;
    size_t total_size = size * nmemb;

    // Append to streaming buffer
    // Parse SSE format
    // Extract JSON chunks from "data: {...}" lines
    // Render to scrollback

    return total_size;  // Must return bytes processed
}
```

### Cancellation

```c
if (action == ACTION_CANCEL && repl->is_streaming) {
    ik_llm_cancel_stream(repl->llm);
    append_to_scrollback(repl->scrollback, "[Cancelled by user]");
    cleanup_streaming_state(repl);
}
```

---

## Testing Strategy

**Unit Tests:**
- Mock libcurl responses
- Feed SSE format data
- Verify JSON parsing
- Error handling (timeouts, malformed JSON, rate limits, cancellation)
- Test cases: complete stream, partial response, empty response, [DONE] marker

**Integration Tests:**
- Send message to LLM
- Receive streaming response
- Verify message persisted to DB
- Check scrollback rendering
- Verify session_messages updated

---

## Related Documentation

- [v1-architecture.md](v1-architecture.md) - Overall v1.0 architecture
- [v1-database-design.md](v1-database-design.md) - Message persistence
- [v1-conversation-management.md](v1-conversation-management.md) - Message lifecycle
- [v1-implementation-roadmap.md](v1-implementation-roadmap.md) - Implementation phases
