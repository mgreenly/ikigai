#ifndef IK_OPENAI_CLIENT_MULTI_INTERNAL_H
#define IK_OPENAI_CLIENT_MULTI_INTERNAL_H

#include "openai/client_multi.h"
#include "openai/client_multi_callbacks.h"
#include "vendor/yyjson/yyjson.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <talloc.h>

/**
 * Internal header for client_multi implementation
 *
 * Shared between client_multi.c and client_multi_request.c
 */

/**
 * yyjson wrapper function prototypes for testing inline vendor functions
 */
yyjson_mut_val *yyjson_mut_doc_get_root_wrapper(yyjson_mut_doc *doc);
bool yyjson_mut_obj_add_str_wrapper(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val);
bool yyjson_mut_obj_add_int_wrapper(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t val);
yyjson_mut_val *yyjson_mut_obj_add_obj_wrapper(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);

/**
 * Active request context
 *
 * Tracks state for a single in-flight HTTP request.
 */
typedef struct {
    CURL *easy_handle;                    // curl easy handle for this request
    struct curl_slist *headers;           // HTTP headers
    http_write_ctx_t *write_ctx;          // Write callback context
    char *request_body;                   // JSON request body (must persist)
    ik_http_completion_cb_t completion_cb; // Completion callback (or NULL)
    void *completion_ctx;                 // Context for completion callback
} active_request_t;

/**
 * Multi-handle manager structure
 */
struct ik_openai_multi {
    CURLM *multi_handle;                  // curl multi handle
    active_request_t **active_requests;   // Array of active request contexts
    size_t active_count;                  // Number of active requests
};

#endif /* IK_OPENAI_CLIENT_MULTI_INTERNAL_H */
