/**
 * @file repl_streaming_test_common.h
 * @brief Common mock infrastructure for REPL streaming and completion tests
 */

#ifndef IK_REPL_STREAMING_TEST_COMMON_H
#define IK_REPL_STREAMING_TEST_COMMON_H

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <curl/curl.h>
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/repl_actions.h"
#include "../../../src/repl_event_handlers.h"
#include "../../../src/render.h"
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../../src/openai/client_multi.h"
#include "../../test_utils.h"

// Forward declarations
ssize_t posix_write_(int fd, const void *buf, size_t count);
typedef size_t (*curl_write_callback)(char *data, size_t size, size_t nmemb, void *userdata);

// Mock curl function prototypes (strong symbols to override wrapper.c weak symbols)
CURL *curl_easy_init_(void);
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val);
CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles);

// Global state for curl mocking
extern curl_write_callback g_write_callback;
extern void *g_write_data;
extern const char *mock_response_data;
extern size_t mock_response_len;
extern bool invoke_write_callback;
extern CURL *g_last_easy_handle;
extern bool simulate_completion;
extern bool mock_write_should_fail;

// Helper function to create a REPL context with all LLM components
ik_repl_ctx_t *create_test_repl_with_llm(void *ctx);

#endif // IK_REPL_STREAMING_TEST_COMMON_H
