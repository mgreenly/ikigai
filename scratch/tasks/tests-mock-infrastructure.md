# Task: Create Test Mock Infrastructure

**Layer:** 2
**Model:** sonnet/thinking
**Depends on:** provider-types.md, http-client.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load style` - Code style for test files

**Source:**
- `src/wrapper.c` - Existing MOCKABLE pattern
- `src/wrapper.h` - MOCKABLE macro definition
- `tests/unit/openai/` - Existing mock patterns (if any)

**Plan:**
- `scratch/plan/testing-strategy.md` - Mock organization

## Objective

Establish the MOCKABLE infrastructure for HTTP/curl operations and create reusable fixture directories. This foundational task enables all provider tests to mock network calls consistently.

## Interface

**Files to create:**

| File | Purpose |
|------|---------|
| `src/providers/common/http_wrapper.h` | MOCKABLE curl function declarations |
| `src/providers/common/http_wrapper.c` | Default implementations calling real curl |
| `tests/helpers/mock_http.h` | Mock setup/teardown helpers |
| `tests/helpers/mock_http.c` | Mock response injection functions |

**Directories to create:**

| Directory | Purpose |
|-----------|---------|
| `tests/fixtures/responses/` | Shared response samples |
| `tests/fixtures/errors/` | Error response samples |

## Behaviors

**MOCKABLE Pattern:**

Follow existing `src/wrapper.c` pattern:

```c
// src/providers/common/http_wrapper.h
#include "wrapper.h"  // For MOCKABLE macro

MOCKABLE CURLcode curl_perform_(CURL *handle);
MOCKABLE CURLcode curl_setopt_str_(CURL *handle, CURLoption opt, const char *val);
MOCKABLE CURLcode curl_setopt_long_(CURL *handle, CURLoption opt, long val);
MOCKABLE CURLcode curl_setopt_ptr_(CURL *handle, CURLoption opt, void *val);
```

```c
// src/providers/common/http_wrapper.c
#include "http_wrapper.h"
#include <curl/curl.h>

CURLcode curl_perform_(CURL *handle) {
    return curl_easy_perform(handle);
}

CURLcode curl_setopt_str_(CURL *handle, CURLoption opt, const char *val) {
    return curl_easy_setopt(handle, opt, val);
}
// ... etc
```

**Mock Helpers:**

```c
// tests/helpers/mock_http.h
typedef struct {
    int32_t status_code;
    const char *body;
    size_t body_len;
    const char *headers;
} mock_http_response_t;

void mock_http_set_response(const mock_http_response_t *resp);
void mock_http_set_error(CURLcode error);
void mock_http_reset(void);

// For streaming tests
typedef void (*mock_chunk_callback_t)(const char *chunk, size_t len, void *userdata);
void mock_http_set_streaming(mock_chunk_callback_t cb, void *userdata);
```

```c
// tests/helpers/mock_http.c
#include "mock_http.h"

static mock_http_response_t *g_mock_response = NULL;
static CURLcode g_mock_error = CURLE_OK;

// Override the MOCKABLE functions
CURLcode curl_perform_(CURL *handle) {
    if (g_mock_error != CURLE_OK) return g_mock_error;
    // Invoke write callback with mock body...
    return CURLE_OK;
}

void mock_http_set_response(const mock_http_response_t *resp) {
    g_mock_response = (mock_http_response_t *)resp;
}
// ... etc
```

**Fixture Organization:**

Create empty directories with README:

```
tests/fixtures/
├── responses/
│   └── README.md  # "Provider response fixtures, organized by provider"
└── errors/
    └── README.md  # "Error response fixtures for testing error handling"
```

## Test Scenarios

Create one validation test to confirm mock works:

```c
// tests/unit/providers/common/test_mock_infrastructure.c
START_TEST(test_mock_http_returns_configured_response)
{
    mock_http_response_t resp = {
        .status_code = 200,
        .body = "{\"ok\":true}",
        .body_len = 11
    };
    mock_http_set_response(&resp);

    // Call HTTP client, verify it receives mock response
    // ...

    mock_http_reset();
}
END_TEST
```

## Postconditions

- [ ] `src/providers/common/http_wrapper.h` exists with MOCKABLE declarations
- [ ] `src/providers/common/http_wrapper.c` exists with real implementations
- [ ] `tests/helpers/mock_http.h` exists with mock API
- [ ] `tests/helpers/mock_http.c` exists with mock implementations
- [ ] `tests/fixtures/responses/` directory created
- [ ] `tests/fixtures/errors/` directory created
- [ ] Validation test compiles and passes
- [ ] `make check` passes
