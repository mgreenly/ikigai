/**
 * @file mock_queue.c
 * @brief FIFO response queue for mock provider
 */

#include "apps/mock-provider/mock_queue.h"

#include "shared/json_allocator.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>

struct mock_queue {
    mock_response_t *head;
    mock_response_t *tail;
};

mock_queue_t *mock_queue_create(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);
    mock_queue_t *queue = talloc_zero(ctx, mock_queue_t);
    assert(queue != NULL);
    return queue;
}

/**
 * Parse a single tool_call object from JSON.
 */
static int parse_tool_call(TALLOC_CTX *ctx, yyjson_val *tc_val,
                           mock_tool_call_t *out)
{
    yyjson_val *name_val = yyjson_obj_get(tc_val, "name");
    if (name_val == NULL || !yyjson_is_str(name_val)) {
        return -1;
    }

    out->name = talloc_strdup(ctx, yyjson_get_str(name_val));
    if (out->name == NULL) {
        return -1;
    }

    yyjson_val *args_val = yyjson_obj_get(tc_val, "arguments");
    if (args_val != NULL && yyjson_is_obj(args_val)) {
        /* Serialize the arguments object back to JSON string */
        size_t len = 0;
        char *json_str = yyjson_val_write(args_val, 0, &len);
        if (json_str != NULL) {
            out->arguments_json = talloc_strdup(ctx, json_str);
            free(json_str);
        } else {
            out->arguments_json = talloc_strdup(ctx, "{}");
        }
    } else {
        out->arguments_json = talloc_strdup(ctx, "{}");
    }

    return 0;
}

/**
 * Parse a single response object from JSON and append to queue.
 */
static int parse_response(mock_queue_t *queue, yyjson_val *resp_val)
{
    mock_response_t *resp = talloc_zero(queue, mock_response_t);
    if (resp == NULL) {
        return -1;
    }

    yyjson_val *content_val = yyjson_obj_get(resp_val, "content");
    yyjson_val *tool_calls_val = yyjson_obj_get(resp_val, "tool_calls");

    if (tool_calls_val != NULL && yyjson_is_arr(tool_calls_val)) {
        resp->type = MOCK_TOOL_CALLS;
        size_t count = yyjson_arr_size(tool_calls_val);
        resp->tool_call_count = (int32_t)count;

        if (count > 0) {
            resp->tool_calls = talloc_zero_array(resp, mock_tool_call_t,
                                                 (unsigned)count);
            if (resp->tool_calls == NULL) {
                talloc_free(resp);
                return -1;
            }

            size_t idx = 0;
            yyjson_val *tc_val;
            yyjson_arr_iter iter;
            yyjson_arr_iter_init(tool_calls_val, &iter);
            while ((tc_val = yyjson_arr_iter_next(&iter)) != NULL) {
                if (parse_tool_call(resp, tc_val,
                                    &resp->tool_calls[idx]) != 0) {
                    talloc_free(resp);
                    return -1;
                }
                idx++;
            }
        }
    } else if (content_val != NULL && yyjson_is_str(content_val)) {
        resp->type = MOCK_TEXT;
        resp->content = talloc_strdup(resp, yyjson_get_str(content_val));
        if (resp->content == NULL) {
            talloc_free(resp);
            return -1;
        }
    } else {
        /* Default to empty text response */
        resp->type = MOCK_TEXT;
        resp->content = talloc_strdup(resp, "");
        if (resp->content == NULL) {
            talloc_free(resp);
            return -1;
        }
    }

    resp->next = NULL;

    /* Append to queue */
    if (queue->tail != NULL) {
        queue->tail->next = resp;
    } else {
        queue->head = resp;
    }
    queue->tail = resp;

    return 0;
}

int mock_queue_load(mock_queue_t *queue, const char *json_body,
                    size_t json_len)
{
    assert(queue != NULL);
    assert(json_body != NULL);

    yyjson_alc alc = ik_make_talloc_allocator(queue);
    char *body_copy = talloc_memdup(queue, json_body, json_len);
    if (body_copy == NULL) {
        return -1;
    }
    yyjson_doc *doc = yyjson_read_opts(body_copy, json_len, 0,
                                       &alc, NULL);
    if (doc == NULL) {
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL || !yyjson_is_obj(root)) {
        return -1;
    }

    yyjson_val *responses_val = yyjson_obj_get(root, "responses");
    if (responses_val == NULL || !yyjson_is_arr(responses_val)) {
        return -1;
    }

    yyjson_val *resp_val;
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(responses_val, &iter);
    while ((resp_val = yyjson_arr_iter_next(&iter)) != NULL) {
        if (parse_response(queue, resp_val) != 0) {
            return -1;
        }
    }

    return 0;
}

mock_response_t *mock_queue_pop(mock_queue_t *queue)
{
    assert(queue != NULL);

    if (queue->head == NULL) {
        return NULL;
    }

    mock_response_t *resp = queue->head;
    queue->head = resp->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    resp->next = NULL;

    /* Re-parent to caller's context (queue still owns until caller steals) */
    return resp;
}

void mock_queue_reset(mock_queue_t *queue)
{
    assert(queue != NULL);

    mock_response_t *curr = queue->head;
    while (curr != NULL) {
        mock_response_t *next = curr->next;
        talloc_free(curr);
        curr = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
}

bool mock_queue_is_empty(const mock_queue_t *queue)
{
    assert(queue != NULL);
    return queue->head == NULL;
}
