# Protocol Module (`protocol.c/h`)

[← Back to Phase 1 Details](phase-1-details.md)

Handles post-handshake message parsing, serialization, and UUID generation. Handshake messages (`hello`/`welcome`) are parsed inline in `websocket.c`.

## API

```c
typedef struct {
  char *sess_id;
  char *corr_id;
  char *type;
  json_t *payload;  // Generic JSON, handler interprets based on type
} ik_protocol_msg_t;

// Parse envelope message from JSON string
ik_result_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str);

// Serialize envelope message to JSON string
ik_result_t ik_protocol_msg_serialize(TALLOC_CTX *ctx, ik_protocol_msg_t *msg);

// Generate base64url-encoded UUID (22 characters)
ik_result_t ik_protocol_generate_uuid(TALLOC_CTX *ctx);

// Constructors for server-created messages
ik_result_t ik_protocol_msg_create_err(TALLOC_CTX *ctx,
                                              const char *sess_id,
                                              const char *corr_id,
                                              const char *source,
                                              const char *err_msg);

ik_result_t ik_protocol_msg_create_assistant_resp(TALLOC_CTX *ctx,
                                                           const char *sess_id,
                                                           const char *corr_id,
                                                           json_t *payload);
```

## Message Format

All post-handshake messages use the envelope format.

**Client → Server:**
```json
{
  "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA",
  "type": "user_query",
  "payload": {
    "model": "gpt-4o-mini",
    "messages": [...]
  }
}
```

**Server → Client:**
```json
{
  "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA",
  "corr_id": "8fKm3pLxTdOqZ1YnHjW9Gg",
  "type": "assistant_response",
  "payload": {...}
}
```

**Fields:**
- `sess_id`: Base64URL-encoded UUID (22 chars) - identifies WebSocket connection (both directions)
- `corr_id`: Base64URL-encoded UUID (22 chars) - identifies exchange (server→client only, for logging/observability)
- `type`: String identifying message type ("user_query", "assistant_response", "error")
- `payload`: Generic JSON object, interpreted based on `type`

**Note:** Client messages do NOT include `corr_id`. Server generates it when processing requests and includes it in responses.

## UUID Generation

```c
ik_result_t ik_protocol_generate_uuid(TALLOC_CTX *ctx) {
    uuid_t uuid;
    uuid_generate_random(uuid);  // 16 bytes from libuuid

    // Base64 encode using libb64
    char *b64 = base64_encode(uuid, 16);
    if (!b64) {
        return ERR(ctx, OOM, "Failed to encode UUID");
    }

    // Convert to base64url: + → -, / → _, remove padding =
    char *b64url = talloc_array(ctx, char, 23);  // 22 + null
    if (!b64url) {
        free(b64);
        return ERR(ctx, OOM, "Failed to allocate UUID string");
    }

    int j = 0;
    for (int i = 0; b64[i] && b64[i] != '='; i++) {
        if (b64[i] == '+') b64url[j++] = '-';
        else if (b64[i] == '/') b64url[j++] = '_';
        else b64url[j++] = b64[i];
    }
    b64url[j] = '\0';

    free(b64);  // libb64 uses malloc
    return OK(b64url);
}
```

**UUID comparison:** IDs are always 22 characters, so can use `strcmp()` or `memcmp(id1, id2, 22)`.

## Message Parsing

```c
ik_result_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str) {
    // Parse JSON
    json_error_t jerr;
    json_t *root = json_loads(json_str, 0, &jerr);
    if (!root) {
        return ERR(ctx, PARSE, "JSON parse error: %s", jerr.text);
    }

    // Extract envelope fields
    // Validate required fields exist and have correct types
    // Extract strings with talloc_strdup()
    // payload remains as json_t* (don't incref - steal or copy as needed)

    json_decref(root);
    return OK(message);
}
```

**Key points:**
- Use jansson's default allocator (malloc/free)
- Extract strings to talloc with `talloc_strdup()`
- `payload` field: keep as `json_t*` pointer (caller decides whether to incref/copy)
- Validate envelope structure, but don't validate payload contents
- Call `json_decref()` on root when done

## Message Serialization

```c
ik_result_t ik_protocol_msg_serialize(TALLOC_CTX *ctx, ik_protocol_msg_t *msg) {
    // Build JSON object with jansson
    json_t *root = json_object();
    json_object_set_new(root, "sess_id", json_string(msg->sess_id));
    json_object_set_new(root, "corr_id", json_string(msg->corr_id));
    json_object_set_new(root, "type", json_string(msg->type));
    json_object_set(root, "payload", msg->payload);  // Reference, not copy

    // Serialize to string
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    if (!json_str) {
        return ERR(ctx, OOM, "Failed to serialize message");
    }

    // Copy to talloc context
    char *result = talloc_strdup(ctx, json_str);
    free(json_str);  // jansson uses malloc

    if (!result) {
        return ERR(ctx, OOM, "Failed to allocate serialized message");
    }

    return OK(result);
}
```

## Handshake Messages

**NOT handled by this module.** WebSocket handler parses handshake inline:

**Hello (client → server):**
```json
{"type": "hello", "identity": "hostname@username"}
```

**Welcome (server → client):**
```json
{"type": "welcome", "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA"}
```

Rationale: Handshake is connection setup (WebSocket layer concern), not application protocol (Protocol layer concern). Keeping them separate allows different formats without complicating the message parser.

## Memory Management

- `ik_protocol_msg_t` and all strings allocated on provided `ctx`
- `payload` is a `json_t*` - caller responsible for reference counting if needed
- No `ik_protocol_msg_free()` - caller uses `talloc_free(ctx)`

## Dependencies

- `libuuid-dev` - UUID generation
- `libb64-dev` - Base64 encoding (add to Makefile)
- `libjansson-dev` - JSON parsing (already present)

Update Makefile:
```makefile
SERVER_LIBS = -lulfius -ljansson -lcurl -ltalloc -luuid -lb64
```

## Test Coverage

`tests/unit/protocol_test.c`:
- Parse valid envelope message
- Parse error on invalid JSON
- Parse error on missing envelope fields
- Parse error on wrong field types
- Serialize message round-trip (parse → serialize → parse)
- UUID generation produces 22-char base64url strings
- UUID generation produces unique values
- ik_protocol_msg_create_err constructs valid error message
- ik_protocol_msg_create_assistant_resp constructs valid response
