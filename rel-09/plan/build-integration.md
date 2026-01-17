# Build Integration

Makefile changes and build dependencies for web-related external tools.

## Overview

All three tools are C programs built from source and installed as external executables:

- `web-search-brave-tool` - Brave Search API client
- `web-search-google-tool` - Google Custom Search API client
- `web-fetch-tool` - HTTP fetcher with HTML→markdown conversion

## Build Target

Add `make tools` target that builds all three external tools.

### Target Definition

```makefile
tools: web-search-brave-tool web-search-google-tool web-fetch-tool

web-search-brave-tool: tools/web-search-brave.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(HTTP_LIBS)

web-search-google-tool: tools/web-search-google.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(HTTP_LIBS)

web-fetch-tool: tools/web-fetch.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(HTTP_LIBS) $(XML_LIBS)
```

### Source Location

Tool source files live in `tools/` directory:

```
tools/
├── web-search-brave.c
├── web-search-google.c
└── web-fetch.c
```

Not in `src/` because these are external executables, not ikigai core code.

## Dependencies

### libxml2

Required for HTML parsing in `web-fetch-tool`.

**Package names:**
- Debian/Ubuntu: `libxml2-dev`
- Fedora/RHEL: `libxml2-devel`
- macOS: `brew install libxml2`
- Arch: `libxml2`

**Compiler flags:**
```makefile
XML_CFLAGS := $(shell pkg-config --cflags libxml-2.0)
XML_LIBS := $(shell pkg-config --libs libxml-2.0)
```

### HTTP Client Library

**Decision: libcurl**

ikigai core already uses libcurl for all HTTP operations (provider API calls). External tools will use the same library for consistency.

**Rationale:**
- Already a dependency (ikigai links against `-lcurl`)
- Mature, battle-tested, widely available
- Handles HTTPS, redirects, timeouts automatically
- Consistent with ikigai's HTTP abstraction (see `src/wrapper_curl.c`)
- No additional build dependencies needed

**Compiler flags:**
```makefile
HTTP_CFLAGS := $(shell pkg-config --cflags libcurl)
HTTP_LIBS := $(shell pkg-config --libs libcurl)
```

**Note:** libcurl is already a dependency of ikigai core (see Makefile `CLIENT_LIBS`). No additional installation required.

### JSON Library

Tools need JSON parsing for:
1. Tool input from stdin (request parameters)
2. Tool output to stdout (response data)
3. Credentials file parsing (`~/.config/ikigai/credentials.json`)
4. API response parsing (Brave/Google return JSON)

**Options:**

1. **jsmn** (minimalist)
   - Pros: Single header, no dependencies
   - Cons: Manual parsing, more code

2. **cJSON**
   - Pros: Simple API, easy to use
   - Cons: Additional dependency
   - Flags: `-lcjson`

3. **jansson**
   - Pros: Clean API, good documentation
   - Cons: Less common
   - Flags: `$(shell pkg-config --cflags --libs jansson)`

**Recommendation:** cJSON for simplicity.

**Compiler flags:**
```makefile
JSON_LIBS := -lcjson
```

**Credentials file structure** (see `tool-schemas.md`):
```json
{
  "web_search": {
    "brave": {"api_key": "..."},
    "google": {"api_key": "...", "engine_id": "..."}
  }
}
```

Tools must parse nested JSON to extract credentials.

## Makefile Variables

Add to top of Makefile:

```makefile
# External tool dependencies
XML_CFLAGS := $(shell pkg-config --cflags libxml-2.0)
XML_LIBS := $(shell pkg-config --libs libxml-2.0)
HTTP_CFLAGS := $(shell pkg-config --cflags libcurl)
HTTP_LIBS := $(shell pkg-config --libs libcurl)
JSON_LIBS := -lcjson

# Update CFLAGS to include external tool flags
CFLAGS += $(XML_CFLAGS) $(HTTP_CFLAGS)
```

## Installation

External tools install to `libexec/ikigai/`:

```makefile
install: tools
	install -d $(DESTDIR)$(PREFIX)/libexec/ikigai
	install -m 755 web-search-brave-tool $(DESTDIR)$(PREFIX)/libexec/ikigai/
	install -m 755 web-search-google-tool $(DESTDIR)$(PREFIX)/libexec/ikigai/
	install -m 755 web-fetch-tool $(DESTDIR)$(PREFIX)/libexec/ikigai/
```

**Path resolution:**
- `PREFIX` defaults to `/usr/local`
- System install: `/usr/local/libexec/ikigai/web-search-brave-tool`
- User override: `~/.ikigai/tools/web-search-brave-tool`
- Project override: `.ikigai/tools/web-search-brave-tool`

## Clean Target

Update clean target to remove built tools:

```makefile
clean:
	rm -f $(OBJS) ikigai
	rm -f web-search-brave-tool web-search-google-tool web-fetch-tool
	rm -f tests/unit/*.o tests/integration/*.o
```

## Dependency Checking

Optional: Add dependency checks to fail fast if libraries missing:

```makefile
check-deps:
	@pkg-config --exists libxml-2.0 || (echo "libxml2 not found. Install libxml2-dev" && false)
	@pkg-config --exists libcurl || (echo "libcurl not found. Install libcurl-dev" && false)
	@echo "Dependencies OK"

tools: check-deps
```

## Build Order

Normal build flow:

```bash
make              # Build ikigai (core)
make tools        # Build external tools
make install      # Install both core and tools
```

## Platform Considerations

### Linux
- Standard pkg-config paths work
- Install dev packages via package manager

### macOS
- May need `PKG_CONFIG_PATH` for Homebrew libraries
- Example: `export PKG_CONFIG_PATH=/usr/local/opt/libxml2/lib/pkgconfig`

### BSD
- Similar to Linux, check package names

## Testing Tools

Tools can be tested independently:

```bash
# Test schema output
./web-search-brave-tool --schema

# Test execution
echo '{"query":"test search"}' | ./web-search-brave-tool

# Test with missing credentials (should fail gracefully)
env -u BRAVE_API_KEY ./web-search-brave-tool <<< '{"query":"test"}'
```

## Documentation

Update `project/external-tool-architecture.md` to reference these build instructions.

Update `README.md` to document:
- Build dependencies (libxml2, libcurl, cjson)
- Installation instructions
- Credential configuration

## Implementation Notes

### Library Choice Rationale

**libxml2**: Industry standard, excellent HTML parsing, well-maintained

**libcurl**: De facto standard for HTTP in C, handles TLS, redirects, timeouts automatically

**cJSON**: Lightweight, simple API, sufficient for tool I/O needs

### Alternative: Static Linking

Could statically link dependencies to create standalone tools:

```makefile
LDFLAGS += -static
```

Pros: No runtime dependencies, easier deployment
Cons: Larger binaries, security updates require rebuild

Decision: Dynamic linking preferred for standard distro packages.

### Error Handling

All tools must:
1. Validate JSON input from stdin
2. Return meaningful error messages on stdout (as JSON with `success: false` field)
3. Exit 0 for successful execution (even if operation failed - use `success` field to indicate operation failure)
4. Exit non-zero only for tool crashes or fatal errors
5. Include optional `_event` field in JSON output for metadata (extracted by ikigai, not sent to LLM)
6. Never write to stderr (protocol uses stdout only, stderr is discarded)

**Event Field Protocol:**

Tools can include an optional `_event` field in their JSON output for out-of-band metadata:

```json
{
  "success": false,
  "error": "...",
  "error_code": "AUTH_MISSING",
  "_event": {
    "kind": "config_required",
    "content": "User-facing message with setup instructions",
    "data": {"tool": "web_search_brave", "credential": "api_key", ...}
  }
}
```

**Processing:**
- ikigai's tool_wrapper.c extracts `_event` field from tool output
- Event stored in messages table (kind='config_required')
- `_event` field removed before wrapping result for LLM
- User sees event displayed separately (dim yellow)
- LLM sees only the error message (not the event metadata)

**Use cases:**
- Configuration required (missing credentials)
- User action needed (external setup, approval, etc.)
- Important warnings (not errors)

The `_` prefix indicates internal metadata not meant for LLM consumption.

### Timeouts

Tools must respect the 30-second execution timeout enforced by ikigai's external tool framework.

Internal HTTP timeouts should be shorter (e.g., 10 seconds) to allow for retry logic.

---

## Database Integration for config_required Events

Tools emit `config_required` events via the `_event` field in JSON output. ikigai must extract these events and store them in the database.

### Integration Point: tool_wrapper.c

The `ik_tool_wrap_success()` function (or equivalent wrapper) must be updated to:

1. **Parse tool JSON output** and extract optional `_event` field
2. **If `_event` field present:**
   - Store event in messages table
   - Remove `_event` field from result before wrapping for LLM
3. **Wrap remaining result** for LLM as usual

### Event Structure

Tools include `_event` field in JSON output:

```json
{
  "success": false,
  "error": "...",
  "error_code": "AUTH_MISSING",
  "_event": {
    "kind": "config_required",
    "content": "⚠ Configuration Required\n\n...",
    "data": {
      "tool": "web_search_brave",
      "credential": "api_key",
      "signup_url": "https://brave.com/search/api/"
    }
  }
}
```

### Database Schema

Messages table (share/ikigai/migrations/001-initial-schema.sql:41-48) already supports arbitrary event types:

```sql
CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL REFERENCES sessions(id),
    kind TEXT NOT NULL,        -- Event type: 'config_required'
    content TEXT,              -- User-facing message from _event.content
    data JSONB,                -- Structured metadata from _event.data
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

### Insertion Logic

When `_event` field is present in tool output:

```c
// Pseudo-code for tool_wrapper.c
res_t ik_tool_wrap_success(ctx, tool_output_json, wrapped_result) {
    // 1. Parse tool JSON
    yyjson_val *event = yyjson_obj_get(root, "_event");

    if (event != NULL) {
        // 2. Extract event fields
        const char *kind = yyjson_get_str(yyjson_obj_get(event, "kind"));
        const char *content = yyjson_get_str(yyjson_obj_get(event, "content"));
        yyjson_val *data = yyjson_obj_get(event, "data");

        // 3. Serialize data to JSON string for JSONB column
        char *data_json = yyjson_val_write(data, 0, NULL);

        // 4. Insert into messages table
        // INSERT INTO messages (session_id, kind, content, data)
        // VALUES ($1, $2, $3, $4::jsonb)
        // Use current session_id from context

        // 5. Remove _event field from tool result
        yyjson_mut_obj_remove(mutable_root, "_event");

        // 6. Free data_json after insertion
        free(data_json);
    }

    // 7. Wrap remaining result for LLM (without _event field)
    // ... existing wrapping logic ...
}
```

### Display Logic

Scrollback rendering must query and display `config_required` events:

**Query:** When loading conversation history, fetch all messages including `kind='config_required'`

**Rendering:** Events with `kind='config_required'` displayed in dim yellow, separate from tool_result

**Association:** Event is inserted immediately after tool execution completes, so it appears in the same conversation turn as the tool_call/tool_result pair.

### Event Lifecycle

1. Tool executes, finds credentials missing
2. Tool returns JSON with `success: false` and `_event` field
3. `ik_tool_wrap_success()` extracts `_event`, stores in database
4. `_event` removed from result
5. Wrapped result sent to LLM: `{"tool_success": true, "result": {"success": false, "error": "..."}}`
6. LLM sees only the error message, not the event metadata
7. User sees both:
   - Tool result in dim gray
   - Config event in dim yellow (queried from messages table during scrollback render)

This separation allows tools to provide rich setup instructions to users without cluttering the LLM context.
