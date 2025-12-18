// REPL action processing - LLM and slash command handling
#include "repl_actions.h"
#include "repl_actions_internal.h"
#include "repl.h"
#include "agent.h"
#include "repl_callbacks.h"
#include "panic.h"
#include "shared.h"
#include "wrapper.h"
#include "format.h"
#include "commands.h"
#include "db/message.h"
#include "input_buffer/core.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "scrollback.h"
#include <assert.h>
#include <talloc.h>
#include <stdio.h>
#include <string.h>
#include "logger.h"

/**
 * @brief Handle legacy /pp command (internal debug command)
 *
 * Note: This is a legacy command for debugging. All other slash commands
 * are handled by the command dispatcher (ik_cmd_dispatch).
 *
 * @param repl REPL context
 * @param command Command text (without leading /)
 * @return res_t Result
 */
static res_t ik_repl_handle_slash_command(ik_repl_ctx_t *repl, const char *command)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(command != NULL); /* LCOV_EXCL_BR_LINE */
    assert(strncmp(command, "pp", 2) == 0); /* LCOV_EXCL_BR_LINE */  // Only /pp reaches here
    (void)command;  // Used only in assert (compiled out in release builds)

    // Create format buffer for output
    ik_format_buffer_t *buf = ik_format_buffer_create(repl);

    // Pretty-print the input buffer
    ik_pp_input_buffer(repl->current->input_buffer, buf, 0);

    // Append output to scrollback buffer (split by newlines)
    const char *output = ik_format_get_string(buf);
    size_t output_len = strlen(output);
    ik_repl_append_multiline_to_scrollback(repl->current->scrollback, output, output_len);

    // Clean up format buffer
    talloc_free(buf);

    return OK(NULL);
}

/**
 * @brief Handle slash command dispatch
 *
 * @param repl REPL context
 * @param command_text Command text (with leading /)
 */
static void handle_slash_cmd_(ik_repl_ctx_t *repl, char *command_text)
{
    if (strncmp(command_text + 1, "pp", 2) == 0) {
        res_t result = ik_repl_handle_slash_command(repl, command_text + 1);
        if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    } else {
        res_t result = ik_cmd_dispatch(repl, repl, command_text);
        if (is_err(&result)) {
            const char *err_msg = error_message(result.err);
            char *display_msg = talloc_asprintf(repl, "Error: %s", err_msg);
            if (display_msg != NULL) {  // LCOV_EXCL_BR_LINE
                ik_scrollback_append_line(repl->current->scrollback,  // LCOV_EXCL_LINE
                                          display_msg, strlen(display_msg));  // LCOV_EXCL_LINE
                talloc_free(display_msg);  // LCOV_EXCL_LINE
            }  // LCOV_EXCL_LINE
            talloc_free(result.err);
        }
    }
}

/**
 * @brief Send user message to LLM
 *
 * @param repl REPL context
 * @param message_text User message (null-terminated)
 */
static void send_to_llm_(ik_repl_ctx_t *repl, char *message_text)
{
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user", message_text).ok;
    res_t result = ik_openai_conversation_add_msg(repl->current->conversation, user_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Persist user message to database
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        char *data_json = talloc_asprintf(repl,
                                          "{\"model\":\"%s\",\"temperature\":%.2f,\"max_completion_tokens\":%d}",
                                          repl->shared->cfg->openai_model,
                                          repl->shared->cfg->openai_temperature,
                                          repl->shared->cfg->openai_max_completion_tokens);

        res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                            repl->current->uuid, "user", message_text, data_json);
        if (is_err(&db_res)) {
            yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
            yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "context", "send_to_llm");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_user_message");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));  // LCOV_EXCL_LINE
            ik_log_warn_json(log_doc);  // LCOV_EXCL_LINE
            talloc_free(db_res.err);  // LCOV_EXCL_LINE
        }
        talloc_free(data_json);
    }

    // Clear previous assistant response
    if (repl->current->assistant_response != NULL) {
        talloc_free(repl->current->assistant_response);
        repl->current->assistant_response = NULL;
    }
    if (repl->current->streaming_line_buffer != NULL) {
        talloc_free(repl->current->streaming_line_buffer);
        repl->current->streaming_line_buffer = NULL;
    }

    repl->current->tool_iteration_count = 0;
    ik_agent_transition_to_waiting_for_llm(repl->current);

    result = ik_openai_multi_add_request(repl->current->multi, repl->shared->cfg, repl->current->conversation,
                                         ik_repl_streaming_callback, repl,
                                         ik_repl_http_completion_callback, repl, false,
                                         repl->shared->logger);
    if (is_err(&result)) {
        const char *err_msg = error_message(result.err);
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        ik_agent_transition_to_idle(repl->current);
        talloc_free(result.err);
    } else {
        repl->current->curl_still_running = 1;
    }
}

/**
 * @brief Handle newline action (Enter key)
 *
 * Processes slash commands or sends regular text to the LLM.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_newline_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    const char *text = (const char *)repl->current->input_buffer->text->data;
    size_t text_len = ik_byte_array_size(repl->current->input_buffer->text);

    bool is_slash_command = (text_len > 0 && text[0] == '/');
    char *command_text = NULL;
    if (is_slash_command) {
        command_text = talloc_zero_(repl, text_len + 1);
        if (command_text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        memcpy(command_text, text, text_len);
        command_text[text_len] = '\0';
    }

    ik_repl_dismiss_completion(repl);

    if (is_slash_command) {
        ik_input_buffer_clear(repl->current->input_buffer);
        repl->current->viewport_offset = 0;
    } else {
        ik_repl_submit_line(repl);
    }

    if (is_slash_command) {
        handle_slash_cmd_(repl, command_text);
        talloc_free(command_text);
    } else if (text_len > 0 && repl->current->conversation != NULL && repl->shared->cfg != NULL) {
        char *message_text = talloc_zero_(repl, text_len + 1);
        if (message_text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        memcpy(message_text, text, text_len);
        message_text[text_len] = '\0';

        send_to_llm_(repl, message_text);
        talloc_free(message_text);
    }

    return OK(NULL);
}
