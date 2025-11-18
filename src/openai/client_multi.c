#include "openai/client_multi.h"
#include "openai/client.h"
#include "openai/sse_parser.h"
#include "error.h"
#include "wrapper.h"
#include "json_allocator.h"
#include "vendor/yyjson/yyjson.h"
#include <talloc.h>
#include <string.h>
#include <assert.h>
#include <curl/curl.h>
#include <sys/select.h>

/**
 * OpenAI multi-handle client implementation
 *
 * Provides non-blocking HTTP client using curl_multi interface.
 */

/*
 * Context for HTTP write callback
 *
 * Accumulates response data and handles streaming via SSE parser.
 */
typedef struct {
    ik_openai_sse_parser_t *parser;     /* SSE parser for streaming responses */
    ik_openai_stream_cb_t user_callback; /* User's streaming callback */
    void *user_ctx;                      /* User's callback context */
    char *complete_response;             /* Accumulated complete response */
    size_t response_len;                 /* Length of complete response */
    bool has_error;                      /* Whether an error occurred */
} http_write_ctx_t;

/*
 * libcurl write callback
 *
 * Called by libcurl as data arrives from the server.
 * Feeds data to SSE parser and invokes user callback for each content chunk.
 */
static size_t http_write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
    http_write_ctx_t *ctx = (http_write_ctx_t *)userdata;
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    size_t total_size = size * nmemb;

    /* Feed data to SSE parser */
    ik_openai_sse_parser_feed(ctx->parser, data, total_size);

    /* Extract and process all complete SSE events */
    while (true) {
        char *event = ik_openai_sse_parser_get_event(ctx->parser);
        if (event == NULL) {
            break; /* No more complete events */
        }

        /* Parse SSE event to extract content */
        res_t content_res = ik_openai_parse_sse_event(ctx->parser, event);
        if (content_res.is_err) {
            /* Parse error - log but continue */
            talloc_free(event);
            continue;
        }

        char *content = content_res.ok;
        if (content != NULL) {
            /* Invoke user's streaming callback if provided */
            if (ctx->user_callback != NULL) {
                res_t cb_res = ctx->user_callback(content, ctx->user_ctx);
                if (cb_res.is_err) {
                    ctx->has_error = true;
                    talloc_free(content);
                    talloc_free(event);
                    return 0;
                }
            }

            /* Accumulate to complete response */
            if (ctx->complete_response == NULL) {
                ctx->complete_response = talloc_strdup(ctx->parser, content);
            } else {
                ctx->complete_response = talloc_strdup_append(ctx->complete_response, content);
            }

            if (ctx->complete_response == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to accumulate response");  // LCOV_EXCL_LINE
            }

            talloc_free(content);
        }

        talloc_free(event);
    }

    return total_size;
}

/**
 * Active request context
 *
 * Tracks state for a single in-flight HTTP request.
 */
typedef struct {
    CURL *easy_handle;                    /* curl easy handle for this request */
    struct curl_slist *headers;           /* HTTP headers */
    http_write_ctx_t *write_ctx;          /* Write callback context */
    char *request_body;                   /* JSON request body (must persist) */
} active_request_t;

/**
 * Multi-handle manager structure
 */
struct ik_openai_multi {
    CURLM *multi_handle;                  /* curl multi handle */
    active_request_t **active_requests;   /* Array of active request contexts */
    size_t active_count;                  /* Number of active requests */
};

/**
 * Destructor for multi-handle manager
 *
 * Cleans up curl multi handle and any remaining active requests.
 */
static int multi_destructor(ik_openai_multi_t *multi) {
    /* Clean up any remaining active requests */
    for (size_t i = 0; i < multi->active_count; i++) {
        active_request_t *req = multi->active_requests[i];
        curl_multi_remove_handle_(multi->multi_handle, req->easy_handle);
        curl_easy_cleanup_(req->easy_handle);
        curl_slist_free_all_(req->headers);
        /* talloc will free req and its children */
    }

    /* Clean up multi handle */
    if (multi->multi_handle != NULL) {  // LCOV_EXCL_BR_LINE
        curl_multi_cleanup_(multi->multi_handle);
    }

    return 0;  /* Success */
}

res_t ik_openai_multi_create(void *parent) {
    ik_openai_multi_t *multi = talloc_zero(parent, ik_openai_multi_t);
    if (multi == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate multi-handle manager");  // LCOV_EXCL_LINE
    }

    multi->multi_handle = curl_multi_init_();
    if (multi->multi_handle == NULL) {
        talloc_free(multi);
        return ERR(parent, IO, "Failed to initialize curl multi handle");
    }

    multi->active_requests = NULL;
    multi->active_count = 0;

    /* Register destructor */
    talloc_set_destructor(multi, multi_destructor);

    return OK(multi);
}

res_t ik_openai_multi_add_request(ik_openai_multi_t *multi,
                                   const ik_cfg_t *cfg,
                                   ik_openai_conversation_t *conv,
                                   ik_openai_stream_cb_t stream_cb,
                                   void *cb_ctx) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);  // LCOV_EXCL_BR_LINE
    assert(conv != NULL);  // LCOV_EXCL_BR_LINE

    /* Validate inputs */
    if (conv->message_count == 0) {
        return ERR(multi, INVALID_ARG, "Conversation must contain at least one message");
    }

    if (cfg->openai_api_key == NULL || strlen(cfg->openai_api_key) == 0) {
        return ERR(multi, INVALID_ARG, "OpenAI API key is required");
    }

    /* Create request */
    ik_openai_request_t *request = ik_openai_request_create(multi, cfg, conv);

    /* Serialize request to JSON */
    char *json_body = ik_openai_serialize_request(multi, request);

    /* Create active request context */
    active_request_t *active_req = talloc_zero(multi, active_request_t);
    if (active_req == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate active request");  // LCOV_EXCL_LINE
    }

    /* Keep request body alive for the duration of the request */
    active_req->request_body = talloc_steal(active_req, json_body);

    /* Initialize libcurl easy handle */
    active_req->easy_handle = curl_easy_init_();
    if (active_req->easy_handle == NULL) {
        talloc_free(active_req);
        return ERR(multi, IO, "Failed to initialize curl easy handle");
    }

    /* Create write callback context */
    active_req->write_ctx = talloc_zero(active_req, http_write_ctx_t);
    if (active_req->write_ctx == NULL) {  // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        PANIC("Failed to allocate write context");  // LCOV_EXCL_LINE
    }

    /* Create SSE parser */
    active_req->write_ctx->parser = ik_openai_sse_parser_create(active_req->write_ctx);
    active_req->write_ctx->user_callback = stream_cb;
    active_req->write_ctx->user_ctx = cb_ctx;
    active_req->write_ctx->complete_response = NULL;
    active_req->write_ctx->response_len = 0;
    active_req->write_ctx->has_error = false;

    /* Set up curl options */
    const char *url = "https://api.openai.com/v1/chat/completions";
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_URL, url);
#ifdef NDEBUG
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_POST, 1L);
#else
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_POST, (const void *)1L);
#endif
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_POSTFIELDS, active_req->request_body);
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_WRITEDATA, active_req->write_ctx);

    /* Set headers */
    active_req->headers = NULL;
    active_req->headers = curl_slist_append_(active_req->headers, "Content-Type: application/json");

    char auth_header[256];
    int32_t written = snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", cfg->openai_api_key);
    if (written < 0 || (size_t)written >= sizeof(auth_header)) {  // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        curl_slist_free_all_(active_req->headers);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        return ERR(multi, INVALID_ARG, "API key too long");  // LCOV_EXCL_LINE
    }
    active_req->headers = curl_slist_append_(active_req->headers, auth_header);

    curl_easy_setopt_(active_req->easy_handle, CURLOPT_HTTPHEADER, active_req->headers);

    /* Add to multi handle */
    CURLMcode mres = curl_multi_add_handle_(multi->multi_handle, active_req->easy_handle);
    if (mres != CURLM_OK) {
        curl_easy_cleanup_(active_req->easy_handle);
        curl_slist_free_all_(active_req->headers);
        talloc_free(active_req);
        return ERR(multi, IO, "Failed to add handle to multi: %s", curl_multi_strerror_(mres));
    }

    /* Add to active requests array */
    active_request_t **new_array = talloc_realloc_(multi, multi->active_requests,
                                                    sizeof(active_request_t *) * (multi->active_count + 1));
    if (new_array == NULL) {  // LCOV_EXCL_BR_LINE
        curl_multi_remove_handle_(multi->multi_handle, active_req->easy_handle);  // LCOV_EXCL_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        curl_slist_free_all_(active_req->headers);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        PANIC("Failed to resize active requests array");  // LCOV_EXCL_LINE
    }

    multi->active_requests = new_array;
    multi->active_requests[multi->active_count] = active_req;
    multi->active_count++;

    return OK(NULL);
}

res_t ik_openai_multi_perform(ik_openai_multi_t *multi, int *still_running) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(still_running != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_perform_(multi->multi_handle, still_running);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_perform failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

res_t ik_openai_multi_fdset(ik_openai_multi_t *multi,
                             fd_set *read_fds, fd_set *write_fds,
                             fd_set *exc_fds, int *max_fd) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(write_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(exc_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(max_fd != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_fdset_(multi->multi_handle, read_fds, write_fds, exc_fds, max_fd);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_fdset failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

res_t ik_openai_multi_timeout(ik_openai_multi_t *multi, long *timeout_ms) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(timeout_ms != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_timeout_(multi->multi_handle, timeout_ms);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_timeout failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

res_t ik_openai_multi_info_read(ik_openai_multi_t *multi) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE

    int msgs_left;
    CURLMsg *msg;

    while ((msg = curl_multi_info_read_(multi->multi_handle, &msgs_left)) != NULL) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy_handle = msg->easy_handle;

            /* Find and remove the completed request from active_requests array */
            for (size_t i = 0; i < multi->active_count; i++) {
                if (multi->active_requests[i]->easy_handle == easy_handle) {
                    active_request_t *completed = multi->active_requests[i];

                    /* Clean up curl handles */
                    curl_multi_remove_handle_(multi->multi_handle, easy_handle);
                    curl_easy_cleanup_(easy_handle);
                    curl_slist_free_all_(completed->headers);

                    /* Free the completed request context (talloc will clean up children) */
                    talloc_free(completed);

                    /* Remove from array by shifting remaining elements */
                    for (size_t j = i; j < multi->active_count - 1; j++) {
                        multi->active_requests[j] = multi->active_requests[j + 1];
                    }
                    multi->active_count--;

                    break;
                }
            }
        }
    }

    return OK(NULL);
}
