# Task: VCR Mock Integration - Curl Hooks and Write Callback

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** provider-types.md, vcr-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical VCR Architecture Constraint

VCR operates at the HTTP layer, intercepting curl_multi operations to record/replay raw HTTP traffic. It is **provider-agnostic** - it knows nothing about SSE, NDJSON, JSON, or specific provider formats. VCR simply:

- **In playback mode:** Reads JSONL fixture files and delivers chunks to curl write callbacks
- **In record mode:** Captures data from write callbacks and writes to JSONL fixture files

This integration layer hooks VCR into the existing MOCKABLE curl wrapper infrastructure.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `tests/helpers/vcr.h` exists with core VCR API (vcr-core.md Phase 1 complete)
- [ ] `tests/helpers/vcr.c` exists with:
  - `vcr_is_recording()` - mode detection
  - `vcr_next_chunk()` - get next chunk from fixture
  - `vcr_has_more()` - check if more chunks available
  - `vcr_record_chunk()` - write chunk to fixture
  - `vcr_record_response()` - write response metadata

## Pre-Read

**VCR Core (already implemented):**
- `tests/helpers/vcr.h` - Core VCR API
- `tests/helpers/vcr.c` - JSONL parsing, chunk queue, record functions

**Curl Wrapper Infrastructure:**
- `src/wrapper.h` - MOCKABLE curl function declarations
- `src/wrapper.c` - Curl wrapper implementations (if exists)
- Search for `curl_multi_perform_` implementation to find mock hook point
- Search for write callback handling in curl wrappers

**Design Context:**
- `scratch/plan/vcr-cassettes.md` - Architecture section: "Mock Integration"
- `scratch/vcr-tasks.md` - Phase 2: Tasks 2.1-2.2

## Objective

Integrate VCR with the existing curl mock infrastructure to enable:
1. **Playback mode:** VCR delivers fixture chunks through curl write callbacks
2. **Record mode:** VCR captures write callback data to fixture files

This completes the VCR recording/playback pipeline.

## Phase 2.1: Curl Mock VCR Hooks

### Locate Mock Hook Points

Find where curl_multi_perform_ is implemented (likely in `src/wrapper.c` or test helpers):

```c
CURLMcode curl_multi_perform_(CURLM *multi_handle, int *running_handles)
{
    // Existing mock implementation here
    // Need to add VCR hook
}
```

### Playback Hook Implementation

Add VCR playback logic to `curl_multi_perform_`:

```c
CURLMcode curl_multi_perform_(CURLM *multi_handle, int *running_handles)
{
    // Check if VCR is active and in playback mode
    if (!vcr_is_recording() && vcr_is_active()) {
        // Get next chunk from fixture
        const char *chunk_data;
        size_t chunk_len;

        if (vcr_next_chunk(&chunk_data, &chunk_len)) {
            // Deliver chunk to registered write callback
            // (see Phase 2.2 for callback delivery mechanism)
            deliver_vcr_chunk_to_callback(chunk_data, chunk_len);

            // Update running handles based on whether more data available
            *running_handles = vcr_has_more() ? 1 : 0;
        } else {
            // No more data
            *running_handles = 0;
        }

        return CURLM_OK;
    }

    // Fall through to real curl or existing mock behavior
    return real_curl_multi_perform(multi_handle, running_handles);
}
```

### Key Behaviors

- **Mode detection:** Check `vcr_is_recording()` and `vcr_is_active()` at top of perform
- **Chunk delivery:** Call `vcr_next_chunk()` to get next chunk from fixture
- **Running handles:** Set to 1 if `vcr_has_more()` is true, 0 otherwise
- **Return codes:** Always return `CURLM_OK` in playback mode
- **No network:** Playback mode never calls real curl functions

## Phase 2.2: Write Callback Integration

### Playback: Deliver Chunks to Write Callback

Implement mechanism to invoke curl write callback with VCR data:

```c
// Internal helper function
static void deliver_vcr_chunk_to_callback(const char *chunk_data, size_t chunk_len)
{
    // Get write callback and user context from curl easy handle
    curl_write_callback write_cb;
    void *write_ctx;

    // Extract from easy handle (likely stored in mock state)
    // curl_easy_getinfo_() or internal tracking structure

    if (write_cb) {
        // Invoke callback exactly as curl would
        write_cb(chunk_data, chunk_len, 1, write_ctx);
    }
}
```

### Record: Capture Write Callback Data

Wrap or intercept write callback to capture data:

```c
// Wrapper around user's write callback
static size_t vcr_write_callback_wrapper(
    char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t realsize = size * nmemb;

    // In record mode, capture this chunk
    if (vcr_is_recording()) {
        vcr_record_chunk(ptr, realsize);
    }

    // Call user's original callback
    vcr_write_callback_ctx_t *ctx = (vcr_write_callback_ctx_t *)userdata;
    return ctx->original_callback(ptr, size, nmemb, ctx->original_userdata);
}
```

### Hook Write Callback Registration

Intercept `curl_easy_setopt_` when setting `CURLOPT_WRITEFUNCTION`:

```c
CURLcode curl_easy_setopt_(CURL *handle, CURLoption option, ...)
{
    if (option == CURLOPT_WRITEFUNCTION && vcr_is_recording()) {
        // Store user's callback
        // Register vcr_write_callback_wrapper instead
        // Pass wrapped context with original callback + userdata
    }

    // Call real setopt
    return real_curl_easy_setopt(handle, option, ...);
}
```

### HTTP Status Handling

VCR must expose response status from `_response` JSONL line:

```c
// In vcr.c (add to Phase 1)
int vcr_get_response_status(void)
{
    // Return HTTP status code from current _response line
    // e.g., 200, 404, 500
}

// In curl mock integration
if (!vcr_is_recording() && vcr_is_active()) {
    // Before delivering chunks, set HTTP status on easy handle
    long status = vcr_get_response_status();
    // Store for later retrieval via curl_easy_getinfo_(CURLINFO_RESPONSE_CODE)
}
```

## Integration Points

### Files to Modify

| File | Changes |
|------|---------|
| `src/wrapper.c` (or equivalent) | Add VCR hooks to `curl_multi_perform_` |
| `src/wrapper.c` | Add VCR hooks to `curl_easy_setopt_` for write callback capture |
| `tests/helpers/vcr.h` | Add `vcr_is_active()`, `vcr_get_response_status()` declarations |
| `tests/helpers/vcr.c` | Implement `vcr_is_active()`, `vcr_get_response_status()` |

### Mock State Management

The curl mock infrastructure likely maintains state for active requests. VCR integration needs:

- **Write callback tracking:** Map easy handles to their write callbacks
- **Context preservation:** Store user's original callback and context for record mode
- **Status storage:** Store HTTP status from VCR for retrieval via getinfo

If this state structure doesn't exist, create it:

```c
typedef struct {
    CURL *easy_handle;
    curl_write_callback user_callback;
    void *user_context;
    long http_status;  // from VCR in playback mode
} vcr_curl_state_t;
```

## Memory Management

- **Chunk data from VCR:** Owned by VCR, only valid until next `vcr_next_chunk()` call
- **Write callback wrapper context:** Allocated with talloc, tied to easy handle lifetime
- **Callback copies:** If data needs to persist, write callback must copy it (normal curl behavior)

## Error Handling

- **Missing write callback:** Log warning, skip delivery (don't crash)
- **VCR errors:** If `vcr_next_chunk()` returns NULL unexpectedly, log error and set running_handles=0
- **Record failures:** If `vcr_record_chunk()` fails, log warning but don't fail the HTTP call

## Test Scenarios

Create `tests/unit/helpers/vcr_mock_integration_test.c`:

**Playback tests:**
- Playback delivers chunks: VCR fixture data reaches write callback in correct order
- Playback sets running handles: running_handles reflects vcr_has_more() state
- Playback provides status: HTTP status from fixture available via getinfo
- Playback completes: curl_multi_info_read detects completion after last chunk

**Record tests:**
- Record captures chunks: Write callback data written to fixture via vcr_record_chunk()
- Record preserves callback: User's original callback still invoked
- Record handles multiple chunks: Multiple callback invocations produce multiple _chunk lines

**Mode tests:**
- Real curl when VCR inactive: Normal curl behavior when vcr_is_active() is false
- Record mode uses real curl: curl_multi_perform_ calls real curl in record mode

## Makefile Updates

Add to Makefile:
- `tests/unit/helpers/vcr_mock_integration_test.c` to TEST_SOURCES
- Ensure VCR helpers linked to test binaries

## Postconditions

- [ ] `curl_multi_perform_` has VCR playback hook (deliver chunks from fixture)
- [ ] `curl_easy_setopt_` has VCR record hook (capture write callback data)
- [ ] `vcr_is_active()` function implemented to check if VCR initialized
- [ ] `vcr_get_response_status()` function implemented to get HTTP status
- [ ] Write callback wrapper implemented for record mode
- [ ] Playback mode delivers chunks to write callbacks without network
- [ ] Record mode captures write callback data to fixture
- [ ] HTTP status from VCR available via curl_easy_getinfo_(CURLINFO_RESPONSE_CODE)
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] Compiles without warnings
- [ ] Changes committed to git with message: `task: vcr-mock-integration.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).

## Notes

- This task integrates VCR with curl mocks, not with provider code
- Provider code remains unchanged - providers use normal curl_multi operations
- VCR intercepts at the curl layer, transparent to providers
- After this task, tests can use `vcr_init()` + `vcr_finish()` for automated record/replay
