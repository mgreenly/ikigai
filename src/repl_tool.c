// REPL tool execution helper
#include "repl.h"

#include "event_render.h"
#include "format.h"
#include "panic.h"
#include "tool.h"

#include <assert.h>
#include <talloc.h>

// Helper: Execute pending tool call and add messages to conversation
// Exposed to reduce complexity in handle_request_success
void ik_repl_execute_pending_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);               // LCOV_EXCL_BR_LINE
    assert(repl->pending_tool_call != NULL);  // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = repl->pending_tool_call;

    // 1. Add tool_call message to conversation (canonical format)
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_openai_msg_t *tc_msg = ik_openai_msg_create_tool_call(
        repl->conversation, tc->id, "function", tc->name, tc->arguments, summary);
    res_t result = ik_openai_conversation_add_msg(repl->conversation, tc_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_call is added
    if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
        fprintf(repl->openai_debug_pipe->write_end,
                "<< TOOL_CALL: %s\n",
                summary);
        fflush(repl->openai_debug_pipe->write_end);
    }

    // 2. Execute tool
    res_t tool_res = ik_tool_dispatch(repl, tc->name, tc->arguments);
    if (is_err(&tool_res)) PANIC("tool dispatch failed"); // LCOV_EXCL_BR_LINE
    char *result_json = tool_res.ok;

    // 3. Add tool result message to conversation
    ik_openai_msg_t *result_msg = ik_openai_msg_create_tool_result(
        repl->conversation, tc->id, result_json);
    result = ik_openai_conversation_add_msg(repl->conversation, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // 4. Display tool call and result in scrollback via event renderer
    ik_event_render(repl->scrollback, "tool_call", summary, "{}");
    const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
    ik_event_render(repl->scrollback, "tool_result", formatted_result, "{}");

    // 5. Clear pending tool call
    talloc_free(summary);
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = NULL;
}
