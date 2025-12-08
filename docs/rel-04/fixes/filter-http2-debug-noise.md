# Fix: Filter HTTP/2 Debug Noise

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions
- `naming.md` - Naming conventions

## Files to Explore

### Source files:
- `src/openai/client_multi_request.c` - Current debug output (lines 98-107)
- `src/wrapper.h` - Check for curl wrapper functions
- `src/debug_pipe.h` - Debug pipe infrastructure

### Test files:
- `tests/unit/repl/repl_debug_pipe_integration_test.c` - Existing debug pipe tests
- Any tests that verify debug output format

## Situation

### Current Behavior

Debug output uses `CURLOPT_VERBOSE` which dumps ALL HTTP/2 protocol details:
```
>> REQUEST
>> URL: https://api.openai.com/v1/chat/completions
>> Body: {"model":"gpt-4o",...}

*   Trying 104.18.7.192:443...
* Connected to api.openai.com (104.18.7.192) port 443
* ALPN: curl offers h2,http/1.1
* TLSv1.3 (OUT), TLS handshake...
* SSL connection using TLSv1.3 / AEAD-AES256-GCM-SHA384
* ALPN: server accepted h2
* using HTTP/2
> POST /v1/chat/completions HTTP/2
> Host: api.openai.com
> Authorization: Bearer sk-proj-actual-secret-key-here
> Content-Type: application/json
> Content-Length: 1234
... hundreds more lines of HTTP/2 frames, window updates, etc. ...
```

### Problem

Users report debug output is too verbose:
1. HTTP/2 protocol negotiation noise (TLS handshake, ALPN, frames) clutters output
2. Authorization header leaks API key secret
3. Only need to see: HTTP method, URL, headers (redacted), and body

### Target Behavior

Clean, focused debug output:
```
>> POST https://api.openai.com/v1/chat/completions
>> Host: api.openai.com
>> Authorization: [REDACTED]
>> Content-Type: application/json
>> Content-Length: 1234
>>
>> {"model":"gpt-4o",...}

<< HTTP/2 200
<< content-type: application/json
<< ...other response headers...
<<
<< {"id":"chatcmpl-123",...}
```

**Key improvements:**
- Show HTTP verb and URL on first line
- Show all headers EXCEPT Authorization (show as `[REDACTED]`)
- Filter out SSL/TLS/HTTP2 protocol details
- Keep request bodies and response bodies
- Use `>>` prefix for outgoing, `<<` for incoming

## High-Level Goal

**Replace CURLOPT_VERBOSE with CURLOPT_DEBUGFUNCTION to filter HTTP/2 noise and redact Authorization header.**

## Implementation Approach

### Step 1: Add curl debug callback function

In `src/openai/client_multi_request.c`, add a custom debug callback:

```c
// libcurl debug info types (from curl/curl.h)
// We need these to filter in our callback
#define CURLINFO_TEXT         0
#define CURLINFO_HEADER_IN    1
#define CURLINFO_HEADER_OUT   2
#define CURLINFO_DATA_IN      3
#define CURLINFO_DATA_OUT     4
#define CURLINFO_SSL_DATA_IN  5
#define CURLINFO_SSL_DATA_OUT 6

// Custom curl debug callback - filters HTTP/2 noise and redacts secrets.
//
// Called by libcurl for each debug event. We filter to show only:
// - Outgoing headers (CURLINFO_HEADER_OUT) with Authorization redacted
// - Incoming headers (CURLINFO_HEADER_IN)
// - Request/response bodies (CURLINFO_DATA_OUT, CURLINFO_DATA_IN)
//
// Filtered out:
// - SSL/TLS negotiation (CURLINFO_SSL_DATA_IN/OUT)
// - Informational messages (CURLINFO_TEXT) like "Trying 104.18.7.192..."
//
// @param handle  curl handle (unused)
// @param type    Info type (header, data, ssl, text)
// @param data    Data buffer (may not be null-terminated)
// @param size    Data size
// @param userptr FILE* for debug output
// @return        Always 0 (success)
static int32_t curl_debug_callback(CURL *handle, curl_infotype type,
                                    char *data, size_t size, void *userptr)
{
    (void)handle;  // Unused
    FILE *debug_output = (FILE *)userptr;

    // Filter out noise - only show meaningful HTTP traffic
    if (type == CURLINFO_TEXT ||
        type == CURLINFO_SSL_DATA_IN ||
        type == CURLINFO_SSL_DATA_OUT) {
        return 0;  // Ignore TLS/HTTP2 protocol details
    }

    // Prefix for direction
    const char *prefix = "";
    if (type == CURLINFO_HEADER_OUT || type == CURLINFO_DATA_OUT) {
        prefix = ">> ";  // Outgoing (request)
    } else if (type == CURLINFO_HEADER_IN || type == CURLINFO_DATA_IN) {
        prefix = "<< ";  // Incoming (response)
    }

    // Write data line-by-line to handle multi-line headers/bodies
    const char *ptr = data;
    const char *end = data + size;

    while (ptr < end) {
        // Find next newline
        const char *line_end = ptr;
        while (line_end < end && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }

        size_t line_len = line_end - ptr;

        // Check for Authorization header (case-insensitive)
        if ((type == CURLINFO_HEADER_OUT || type == CURLINFO_HEADER_IN) &&
            line_len > 14 &&
            strncasecmp(ptr, "authorization:", 14) == 0) {
            // Redact the value
            fprintf(debug_output, "%sAuthorization: [REDACTED]\n", prefix);
        } else if (line_len > 0) {
            // Print the line with prefix
            fprintf(debug_output, "%s%.*s\n", prefix, (int32_t)line_len, ptr);
        }

        // Skip past newline characters
        ptr = line_end;
        while (ptr < end && (*ptr == '\n' || *ptr == '\r')) {
            ptr++;
        }
    }

    fflush(debug_output);
    return 0;
}
```

### Step 2: Replace CURLOPT_VERBOSE with CURLOPT_DEBUGFUNCTION

In `src/openai/client_multi_request.c`, around line 105-106, replace:

```c
// OLD - verbose mode dumps everything:
curl_easy_setopt_(active_req->easy_handle, CURLOPT_VERBOSE, (const void *)1L);
curl_easy_setopt_(active_req->easy_handle, CURLOPT_STDERR, debug_output);
```

with:

```c
// NEW - custom debug callback filters noise and redacts secrets:
curl_easy_setopt_(active_req->easy_handle, CURLOPT_VERBOSE, (const void *)1L);
curl_easy_setopt_(active_req->easy_handle, CURLOPT_DEBUGFUNCTION, curl_debug_callback);
curl_easy_setopt_(active_req->easy_handle, CURLOPT_DEBUGDATA, debug_output);
```

**Note:** We still need `CURLOPT_VERBOSE=1L` to enable the debug callback. We just redirect it through our custom function instead of directly to stderr.

### Step 3: Add wrapper for strncasecmp if needed

Check if `strncasecmp` needs a wrapper in `src/wrapper.h`. If not already present, it's a standard POSIX function and can be used directly.

### Step 4: Update tests

Update debug output tests to expect:
- `>>` prefix for request headers/body
- `<<` prefix for response headers/body
- `Authorization: [REDACTED]` instead of the actual key
- No SSL/TLS/HTTP2 protocol messages

## Testing Strategy

### Unit Tests

1. **Test Authorization header redaction**
   - Mock curl to invoke debug callback with `Authorization: Bearer sk-...`
   - Verify output shows `Authorization: [REDACTED]`

2. **Test other headers pass through**
   - Mock curl to invoke debug callback with `Content-Type: application/json`
   - Verify output shows header unchanged with `>>` or `<<` prefix

3. **Test SSL data filtered**
   - Mock curl to invoke debug callback with `CURLINFO_SSL_DATA_IN`
   - Verify no output (filtered)

4. **Test informational text filtered**
   - Mock curl to invoke debug callback with `CURLINFO_TEXT` ("Trying 104...")
   - Verify no output (filtered)

### Integration Tests

1. Make a real request with `/debug on`
2. Verify output shows:
   - Request method, URL, headers (Auth redacted), body
   - Response status, headers, body
   - NO SSL/TLS handshake details
   - NO HTTP/2 frame messages

## Edge Cases

| Case | Behavior |
|------|----------|
| Authorization with different casing | Filtered (case-insensitive match) |
| Multi-line headers | Each line prefixed correctly |
| Empty lines | Preserved (show just prefix) |
| Binary data in body | Printed as-is (curl handles encoding) |
| Very long header values | Printed in full (no truncation) |

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Debug output shows clean HTTP requests/responses
- Authorization header shows `[REDACTED]`
- No SSL/TLS/HTTP2 protocol noise
- All HTTP verbs visible (POST, GET, PUT, etc.)
- All non-sensitive headers visible
