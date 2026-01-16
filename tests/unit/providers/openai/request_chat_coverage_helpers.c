#include "request_chat_coverage_helpers.h"

#include <talloc.h>

/* Helper: Create a minimal request */
ik_request_t *ik_test_create_minimal_request(void *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(ctx, "gpt-4");
    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->max_output_tokens = 0;
    req->tool_count = 0;
    return req;
}

/* Helper: Add a tool to a request */
void ik_test_add_tool(void *ctx, ik_request_t *req, const char *name, const char *desc, const char *params)
{
    size_t idx = req->tool_count++;
    req->tools = talloc_realloc(ctx, req->tools, ik_tool_def_t, (unsigned int)req->tool_count);
    req->tools[idx].name = talloc_strdup(ctx, name);
    req->tools[idx].description = talloc_strdup(ctx, desc);
    req->tools[idx].parameters = talloc_strdup(ctx, params);
}

/* Helper: Add a text message to a request */
void ik_test_add_message(void *ctx, ik_request_t *req, ik_role_t role, const char *text)
{
    size_t idx = req->message_count++;
    req->messages = talloc_realloc(ctx, req->messages, ik_message_t, (unsigned int)req->message_count);
    req->messages[idx].role = role;
    req->messages[idx].content_count = 1;
    req->messages[idx].content_blocks = talloc_array(ctx, ik_content_block_t, 1);
    req->messages[idx].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[idx].content_blocks[0].data.text.text = talloc_strdup(ctx, text);
}
