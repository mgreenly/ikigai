#include "mem.h"

#include "shared/version.h"
#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "shared/wrapper_curl.h"

#include "vendor/yyjson/yyjson.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#define ZERO_UUID "00000000-0000-0000-0000-000000000000"

struct response_buffer {
    char *data;
    size_t size;
    size_t capacity;
    void *ctx;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct response_buffer *buf = (struct response_buffer *)userp;

    while (buf->size + realsize + 1 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = talloc_realloc(buf->ctx, buf->data, char, (unsigned int)buf->capacity);
        if (buf->data == NULL) { // LCOV_EXCL_BR_LINE
            return 0; // LCOV_EXCL_LINE
        } // LCOV_EXCL_LINE
    }

    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write_opts(doc, 0, &allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);
}

// Perform an HTTP request and return the response body as a talloc'd string.
// On network/HTTP error, outputs a JSON error and returns NULL.
static char *do_request_raw(void *ctx, const char *method, const char *url, const char *body)
{
    CURL *curl = curl_easy_init_();
    if (curl == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "Failed to initialize HTTP client", "ERR_IO");
        return NULL;
    }

    struct response_buffer response;
    response.data = talloc_array(ctx, char, 4096);
    if (response.data == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(curl); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE
    response.size = 0;
    response.capacity = 4096;
    response.ctx = ctx;
    response.data[0] = '\0';

    struct curl_slist *headers = NULL;

    if (strcmp(method, "POST") == 0) {
        headers = curl_slist_append_(NULL, "Content-Type: application/json");
        curl_easy_setopt_(curl, CURLOPT_POST, 1L);
        curl_easy_setopt_(curl, CURLOPT_POSTFIELDS, body);
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt_(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    if (headers != NULL) {
        curl_easy_setopt_(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt_(curl, CURLOPT_URL, url);
    curl_easy_setopt_(curl, CURLOPT_USERAGENT, "ikigai/" IK_VERSION);
    curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt_(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform_(curl);

    if (headers != NULL) {
        curl_slist_free_all_(headers);
    }

    if (res != CURLE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Request failed: %s", curl_easy_strerror_(res));
        curl_easy_cleanup_(curl);
        output_error(ctx, error_msg, "ERR_IO");
        return NULL;
    }

    long http_code = 0;
    curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup_(curl);

    if (http_code >= 400) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "HTTP %ld error", http_code);
        output_error(ctx, error_msg, "ERR_IO");
        return NULL;
    }

    return response.data;
}

static int32_t do_request(void *ctx, const char *method, const char *url, const char *body)
{
    char *result = do_request_raw(ctx, method, url, body);
    if (result == NULL) {
        return 0; // error already printed
    }
    printf("%s\n", result);
    return 0;
}

// URL-encode a string for use in a query parameter value.
// Returns a talloc'd string (the caller owns it under ctx).
static char *url_encode(void *ctx, const char *input)
{
    char *escaped = curl_easy_escape(NULL, input, 0);
    if (escaped == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    char *result = talloc_strdup(ctx, escaped);
    curl_free(escaped);
    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}

int32_t mem_execute(void *ctx, const mem_params_t *params)
{
    const char *scheme = getenv("RALPH_REMEMBERS_SCHEME");
    const char *host = getenv("RALPH_REMEMBERS_HOST");
    const char *port_str = getenv("RALPH_REMEMBERS_PORT");
    const char *env_project = getenv("RALPH_REMEMBERS_PROJECT");

    if (scheme == NULL || host == NULL || port_str == NULL || env_project == NULL) {
        output_error(ctx,
                     "Missing required environment variables: RALPH_REMEMBERS_SCHEME, "
                     "RALPH_REMEMBERS_HOST, RALPH_REMEMBERS_PORT, RALPH_REMEMBERS_PROJECT",
                     "ERR_CONFIG");
        return 0;
    }

    const char *agent;
    const char *project;

    if (params->scope == MEM_SCOPE_GLOBAL) {
        agent = ZERO_UUID;
        project = ZERO_UUID;
    } else {
        agent = getenv("IKIGAI_AGENT_ID");
        project = env_project;
    }

    char *base_url = talloc_asprintf(ctx, "%s://%s:%s", scheme, host, port_str);
    if (base_url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (params->action == MEM_ACTION_CREATE) {
        if (params->body == NULL) {
            output_error(ctx, "body is required for create action", "ERR_PARAMS");
            return 0;
        }

        yyjson_alc allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *req_doc = yyjson_mut_doc_new(&allocator);
        if (req_doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        yyjson_mut_val *req_obj = yyjson_mut_obj(req_doc);
        if (req_obj == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_str(req_doc, req_obj, "body", params->body);
        yyjson_mut_obj_add_str(req_doc, req_obj, "project", project);

        if (agent != NULL) {
            yyjson_mut_obj_add_str(req_doc, req_obj, "agent", agent);
        }

        if (params->path != NULL) {
            yyjson_mut_obj_add_str(req_doc, req_obj, "title", params->path);
        }

        yyjson_mut_doc_set_root(req_doc, req_obj);

        char *req_json = yyjson_mut_write_opts(req_doc, 0, &allocator, NULL, NULL);
        if (req_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        char *url = talloc_asprintf(ctx, "%s/documents", base_url);
        if (url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "POST", url, req_json);
    }

    if (params->action == MEM_ACTION_GET) {
        if (params->path == NULL) {
            output_error(ctx, "path is required for get action", "ERR_PARAMS");
            return 0;
        }

        char *encoded_path = url_encode(ctx, params->path);
        char *encoded_project = url_encode(ctx, project);

        char *url;
        if (agent != NULL) {
            char *encoded_agent = url_encode(ctx, agent);
            url = talloc_asprintf(ctx, "%s/documents?title=%s&agent=%s&project=%s",
                                  base_url, encoded_path, encoded_agent, encoded_project);
        } else {
            url = talloc_asprintf(ctx, "%s/documents?title=%s&project=%s",
                                  base_url, encoded_path, encoded_project);
        }
        if (url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "GET", url, NULL);
    }

    if (params->action == MEM_ACTION_LIST) {
        char *encoded_project = url_encode(ctx, project);

        char *url;
        if (agent != NULL) {
            char *encoded_agent = url_encode(ctx, agent);
            url = talloc_asprintf(ctx, "%s/documents?agent=%s&project=%s",
                                  base_url, encoded_agent, encoded_project);
        } else {
            url = talloc_asprintf(ctx, "%s/documents?project=%s",
                                  base_url, encoded_project);
        }
        if (url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "GET", url, NULL);
    }

    if (params->action == MEM_ACTION_DELETE) {
        if (params->path == NULL) {
            output_error(ctx, "path is required for delete action", "ERR_PARAMS");
            return 0;
        }

        // Step 1: Look up document by path to get its UUID
        char *encoded_path = url_encode(ctx, params->path);
        char *encoded_project = url_encode(ctx, project);

        char *lookup_url;
        if (agent != NULL) {
            char *encoded_agent = url_encode(ctx, agent);
            lookup_url = talloc_asprintf(ctx, "%s/documents?title=%s&agent=%s&project=%s",
                                         base_url, encoded_path, encoded_agent, encoded_project);
        } else {
            lookup_url = talloc_asprintf(ctx, "%s/documents?title=%s&project=%s",
                                         base_url, encoded_path, encoded_project);
        }
        if (lookup_url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        char *lookup_body = do_request_raw(ctx, "GET", lookup_url, NULL);
        if (lookup_body == NULL) {
            return 0; // error already printed
        }

        // Step 2: Parse response to extract UUID
        yyjson_alc allocator = ik_make_talloc_allocator(ctx);
        yyjson_doc *resp_doc = yyjson_read_opts(lookup_body, strlen(lookup_body),
                                                0, &allocator, NULL);
        if (resp_doc == NULL) {
            output_error(ctx, "Failed to parse lookup response", "ERR_IO");
            return 0;
        }

        yyjson_val *root = yyjson_doc_get_root(resp_doc);
        yyjson_val *items = yyjson_obj_get(root, "items");
        if (items == NULL || !yyjson_is_arr(items) || yyjson_arr_size(items) == 0) {
            output_error(ctx, "Document not found", "ERR_NOT_FOUND");
            return 0;
        }

        yyjson_val *first = yyjson_arr_get_first(items);
        yyjson_val *id_val = yyjson_obj_get(first, "id");
        if (id_val == NULL || !yyjson_is_str(id_val)) {
            output_error(ctx, "Unexpected response format", "ERR_IO");
            return 0;
        }

        const char *doc_id = yyjson_get_str(id_val);

        // Step 3: Delete by UUID
        char *delete_url = talloc_asprintf(ctx, "%s/documents/%s", base_url, doc_id);
        if (delete_url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "DELETE", delete_url, NULL);
    }

    output_error(ctx, "Unknown action", "ERR_PARAMS");
    return 0;
}
