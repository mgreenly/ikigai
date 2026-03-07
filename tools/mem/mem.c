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

static int32_t do_request(void *ctx, const char *method, const char *url, const char *body)
{
    CURL *curl = curl_easy_init_();
    if (curl == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "Failed to initialize HTTP client", "ERR_IO");
        return 0;
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
        return 0;
    }

    long http_code = 0;
    curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup_(curl);

    if (http_code >= 400) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "HTTP %ld error", http_code);
        output_error(ctx, error_msg, "ERR_IO");
        return 0;
    }

    printf("%s\n", response.data);
    return 0;
}

int32_t mem_execute(void *ctx, const mem_params_t *params)
{
    const char *scheme = getenv("RALPH_REMEMBERS_SCHEME");
    const char *host = getenv("RALPH_REMEMBERS_HOST");
    const char *port_str = getenv("RALPH_REMEMBERS_PORT");
    const char *project = getenv("RALPH_REMEMBERS_PROJECT");

    if (scheme == NULL || host == NULL || port_str == NULL || project == NULL) {
        output_error(ctx,
                     "Missing required environment variables: RALPH_REMEMBERS_SCHEME, "
                     "RALPH_REMEMBERS_HOST, RALPH_REMEMBERS_PORT, RALPH_REMEMBERS_PROJECT",
                     "ERR_CONFIG");
        return 0;
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

        const char *agent = getenv("IKIGAI_AGENT_ID");
        if (agent != NULL) {
            yyjson_mut_obj_add_str(req_doc, req_obj, "agent", agent);
        }

        if (params->title != NULL) {
            yyjson_mut_obj_add_str(req_doc, req_obj, "title", params->title);
        }

        yyjson_mut_doc_set_root(req_doc, req_obj);

        char *req_json = yyjson_mut_write_opts(req_doc, 0, &allocator, NULL, NULL);
        if (req_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        char *url = talloc_asprintf(ctx, "%s/documents", base_url);
        if (url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "POST", url, req_json);
    }

    if (params->action == MEM_ACTION_GET) {
        if (params->id == NULL) {
            output_error(ctx, "id is required for get action", "ERR_PARAMS");
            return 0;
        }

        char *url = talloc_asprintf(ctx, "%s/documents/%s", base_url, params->id);
        if (url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "GET", url, NULL);
    }

    if (params->action == MEM_ACTION_LIST) {
        char *url = talloc_asprintf(ctx, "%s/documents", base_url);
        if (url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "GET", url, NULL);
    }

    if (params->action == MEM_ACTION_DELETE) {
        if (params->id == NULL) {
            output_error(ctx, "id is required for delete action", "ERR_PARAMS");
            return 0;
        }

        char *url = talloc_asprintf(ctx, "%s/documents/%s", base_url, params->id);
        if (url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        return do_request(ctx, "DELETE", url, NULL);
    }

    output_error(ctx, "Unknown action", "ERR_PARAMS");
    return 0;
}
