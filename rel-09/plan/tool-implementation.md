# Tool Implementation Patterns

Internal C implementation patterns for external tools, based on existing tool conventions in `src/tools/`.

## Overview

All external tools follow consistent internal patterns:
- **Memory management:** talloc with single root context
- **Error handling:** Simple int return codes (0 = success)
- **Initialization:** Standard pattern for all tools
- **Cleanup:** Single talloc_free() on all paths

These patterns are already established in existing tools (bash, file_read, file_write, file_edit, glob, grep).

---

## Memory Management

### Pattern: talloc with Root Context

**All tools use talloc**, consistent with ikigai core. Pattern from existing tools:

```c
int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    // All allocations off ctx
    char *buffer = talloc_array(ctx, char, size);
    char *string = talloc_strdup(ctx, source);

    // On success or error:
    talloc_free(ctx);
    return 0;  // or 1 for error
}
```

**See:** `src/tools/bash/main.c:14`, `src/tools/file_read/main.c:14`, etc.

### Allocation Rules

1. **Root context:** `void *ctx = talloc_new(NULL);` at start of main()
2. **All tool allocations:** Use talloc functions off ctx
   - `talloc_array(ctx, type, count)` - Arrays
   - `talloc_strdup(ctx, str)` - String copies
   - `talloc_asprintf(ctx, fmt, ...)` - Formatted strings
3. **Library allocations:** Must be freed before talloc_free(ctx)
4. **Single cleanup:** `talloc_free(ctx);` before every return

### Mixed Library Memory

**Web tools will use libraries with different allocators:**

- **libcurl:** Uses malloc/free internally (automatic cleanup via curl_easy_cleanup())
- **libxml2:** Uses xmlMalloc/xmlFree (must call xmlFreeDoc(), xmlCleanupParser())
- **cJSON:** Uses malloc/free (must call cJSON_Delete())

**Pattern:**

```c
void *ctx = talloc_new(NULL);

// Tool allocations with talloc
char *tool_data = talloc_strdup(ctx, "...");

// Library allocations
CURL *curl = curl_easy_init();          // malloc internally
xmlDocPtr doc = xmlReadMemory(...);     // xmlMalloc internally
cJSON *json = cJSON_Parse(input);       // malloc internally

// Use library objects...

// Free library objects BEFORE talloc_free
curl_easy_cleanup(curl);
xmlFreeDoc(doc);
cJSON_Delete(json);

// Free all tool allocations
talloc_free(ctx);
return 0;
```

**Key rule:** Library-allocated memory must be freed with library-specific functions before `talloc_free(ctx)`.

### Error Paths

**All error paths must free memory:**

```c
void *ctx = talloc_new(NULL);

if (some_error) {
    talloc_free(ctx);
    return 1;
}

// More code...

if (another_error) {
    talloc_free(ctx);
    return 1;
}

// Success
talloc_free(ctx);
return 0;
```

**Pattern:** Every return statement preceded by `talloc_free(ctx);`

---

## Return Value Conventions

### Main Function

**Tools use standard C exit codes:**

- **Exit 0:** Tool executed successfully
  - Operation may have failed (check JSON `success` field)
  - Tool produced valid JSON output
  - Example: Credential error returns exit 0 with `{success: false, ...}`

- **Exit non-zero:** Tool crashed or failed to execute
  - Invalid JSON input
  - Out of memory (should never happen with proper talloc)
  - Fatal internal error
  - ikigai wraps as `{"tool_success": false, "error": "..."}`

**Pattern from existing tools:**

```c
int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    // Handle --schema flag
    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{...schema JSON...}\n");
        talloc_free(ctx);
        return 0;
    }

    // Parse JSON input
    if (parse_error) {
        fprintf(stderr, "tool: invalid JSON\n");
        talloc_free(ctx);
        return 1;  // Fatal error
    }

    // Execute operation
    if (operation_failed) {
        // Write error JSON to stdout
        printf("{\"success\": false, \"error\": \"...\"}\n");
        talloc_free(ctx);
        return 0;  // Tool executed, operation failed
    }

    // Write success JSON to stdout
    printf("{\"success\": true, \"result\": {...}}\n");
    talloc_free(ctx);
    return 0;
}
```

**See:** `src/tools/bash/main.c`, `src/tools/file_read/main.c`

### Internal Functions

**Tools use simple int return codes for internal functions:**

```c
// Return 0 on success, -1 on error
static int32_t parse_credentials(void *ctx, const char *path, char **out_key)
{
    // ...
    if (error) {
        return -1;
    }

    *out_key = talloc_strdup(ctx, key);
    return 0;
}

// Caller checks return value
int32_t main(...)
{
    char *api_key = NULL;
    if (parse_credentials(ctx, path, &api_key) != 0) {
        printf("{\"success\": false, \"error\": \"Failed to load credentials\"}\n");
        talloc_free(ctx);
        return 0;
    }
    // Continue...
}
```

**Pattern:**
- Return `0` for success, `-1` for error
- Output parameters via pointers (allocated off ctx)
- No res_t or complex error types needed for tools

**Why not res_t?**
- Tools are standalone executables, not linked against ikigai core
- res_t requires error.h, error.c from ikigai
- Simple int codes sufficient for tool-internal logic
- Error details communicated via JSON output, not return values

---

## Standard Tool Structure

### Main Function Template

```c
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"
#include "json_allocator.h"

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    // 1. Handle --schema flag
    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n");
        printf("  \"name\": \"tool_name\",\n");
        // ... schema JSON ...
        printf("}\n");
        talloc_free(ctx);
        return 0;
    }

    // 2. Read stdin
    size_t buffer_size = 4096;
    size_t total_read = 0;
    char *input = talloc_array(ctx, char, buffer_size);
    // ... read loop ...

    if (total_read == 0) {
        fprintf(stderr, "tool: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // 3. Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "tool: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    // 4. Extract parameters
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *param = yyjson_obj_get(root, "param_name");
    if (param == NULL) {
        fprintf(stderr, "tool: missing parameter\n");
        talloc_free(ctx);
        return 1;
    }

    // 5. Execute operation
    // ... tool logic ...

    // 6. Write JSON output
    printf("{\"success\": true, \"result\": {...}}\n");

    talloc_free(ctx);
    return 0;
}
```

### Shared Utilities

**Web tools may share common code:**

- **JSON parsing:** Already have `json_allocator.h` and yyjson
- **Credential loading:** Could extract to `tools/common/credentials.c`
- **HTTP client wrapper:** Could create `tools/common/http.c` wrapping libcurl
- **Error formatting:** Could create `tools/common/error_json.c` for consistent error JSON

**Decision:** Start without shared utilities, extract if duplication becomes significant (3+ tools doing identical operations).

**Rationale:** External tools are independent executables. Some duplication is acceptable. Shared code adds build complexity.

---

## Common Patterns

### Credential Loading

```c
static int32_t load_credentials(void *ctx,
                                 const char *tool_name,
                                 const char *cred_name,
                                 char **out_value)
{
    // 1. Check environment variable
    char env_var[256];
    snprintf(env_var, sizeof(env_var), "%s_%s", tool_name, cred_name);
    const char *env_value = getenv(env_var);
    if (env_value != NULL) {
        *out_value = talloc_strdup(ctx, env_value);
        return 0;
    }

    // 2. Check credentials file
    const char *config_path = "~/.config/ikigai/credentials.json";
    // ... load and parse JSON file ...
    // ... extract tool_name.cred_name value ...

    // 3. Not found
    return -1;
}
```

### HTTP Request

```c
static int32_t http_get(void *ctx,
                        const char *url,
                        const char *api_key,
                        char **out_response)
{
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        return -1;
    }

    // Configure request
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "X-API-Key: %s", api_key);
    headers = curl_slist_append(headers, auth_header);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Capture response
    struct response_buffer {
        void *ctx;
        char *data;
        size_t size;
    } buffer = {.ctx = ctx, .data = NULL, .size = 0};

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return -1;
    }

    *out_response = buffer.data;
    return 0;
}
```

### JSON Output Formatting

```c
static void write_error_json(const char *error_msg, const char *error_code)
{
    printf("{\n");
    printf("  \"success\": false,\n");
    printf("  \"error\": \"%s\",\n", error_msg);  // TODO: escape quotes
    printf("  \"error_code\": \"%s\"\n", error_code);
    printf("}\n");
}

static void write_success_json(yyjson_mut_doc *doc)
{
    // Build JSON using yyjson
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_bool(doc, root, "success", true);
    // ... add result fields ...

    // Serialize
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    printf("%s\n", json_str);
    free(json_str);
}
```

---

## Build Integration

### Tool Makefile Pattern

```makefile
# Web tools
WEB_TOOL_SOURCES = tools/common/credentials.c tools/common/http.c

web-search-brave-tool: tools/web-search-brave.c $(WEB_TOOL_SOURCES)
	$(CC) $(CFLAGS) $(HTTP_CFLAGS) $(XML_CFLAGS) -o $@ $^ \
		$(LDFLAGS) $(HTTP_LIBS) $(JSON_LIBS) -ltalloc

web-search-google-tool: tools/web-search-google.c $(WEB_TOOL_SOURCES)
	$(CC) $(CFLAGS) $(HTTP_CFLAGS) $(XML_CFLAGS) -o $@ $^ \
		$(LDFLAGS) $(HTTP_LIBS) $(JSON_LIBS) -ltalloc

web-fetch-tool: tools/web-fetch.c $(WEB_TOOL_SOURCES)
	$(CC) $(CFLAGS) $(HTTP_CFLAGS) $(XML_CFLAGS) -o $@ $^ \
		$(LDFLAGS) $(HTTP_LIBS) $(XML_LIBS) $(JSON_LIBS) -ltalloc
```

### Tool Source Structure

```
tools/
├── common/                    # Shared utilities (if needed)
│   ├── credentials.c
│   ├── credentials.h
│   ├── http.c
│   └── http.h
├── web-search-brave.c         # Brave Search tool
├── web-search-google.c        # Google Search tool
└── web-fetch.c                # URL fetch tool
```

**Note:** common/ directory only created if significant duplication emerges during implementation.

---

## Testing

### Memory Testing

All tools must be tested for memory leaks using valgrind:

```bash
echo '{"query":"test"}' | valgrind --leak-check=full ./web-search-brave-tool
```

**Expected:** "All heap blocks were freed -- no leaks are possible"

### Pattern Testing

Unit tests verify the standard patterns:

```c
// tests/unit/tools/web-search-brave-test.c
TEST(web_search_brave, schema_flag)
{
    // Verify --schema returns valid JSON with exit 0
}

TEST(web_search_brave, empty_input_fails)
{
    // Verify empty stdin returns exit 1
}

TEST(web_search_brave, invalid_json_fails)
{
    // Verify malformed JSON returns exit 1
}

TEST(web_search_brave, missing_credentials_returns_error_json)
{
    // Verify exit 0 with success: false when credentials missing
}
```

See `test-strategy.md` for complete testing approach.

---

## Summary

**Memory Management:**
- talloc with single root context
- All tool allocations off ctx
- Library allocations freed before talloc_free(ctx)
- Every return preceded by talloc_free(ctx)

**Return Values:**
- Exit 0: Tool executed (check JSON for operation result)
- Exit 1: Tool crashed (invalid input, fatal error)
- Internal functions: 0 = success, -1 = error
- No res_t (tools are standalone, not linked against ikigai core)

**Standard Structure:**
- --schema flag handling
- stdin reading with growing buffer
- JSON parsing with yyjson
- Parameter extraction
- Operation execution
- JSON output formatting
- Memory cleanup

These patterns are already proven in existing tools. Web tools follow the same conventions.
