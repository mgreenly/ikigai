/**
 * @file equivalence_compare_basic.c
 * @brief Basic comparison functions (token usage, JSON)
 */

#include "equivalence_compare.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>
#include <math.h>

/* ================================================================
 * Token Usage Comparison
 * ================================================================ */

bool ik_compare_token_usage_tolerant(int32_t a, int32_t b)
{
    /* Handle zero cases */
    if (a == 0 && b == 0) {
        return true;
    }

    /* If one is zero and the other isn't, they don't match */
    if (a == 0 || b == 0) {
        return false;
    }

    /* Calculate percentage difference */
    int32_t diff = abs(a - b);
    int32_t max_val = (a > b) ? a : b;

    /* Allow 5% tolerance */
    double tolerance = 0.05;
    double diff_ratio = (double)diff / (double)max_val;

    return diff_ratio <= tolerance;
}

/* ================================================================
 * JSON Comparison
 * ================================================================ */

/**
 * Compare two yyjson values for semantic equivalence
 *
 * Recursively compares JSON values ignoring key order.
 */
static bool compare_json_values(yyjson_val *a, yyjson_val *b)
{
    /* Type must match */
    yyjson_type type_a = yyjson_get_type(a);
    yyjson_type type_b = yyjson_get_type(b);

    if (type_a != type_b) {
        return false;
    }

    switch (type_a) {
        case YYJSON_TYPE_NULL:
            return true;

        case YYJSON_TYPE_BOOL:
            return yyjson_get_bool(a) == yyjson_get_bool(b);

        case YYJSON_TYPE_NUM:
            /* Compare numbers with small tolerance for floating point */
            if (yyjson_is_int(a) && yyjson_is_int(b)) {
                return yyjson_get_int(a) == yyjson_get_int(b);
            }
            return fabs(yyjson_get_real(a) - yyjson_get_real(b)) < 0.0001;

        case YYJSON_TYPE_STR:
            return strcmp(yyjson_get_str(a), yyjson_get_str(b)) == 0;

        case YYJSON_TYPE_ARR: {
            size_t len_a = yyjson_arr_size(a);
            size_t len_b = yyjson_arr_size(b);

            if (len_a != len_b) {
                return false;
            }

            /* Compare elements in order */
            yyjson_arr_iter iter_a;
            yyjson_arr_iter iter_b;
            yyjson_arr_iter_init(a, &iter_a);
            yyjson_arr_iter_init(b, &iter_b);

            for (size_t i = 0; i < len_a; i++) {
                yyjson_val *elem_a = yyjson_arr_iter_next(&iter_a);
                yyjson_val *elem_b = yyjson_arr_iter_next(&iter_b);

                if (!compare_json_values(elem_a, elem_b)) {
                    return false;
                }
            }

            return true;
        }

        case YYJSON_TYPE_OBJ: {
            size_t len_a = yyjson_obj_size(a);
            size_t len_b = yyjson_obj_size(b);

            if (len_a != len_b) {
                return false;
            }

            /* For each key in a, find matching key in b and compare values */
            yyjson_obj_iter iter;
            yyjson_obj_iter_init(a, &iter);

            yyjson_val *key_a, *val_a;
            while ((key_a = yyjson_obj_iter_next(&iter))) {
                val_a = yyjson_obj_iter_get_val(key_a);
                const char *key_str = yyjson_get_str(key_a);

                /* Find same key in b */
                yyjson_val *val_b = yyjson_obj_get(b, key_str);
                if (val_b == NULL) {
                    return false;  /* Key exists in a but not in b */
                }

                /* Compare values */
                if (!compare_json_values(val_a, val_b)) {
                    return false;
                }
            }

            return true;
        }

        default:
            return false;
    }
}

ik_compare_result_t *ik_compare_json_equivalent(TALLOC_CTX *ctx,
                                                const char *json_a,
                                                const char *json_b)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(json_a != NULL);  // LCOV_EXCL_BR_LINE
    assert(json_b != NULL);  // LCOV_EXCL_BR_LINE

    ik_compare_result_t *result = talloc_zero(ctx, ik_compare_result_t);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Parse both JSON strings */
    yyjson_doc *doc_a = yyjson_read(json_a, strlen(json_a), 0);
    if (doc_a == NULL) {
        result->matches = false;
        result->diff_message = talloc_strdup(result, "Failed to parse json_a");
        return result;
    }

    yyjson_doc *doc_b = yyjson_read(json_b, strlen(json_b), 0);
    if (doc_b == NULL) {
        yyjson_doc_free(doc_a);
        result->matches = false;
        result->diff_message = talloc_strdup(result, "Failed to parse json_b");
        return result;
    }

    /* Compare root values */
    yyjson_val *root_a = yyjson_doc_get_root(doc_a);
    yyjson_val *root_b = yyjson_doc_get_root(doc_b);

    bool matches = compare_json_values(root_a, root_b);

    yyjson_doc_free(doc_a);
    yyjson_doc_free(doc_b);

    result->matches = matches;
    result->diff_message = matches ? NULL : talloc_strdup(result, "JSON values differ");

    return result;
}
