#include "agent.h"

#include "message.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>

res_t ik_agent_add_message(ik_agent_ctx_t *agent, ik_message_t *msg)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);    // LCOV_EXCL_BR_LINE

    // Grow array if needed
    if (agent->message_count >= agent->message_capacity) {
        size_t new_capacity = agent->message_capacity == 0 ? 16 : agent->message_capacity * 2;
        agent->messages = talloc_realloc(agent, agent->messages, ik_message_t *, (unsigned int)new_capacity);
        if (!agent->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        agent->message_capacity = new_capacity;
    }

    // Reparent message to agent and add to array
    talloc_steal(agent, msg);
    agent->messages[agent->message_count++] = msg;

    return OK(msg);
}

void ik_agent_clear_messages(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    // Free messages array (talloc frees all children)
    if (agent->messages != NULL) {
        talloc_free(agent->messages);
        agent->messages = NULL;
    }

    agent->message_count = 0;
    agent->message_capacity = 0;
}

/**
 * Helper to clone a single content block
 */
static void clone_content_block(ik_content_block_t *dest_block,
                                const ik_content_block_t *src_block,
                                TALLOC_CTX *ctx)
{
    dest_block->type = src_block->type;

    if (src_block->type == IK_CONTENT_TEXT) {
        dest_block->data.text.text = talloc_strdup(ctx, src_block->data.text.text);
        if (!dest_block->data.text.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else if (src_block->type == IK_CONTENT_TOOL_CALL) {
        dest_block->data.tool_call.id = talloc_strdup(ctx, src_block->data.tool_call.id);
        if (!dest_block->data.tool_call.id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_call.name = talloc_strdup(ctx, src_block->data.tool_call.name);
        if (!dest_block->data.tool_call.name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_call.arguments = talloc_strdup(ctx, src_block->data.tool_call.arguments);
        if (!dest_block->data.tool_call.arguments) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else if (src_block->type == IK_CONTENT_TOOL_RESULT) {
        dest_block->data.tool_result.tool_call_id = talloc_strdup(ctx, src_block->data.tool_result.tool_call_id);
        if (!dest_block->data.tool_result.tool_call_id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_result.content = talloc_strdup(ctx, src_block->data.tool_result.content);
        if (!dest_block->data.tool_result.content) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_result.is_error = src_block->data.tool_result.is_error;
    } else if (src_block->type == IK_CONTENT_THINKING) {
        dest_block->data.thinking.text = talloc_strdup(ctx, src_block->data.thinking.text);
        if (!dest_block->data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
}

/**
 * Helper to clone a single message
 */
static ik_message_t *clone_message(const ik_message_t *src_msg, TALLOC_CTX *ctx)
{
    ik_message_t *dest_msg = talloc_zero(ctx, ik_message_t);
    if (!dest_msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    dest_msg->role = src_msg->role;
    dest_msg->content_count = src_msg->content_count;
    dest_msg->content_blocks = talloc_array(dest_msg, ik_content_block_t,
                                            (unsigned int)src_msg->content_count);
    if (!dest_msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < src_msg->content_count; j++) {
        clone_content_block(&dest_msg->content_blocks[j], &src_msg->content_blocks[j], dest_msg);
    }

    if (src_msg->provider_metadata != NULL) {
        dest_msg->provider_metadata = talloc_strdup(dest_msg, src_msg->provider_metadata);
        if (!dest_msg->provider_metadata) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        dest_msg->provider_metadata = NULL;
    }

    return dest_msg;
}

res_t ik_agent_clone_messages(ik_agent_ctx_t *dest, const ik_agent_ctx_t *src)
{
    assert(dest != NULL);  // LCOV_EXCL_BR_LINE
    assert(src != NULL);   // LCOV_EXCL_BR_LINE

    ik_agent_clear_messages(dest);

    if (src->message_count == 0) {
        return OK(NULL);
    }

    dest->messages = talloc_array(dest, ik_message_t *, (unsigned int)src->message_count);
    if (!dest->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    dest->message_capacity = src->message_count;

    for (size_t i = 0; i < src->message_count; i++) {
        dest->messages[i] = clone_message(src->messages[i], dest);
    }

    dest->message_count = src->message_count;
    return OK(dest->messages);
}
