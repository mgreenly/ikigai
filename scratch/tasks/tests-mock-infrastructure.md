# Task: Create Test Mock Infrastructure for Async curl_multi

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** provider-types.md, http-client.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking via curl_multi (NOT curl_easy). See `scratch/plan/README.md`.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load style` - Code style for test files

**Source:**
- `src/wrapper.h` - MOCKABLE macro definition (includes wrapper_curl.h)
- `src/wrapper.c` - Existing MOCKABLE implementations for curl_multi_* functions
- `src/openai/client_multi.c` - Reference: existing async curl_multi usage pattern

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop
- `scratch/plan/testing-strategy.md` - Mock HTTP pattern (async), test flow examples
- `scratch/plan/architecture.md` - MOCKABLE pattern reference, event loop integration

## Objective

Create mock infrastructure for testing async HTTP operations via curl_multi. The mock must simulate the fdset/perform/info_read cycle that integrates with select()-based event loops, NOT blocking curl_easy_perform.

**Key insight:** MOCKABLE wrappers for curl_multi already exist in `src/wrapper.c`:
- `curl_multi_init_`, `curl_multi_cleanup_`
- `curl_multi_add_handle_`, `curl_multi_remove_handle_`
- `curl_multi_perform_`, `curl_multi_fdset_`, `curl_multi_timeout_`
- `curl_multi_info_read_`, `curl_multi_strerror_`

This task creates the test helper layer that overrides these existing wrappers.

## Interface

**Files to create:**

| File | Purpose |
|------|---------|
| `tests/helpers/mock_curl_multi.h` | Mock curl_multi state machine and setup helpers |
| `tests/helpers/mock_curl_multi.c` | Mock implementations that override MOCKABLE curl_multi_ wrappers |

**Directories to create:**

| Directory | Purpose |
|-----------|---------|
| `tests/fixtures/responses/` | Shared response samples |
| `tests/fixtures/errors/` | Error response samples |

## Behaviors

### Mock curl_multi State Machine

The mock must simulate the async event loop cycle:

1. **fdset phase** - `curl_multi_fdset_()` returns mock FDs (or -1 for no activity)
2. **perform phase** - `curl_multi_perform_()` simulates progress, delivers data to write callbacks
3. **info_read phase** - `curl_multi_info_read_()` returns completion messages

**Mock State Structure:**

```c
// tests/helpers/mock_curl_multi.h
#ifndef MOCK_CURL_MULTI_H
#define MOCK_CURL_MULTI_H

#include <curl/curl.h>
#include <inttypes.h>
#include <stdbool.h>

// Mock transfer state
typedef struct {
    const char *response_body;     // Full response body
    size_t response_len;           // Response body length
    size_t bytes_delivered;        // Bytes already delivered to write callback
    size_t chunk_size;             // Bytes to deliver per perform() call
    int32_t http_status;           // HTTP status code
    CURLcode result;               // Final transfer result
    bool complete;                 // Transfer finished
} mock_transfer_t;

// Configure mock for a request
void mock_curl_multi_reset(void);
void mock_curl_multi_set_response(int32_t status, const char *body, size_t len);
void mock_curl_multi_set_streaming_response(int32_t status, const char *body,
                                             size_t len, size_t chunk_size);
void mock_curl_multi_set_error(CURLcode error);

// Control mock FD behavior
void mock_curl_multi_set_fd(int fd);  // FD to return from fdset (-1 = no FDs)

// Query mock state
bool mock_curl_multi_transfer_complete(void);
size_t mock_curl_multi_bytes_delivered(void);

#endif // MOCK_CURL_MULTI_H
```

**Mock Implementation Pattern:**

```c
// tests/helpers/mock_curl_multi.c
#include "mock_curl_multi.h"
#include "wrapper.h"  // For MOCKABLE

// Global mock state
static mock_transfer_t g_mock_transfer = {0};
static int g_mock_fd = -1;
static CURL *g_mock_easy = NULL;
static curl_write_callback g_write_callback = NULL;
static void *g_write_userdata = NULL;

void mock_curl_multi_reset(void)
{
    memset(&g_mock_transfer, 0, sizeof(g_mock_transfer));
    g_mock_fd = -1;
    g_mock_easy = NULL;
    g_write_callback = NULL;
    g_write_userdata = NULL;
}

void mock_curl_multi_set_response(int32_t status, const char *body, size_t len)
{
    g_mock_transfer.http_status = status;
    g_mock_transfer.response_body = body;
    g_mock_transfer.response_len = len;
    g_mock_transfer.chunk_size = len;  // Deliver all at once (non-streaming)
    g_mock_transfer.result = CURLE_OK;
}

void mock_curl_multi_set_streaming_response(int32_t status, const char *body,
                                             size_t len, size_t chunk_size)
{
    g_mock_transfer.http_status = status;
    g_mock_transfer.response_body = body;
    g_mock_transfer.response_len = len;
    g_mock_transfer.chunk_size = chunk_size;  // Deliver incrementally
    g_mock_transfer.result = CURLE_OK;
}

void mock_curl_multi_set_error(CURLcode error)
{
    g_mock_transfer.result = error;
    g_mock_transfer.complete = true;
}

void mock_curl_multi_set_fd(int fd)
{
    g_mock_fd = fd;
}

// Override MOCKABLE wrappers

CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set,
                            fd_set *write_fd_set, fd_set *exc_fd_set,
                            int *max_fd)
{
    (void)multi;
    (void)write_fd_set;
    (void)exc_fd_set;

    if (g_mock_fd >= 0 && !g_mock_transfer.complete) {
        FD_SET(g_mock_fd, read_fd_set);
        if (*max_fd < g_mock_fd) {
            *max_fd = g_mock_fd;
        }
    } else {
        *max_fd = -1;  // No FDs to wait on
    }
    return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    (void)multi;

    if (g_mock_transfer.complete) {
        *running_handles = 0;
        return CURLM_OK;
    }

    // Deliver next chunk to write callback
    if (g_write_callback != NULL && g_mock_transfer.response_body != NULL) {
        size_t remaining = g_mock_transfer.response_len - g_mock_transfer.bytes_delivered;
        size_t to_deliver = (remaining < g_mock_transfer.chunk_size)
                            ? remaining : g_mock_transfer.chunk_size;

        if (to_deliver > 0) {
            const char *chunk = g_mock_transfer.response_body + g_mock_transfer.bytes_delivered;
            g_write_callback((char *)chunk, 1, to_deliver, g_write_userdata);
            g_mock_transfer.bytes_delivered += to_deliver;
        }
    }

    // Check if transfer complete
    if (g_mock_transfer.bytes_delivered >= g_mock_transfer.response_len) {
        g_mock_transfer.complete = true;
        *running_handles = 0;
    } else {
        *running_handles = 1;
    }

    return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue)
{
    (void)multi;
    static CURLMsg msg;

    if (g_mock_transfer.complete && g_mock_easy != NULL) {
        msg.msg = CURLMSG_DONE;
        msg.easy_handle = g_mock_easy;
        msg.data.result = g_mock_transfer.result;
        *msgs_in_queue = 0;
        g_mock_easy = NULL;  // Only return once
        return &msg;
    }

    *msgs_in_queue = 0;
    return NULL;
}

CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout)
{
    (void)multi;
    *timeout = g_mock_transfer.complete ? -1 : 0;  // 0 = call perform immediately
    return CURLM_OK;
}

CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy)
{
    (void)multi;
    g_mock_easy = easy;
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle_(CURLM *multi, CURL *easy)
{
    (void)multi;
    (void)easy;
    g_mock_easy = NULL;
    return CURLM_OK;
}

// Capture write callback from curl_easy_setopt
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
    (void)curl;
    if (opt == CURLOPT_WRITEFUNCTION) {
        g_write_callback = (curl_write_callback)val;
    } else if (opt == CURLOPT_WRITEDATA) {
        g_write_userdata = (void *)val;
    }
    return CURLE_OK;
}

bool mock_curl_multi_transfer_complete(void)
{
    return g_mock_transfer.complete;
}

size_t mock_curl_multi_bytes_delivered(void)
{
    return g_mock_transfer.bytes_delivered;
}
```

### Test Harness Pattern

Tests must simulate the fdset/perform/info_read cycle:

```c
// tests/unit/providers/common/test_mock_curl_multi.c
#include <check.h>
#include "mock_curl_multi.h"

START_TEST(test_mock_delivers_full_response)
{
    const char *body = "{\"ok\":true}";
    mock_curl_multi_reset();
    mock_curl_multi_set_response(200, body, strlen(body));

    // Create provider/client that uses curl_multi...
    // ik_provider_t *provider = ...;

    // Simulate event loop
    int running = 1;
    while (running > 0) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = -1;

        // Phase 1: Get FDs
        curl_multi_fdset_(NULL, &read_fds, NULL, NULL, &max_fd);

        // Phase 2: Would normally select() here, but mock returns immediately

        // Phase 3: Process I/O
        curl_multi_perform_(NULL, &running);
    }

    // Phase 4: Check completion
    int msgs;
    CURLMsg *msg = curl_multi_info_read_(NULL, &msgs);
    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->msg, CURLMSG_DONE);
    ck_assert_int_eq(msg->data.result, CURLE_OK);

    // Verify response was delivered
    ck_assert(mock_curl_multi_transfer_complete());
    ck_assert_int_eq(mock_curl_multi_bytes_delivered(), strlen(body));
}
END_TEST

START_TEST(test_mock_streaming_delivers_chunks)
{
    const char *sse_data =
        "event: message_start\n"
        "data: {\"type\":\"message_start\"}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"text_delta\",\"text\":\"Hello\"}\n\n";
    size_t chunk_size = 32;  // Deliver in small chunks

    mock_curl_multi_reset();
    mock_curl_multi_set_streaming_response(200, sse_data, strlen(sse_data), chunk_size);

    int running = 1;
    int perform_calls = 0;
    while (running > 0) {
        curl_multi_perform_(NULL, &running);
        perform_calls++;
    }

    // Should have taken multiple perform() calls
    ck_assert_int_gt(perform_calls, 1);
    ck_assert(mock_curl_multi_transfer_complete());
}
END_TEST

START_TEST(test_mock_error_returns_failure)
{
    mock_curl_multi_reset();
    mock_curl_multi_set_error(CURLE_COULDNT_CONNECT);

    int msgs;
    CURLMsg *msg = curl_multi_info_read_(NULL, &msgs);
    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->data.result, CURLE_COULDNT_CONNECT);
}
END_TEST
```

### Fixture Organization

Create empty directories with README:

```
tests/fixtures/
├── responses/
│   └── README.md  # "Provider response fixtures, organized by provider"
└── errors/
    └── README.md  # "Error response fixtures for testing error handling"
```

## Live Validation (Makefile Updates)

The existing `make verify-mocks` target validates fixtures against real APIs. Update it to use the new credentials location:

**Current** (reads from old config.json):
```makefile
CONFIG_FILE="$$HOME/.config/ikigai/config.json"
API_KEY=$$(jq -r '.openai_api_key' "$$CONFIG_FILE")
```

**Updated** (reads from credentials.json):
```makefile
CREDS_FILE="$$HOME/.config/ikigai/credentials.json"
# Extract per-provider keys
OPENAI_KEY=$$(jq -r '.openai.api_key // empty' "$$CREDS_FILE")
ANTHROPIC_KEY=$$(jq -r '.anthropic.api_key // empty' "$$CREDS_FILE")
GOOGLE_KEY=$$(jq -r '.google.api_key // empty' "$$CREDS_FILE")
```

**IMPORTANT:** The credentials file `~/.config/ikigai/credentials.json` already exists with valid API keys for all three providers. The Makefile should READ from this file to set environment variables for live tests.

Add new targets for each provider:
```makefile
verify-mocks-openai:    # Verify OpenAI fixtures
verify-mocks-anthropic: # Verify Anthropic fixtures
verify-mocks-google:    # Verify Google fixtures
verify-mocks-all:       # Verify all provider fixtures
```

## Postconditions

- [ ] `tests/helpers/mock_curl_multi.h` exists with mock state machine API
- [ ] `tests/helpers/mock_curl_multi.c` exists with MOCKABLE override implementations
- [ ] Mock correctly overrides existing `curl_multi_*` wrappers from `src/wrapper.c`
- [ ] Mock simulates fdset/perform/info_read cycle (NOT blocking curl_easy_perform)
- [ ] Mock supports streaming (incremental chunk delivery via perform())
- [ ] Mock supports error injection (via set_error)
- [ ] `tests/fixtures/responses/` directory created
- [ ] `tests/fixtures/errors/` directory created
- [ ] Validation tests compile and pass:
  - `test_mock_delivers_full_response`
  - `test_mock_streaming_delivers_chunks`
  - `test_mock_error_returns_failure`
- [ ] Makefile `verify-mocks` updated to read from `~/.config/ikigai/credentials.json`
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-mock-infrastructure.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).