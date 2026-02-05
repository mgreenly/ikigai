/**
 * @file request_responses_test_helpers.h
 * @brief Shared test infrastructure for OpenAI Responses API tests
 *
 * Provides mock helpers and common fixtures for all request_responses tests.
 */

#ifndef REQUEST_RESPONSES_TEST_HELPERS_H
#define REQUEST_RESPONSES_TEST_HELPERS_H

#include "apps/ikigai/providers/openai/request.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"

// CRITICAL: Do NOT include wrapper.h here to avoid weak symbol conflicts
// The test helpers provide strong symbol overrides for yyjson wrappers
// #include "shared/wrapper.h"

#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Test fixture context
extern TALLOC_CTX *test_ctx;

// Setup and teardown functions
void request_responses_setup(void);
void request_responses_teardown(void);

// Mock state for yyjson failures
extern int32_t yyjson_fail_count;
extern int32_t yyjson_call_count;

// Mock yyjson wrappers - these override the weak symbols from wrapper_json.c
bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val);
bool yyjson_mut_obj_add_val_(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, yyjson_mut_val *val);
bool yyjson_mut_obj_add_bool_(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool val);
bool yyjson_mut_arr_add_val_(yyjson_mut_val *arr, yyjson_mut_val *val);

#endif // REQUEST_RESPONSES_TEST_HELPERS_H
