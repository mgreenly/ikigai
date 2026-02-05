/**
 * @file request_responses_test_helpers.c
 * @brief Implementation of shared test infrastructure
 */

#include "request_responses_test_helper.h"

// Mock state
int32_t yyjson_fail_count = 0;
int32_t yyjson_call_count = 0;

// Mock implementations - these are strong symbols that override the weak ones in wrapper_json.c
bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, const char *val)
{
    yyjson_call_count++;
    if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
        return false;
    }
    // Call real implementation
    return yyjson_mut_obj_add_str(doc, obj, key, val);
}

bool yyjson_mut_obj_add_val_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, yyjson_mut_val *val)
{
    yyjson_call_count++;
    if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
        return false;
    }
    // Call real implementation
    return yyjson_mut_obj_add_val(doc, obj, key, val);
}

bool yyjson_mut_obj_add_bool_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                              const char *key, bool val)
{
    yyjson_call_count++;
    if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
        return false;
    }
    // Call real implementation
    return yyjson_mut_obj_add_bool(doc, obj, key, val);
}

bool yyjson_mut_arr_add_val_(yyjson_mut_val *arr, yyjson_mut_val *val)
{
    yyjson_call_count++;
    if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
        return false;
    }
    // Call real implementation
    return yyjson_mut_arr_add_val(arr, val);
}

// Test fixture
TALLOC_CTX *test_ctx;

void request_responses_setup(void)
{
    test_ctx = talloc_new(NULL);
}

void request_responses_teardown(void)
{
    talloc_free(test_ctx);
}
