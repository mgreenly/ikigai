# Build Integration

Makefile changes and build dependencies for web-related external tools.

## Overview

All three tools are C programs built from source and installed as external executables:

- `web-search-brave-tool` - Brave Search API client
- `web-search-google-tool` - Google Custom Search API client
- `web-fetch-tool` - HTTP fetcher with HTML→markdown conversion

## Build Targets

Add three new external tools following the existing pattern established by bash-tool, file-read-tool, etc.

### Source Location

Tool source files follow existing pattern in `src/tools/TOOLNAME/main.c`:

```
src/tools/
├── web_search_brave/
│   └── main.c
├── web_search_google/
│   └── main.c
└── web_fetch/
    └── main.c
```

**Rationale:** External tools ARE still ikigai source code, just built as separate executables. Keeping them in `src/tools/` maintains consistency with existing tools (bash, file_read, file_write, file_edit, glob, grep).

### Target Definitions

Following existing Makefile pattern (see Makefile:581-609):

```makefile
# Web search tool (Brave)
web_search_brave_tool: libexec/ikigai/web-search-brave-tool

libexec/ikigai/web-search-brave-tool: src/tools/web_search_brave/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(TYPE_FLAGS) \
		-o $@ $< $(TOOL_COMMON_SRCS) $(CLIENT_LIBS) -lcurl

# Web search tool (Google)
web_search_google_tool: libexec/ikigai/web-search-google-tool

libexec/ikigai/web-search-google-tool: src/tools/web_search_google/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(TYPE_FLAGS) \
		-o $@ $< $(TOOL_COMMON_SRCS) $(CLIENT_LIBS) -lcurl

# Web fetch tool
web_fetch_tool: libexec/ikigai/web-fetch-tool

libexec/ikigai/web-fetch-tool: src/tools/web_fetch/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(TYPE_FLAGS) \
		-o $@ $< $(TOOL_COMMON_SRCS) $(CLIENT_LIBS) -lcurl $(shell pkg-config --libs libxml-2.0)
```

**Key points:**
- Builds to `libexec/ikigai/TOOLNAME-tool` (consistent with existing tools)
- Uses existing Makefile variables: `$(BASE_FLAGS)`, `$(WARNING_FLAGS)`, `$(CLIENT_LIBS)`, etc.
- Uses `$(TOOL_COMMON_SRCS)` for shared tool infrastructure
- Appends `-lcurl` directly (already in CLIENT_LIBS, but explicit for clarity)
- web-fetch-tool adds libxml2 via `pkg-config --libs libxml-2.0` (per-target, not global)
- Convenience targets (`web_search_brave_tool:`) follow existing pattern

### Update Aggregate Tools Target

Update existing `tools:` target to include web tools:

```makefile
tools: bash_tool file_read_tool file_write_tool file_edit_tool glob_tool grep_tool \
       web_search_brave_tool web_search_google_tool web_fetch_tool
```

## Dependencies

### libxml2

Required for HTML parsing in `web-fetch-tool` only.

**Package names:**
- Debian/Ubuntu: `libxml2-dev`
- Fedora/RHEL: `libxml2-devel`
- macOS: `brew install libxml2`
- Arch: `libxml2`

**Makefile handling:**

libxml2 is added **per-target** (not globally) using pkg-config inline:

```makefile
libexec/ikigai/web-fetch-tool: src/tools/web_fetch/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(TYPE_FLAGS) \
		-o $@ $< $(TOOL_COMMON_SRCS) $(CLIENT_LIBS) -lcurl $(shell pkg-config --libs libxml-2.0)
```

**No global variables needed** - pkg-config is called inline in the target where it's needed. This keeps libxml2 scoped to web-fetch-tool only.

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

**Decision: yyjson (already used by ikigai core)**

ikigai core already uses yyjson for all JSON operations. Web tools should use the same library for consistency.

**Rationale:**
- Already vendored in `vendor/yyjson/` (no new dependency)
- Fast, well-tested, used throughout ikigai
- Talloc integration via `src/json_allocator.h` and `src/json_allocator.c`
- Consistent API patterns with ikigai core

**Usage in tools:**
```c
#include "vendor/yyjson/yyjson.h"
#include "json_allocator.h"

// Parse with talloc allocator
yyjson_alc allocator = ik_make_talloc_allocator(ctx);
yyjson_doc *doc = yyjson_read_opts(input, len, 0, &allocator, NULL);
```

**No additional Makefile changes needed** - yyjson source is already in `$(TOOL_COMMON_SRCS)` and json_allocator.c is compiled with tools.

**Credentials file structure** (see `tool-schemas.md`):
```json
{
  "web_search": {
    "brave": {"api_key": "..."},
    "google": {"api_key": "...", "engine_id": "..."}
  }
}
```

Tools must parse nested JSON to extract credentials using yyjson API.

## Makefile Variables

**No new global variables needed.**

Web tools use existing Makefile infrastructure:
- `$(CLIENT_LIBS)` already includes `-lcurl` (HTTP client)
- `$(TOOL_COMMON_SRCS)` already includes yyjson and json_allocator (JSON parsing)
- `$(BASE_FLAGS)`, `$(WARNING_FLAGS)`, etc. already defined

**Only web-fetch-tool needs libxml2** for HTML parsing. This is added per-target:

```makefile
libexec/ikigai/web-fetch-tool: src/tools/web_fetch/main.c $(TOOL_COMMON_SRCS) | libexec/ikigai
	$(CC) $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(TYPE_FLAGS) \
		-o $@ $< $(TOOL_COMMON_SRCS) $(CLIENT_LIBS) -lcurl $(shell pkg-config --libs libxml-2.0)
```

**Why per-target?**
- Only one tool needs libxml2 (web-fetch-tool)
- Adding to global CFLAGS would affect all compilation unnecessarily
- Per-target flags keep dependencies explicit and scoped

## Installation

Update existing `install:` target to include web tools (Makefile:645-660):

```makefile
install: ikigai tools
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(datadir)/ikigai
	install -d $(DESTDIR)$(configdir)
	install -d $(DESTDIR)$(libexecdir)/ikigai
	# ... existing ikigai installation ...

	# Install tool binaries to libexec
	install -m 755 libexec/ikigai/bash-tool $(DESTDIR)$(libexecdir)/ikigai/
	install -m 755 libexec/ikigai/file-read-tool $(DESTDIR)$(libexecdir)/ikigai/
	install -m 755 libexec/ikigai/file-write-tool $(DESTDIR)$(libexecdir)/ikigai/
	install -m 755 libexec/ikigai/file-edit-tool $(DESTDIR)$(libexecdir)/ikigai/
	install -m 755 libexec/ikigai/glob-tool $(DESTDIR)$(libexecdir)/ikigai/
	install -m 755 libexec/ikigai/grep-tool $(DESTDIR)$(libexecdir)/ikigai/
	# Add web tools:
	install -m 755 libexec/ikigai/web-search-brave-tool $(DESTDIR)$(libexecdir)/ikigai/
	install -m 755 libexec/ikigai/web-search-google-tool $(DESTDIR)$(libexecdir)/ikigai/
	install -m 755 libexec/ikigai/web-fetch-tool $(DESTDIR)$(libexecdir)/ikigai/
```

**Path resolution:**
- Uses existing `$(libexecdir)` variable (defaults to `$(PREFIX)/libexec`)
- System install: `/usr/local/libexec/ikigai/web-search-brave-tool`
- User override: `~/.ikigai/tools/web-search-brave-tool` (tool discovery checks here first)
- Project override: `.ikigai/tools/web-search-brave-tool` (tool discovery checks here second)

## Clean Target

**No changes needed.** Existing `clean:` target already removes `libexec/` directory (Makefile:635):

```makefile
clean:
	rm -rf build build-* bin libexec $(COVERAGE_DIR) coverage_html reports
```

Web tools built to `libexec/ikigai/web-*-tool` are already cleaned by this target.

## Uninstall Target

Update existing `uninstall:` target to remove web tools (Makefile:693-701):

```makefile
uninstall:
	# ... existing removals ...
	rm -f $(DESTDIR)$(libexecdir)/ikigai/bash-tool
	rm -f $(DESTDIR)$(libexecdir)/ikigai/file-read-tool
	rm -f $(DESTDIR)$(libexecdir)/ikigai/file-write-tool
	rm -f $(DESTDIR)$(libexecdir)/ikigai/file-edit-tool
	rm -f $(DESTDIR)$(libexecdir)/ikigai/glob-tool
	rm -f $(DESTDIR)$(libexecdir)/ikigai/grep-tool
	# Add web tools:
	rm -f $(DESTDIR)$(libexecdir)/ikigai/web-search-brave-tool
	rm -f $(DESTDIR)$(libexecdir)/ikigai/web-search-google-tool
	rm -f $(DESTDIR)$(libexecdir)/ikigai/web-fetch-tool
	rmdir $(DESTDIR)$(libexecdir)/ikigai 2>/dev/null || true
	rmdir $(DESTDIR)$(libexecdir) 2>/dev/null || true
```

## Dependency Checking

Optional: Add dependency check for libxml2 (only new dependency):

```makefile
check-web-deps:
	@pkg-config --exists libxml-2.0 || (echo "libxml2 not found. Install libxml2-dev" && false)
	@echo "Web tool dependencies OK"

web_fetch_tool: check-web-deps
```

**Note:** libcurl is already required by ikigai core, so no check needed.

## Summary of Makefile Changes

**Files to create:**
- `src/tools/web_search_brave/main.c`
- `src/tools/web_search_google/main.c`
- `src/tools/web_fetch/main.c`

**Makefile modifications:**
1. Add 3 new tool build targets (following existing pattern)
2. Add 3 convenience targets: `web_search_brave_tool`, `web_search_google_tool`, `web_fetch_tool`
3. Update aggregate `tools:` target to include web tools
4. Update `install:` target to install web tools
5. Update `uninstall:` target to remove web tools
6. No changes to `clean:` (already removes libexec/)
7. No new global variables needed

**Dependencies:**
- libxml2-dev (new, only for web-fetch-tool)
- libcurl (already required by ikigai core)
- yyjson (already vendored in ikigai)

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

Tools can be tested independently from build directory:

```bash
# Test schema output
libexec/ikigai/web-search-brave-tool --schema

# Test execution (requires BRAVE_API_KEY environment variable)
echo '{"query":"test search"}' | libexec/ikigai/web-search-brave-tool

# Test with missing credentials (should return error with _event field)
env -u BRAVE_API_KEY libexec/ikigai/web-search-brave-tool <<< '{"query":"test"}'
```

**Note:** During test/build, environment variables for credentials are always set. Tools should handle both env var credentials (for testing) and credentials file (for production).

## Documentation

Update `project/external-tool-architecture.md` to reference these build instructions.

Update `README.md` to document:
- Build dependencies (libxml2-dev for web-fetch-tool only; libcurl already required)
- Installation instructions
- Credential configuration (environment variables + credentials.json)

## Implementation Notes

### Library Choice Rationale

**libxml2**: Industry standard, excellent HTML parsing, well-maintained

**libcurl**: De facto standard for HTTP in C, handles TLS, redirects, timeouts automatically (already used by ikigai core)

**yyjson**: High-performance JSON library already used throughout ikigai core, vendored in `vendor/yyjson/`, integrates with talloc via `src/json_allocator.c`

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
