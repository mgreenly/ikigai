/**
 * @file commands.c
 * @brief REPL command registry and dispatcher implementation
 */

#include "commands.h"

#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "openai/client.h"
#include "marks.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

// Forward declarations of command handlers (to be implemented in later tasks)
static res_t cmd_clear(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_mark(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_rewind(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_help(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_model(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_system(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Command registry
static const ik_command_t commands[] = {
    {"clear", "Clear scrollback, session messages, and marks", cmd_clear},
    {"mark", "Create a checkpoint for rollback (usage: /mark [label])",
     cmd_mark},
    {"rewind", "Rollback to a checkpoint (usage: /rewind [label])", cmd_rewind},
    {"help", "Show available commands", cmd_help},
    {"model", "Switch LLM model (usage: /model <name>)", cmd_model},
    {"system", "Set system message (usage: /system <text>)", cmd_system},
};

static const size_t command_count =
    sizeof(commands) / sizeof(commands[0]);     // LCOV_EXCL_BR_LINE

const ik_command_t *ik_cmd_get_all(size_t *count)
{
    assert(count != NULL);     // LCOV_EXCL_BR_LINE
    *count = command_count;
    return commands;
}

res_t ik_cmd_dispatch(void *ctx, ik_repl_ctx_t *repl, const char *input)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    assert(input != NULL);     // LCOV_EXCL_BR_LINE
    assert(input[0] == '/');     // LCOV_EXCL_BR_LINE

    // Skip leading slash
    const char *cmd_start = input + 1;

    // Skip leading whitespace
    while (isspace((unsigned char)*cmd_start)) {     // LCOV_EXCL_BR_LINE
        cmd_start++;
    }

    // Empty command (just "/")
    if (*cmd_start == '\0') {     // LCOV_EXCL_BR_LINE
        char *msg = talloc_strdup(ctx, "Error: Empty command");
        if (!msg) {         // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Empty command");
    }

    // Find end of command name (space or end of string)
    const char *args_start = cmd_start;
    while (*args_start && !isspace((unsigned char)*args_start)) {     // LCOV_EXCL_BR_LINE
        args_start++;
    }

    // Extract command name
    size_t cmd_len = (size_t)(args_start - cmd_start);
    char *cmd_name = talloc_strndup(ctx, cmd_start, cmd_len);
    if (!cmd_name) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");    // LCOV_EXCL_LINE
    }

    // Skip whitespace before args
    while (isspace((unsigned char)*args_start)) {     // LCOV_EXCL_BR_LINE
        args_start++;
    }

    // Args is NULL if no arguments, otherwise points to remaining text
    const char *args = (*args_start == '\0') ? NULL : args_start;     // LCOV_EXCL_BR_LINE

    // Look up command in registry
    for (size_t i = 0; i < command_count; i++) {     // LCOV_EXCL_BR_LINE
        if (strcmp(cmd_name, commands[i].name) == 0) {         // LCOV_EXCL_BR_LINE
            // Found matching command, invoke handler
            return commands[i].handler(ctx, repl, args);
        }
    }

    // Unknown command
    char *msg = talloc_asprintf(ctx, "Error: Unknown command '%s'", cmd_name);
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
    return ERR(ctx, INVALID_ARG, "Unknown command '%s'", cmd_name);
}

// Command handler stubs (to be implemented in later tasks)

static res_t cmd_clear(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;     // Unused for /clear

    // Clear scrollback buffer
    ik_scrollback_clear(repl->scrollback);

    // Clear conversation (session messages)
    if (repl->conversation != NULL) {  // LCOV_EXCL_BR_LINE
        ik_openai_conversation_clear(repl->conversation);
    }

    // Clear marks
    if (repl->marks != NULL) {  // LCOV_EXCL_BR_LINE
        for (size_t i = 0; i < repl->mark_count; i++) {
            talloc_free(repl->marks[i]);
        }
        talloc_free(repl->marks);
        repl->marks = NULL;
        repl->mark_count = 0;
    }

    return OK(NULL);
}

static res_t cmd_mark(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)ctx;

    // Parse optional label from args
    // Note: dispatcher ensures args is either NULL or points to non-empty string
    const char *label = args;

    // Create the mark
    res_t result = ik_mark_create(repl, label);
    if (is_err(&result)) {  /* LCOV_EXCL_BR_LINE */
        return result;  // LCOV_EXCL_LINE
    }

    return OK(NULL);
}

static res_t cmd_rewind(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)ctx;

    // Parse optional label from args
    // Note: dispatcher ensures args is either NULL or points to non-empty string
    const char *label = args;

    // Rewind to the mark
    res_t result = ik_mark_rewind_to(repl, label);
    if (is_err(&result)) {
        // Show error message in scrollback
        char *err_msg = talloc_asprintf(ctx, "Error: %s", result.err->msg);
        if (err_msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        ik_scrollback_append_line(repl->scrollback, err_msg, strlen(err_msg));
        talloc_free(err_msg);
        return OK(NULL);  // Don't propagate error, just show it
    }

    return OK(NULL);
}

static res_t cmd_help(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;

    // TODO: Implement in Task 7.5
    char *msg = talloc_strdup(ctx, "TODO: /help not yet implemented");
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
    return OK(NULL);
}

static res_t cmd_model(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;

    // TODO: Implement in Task 7.6
    char *msg = talloc_strdup(ctx, "TODO: /model not yet implemented");
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
    return OK(NULL);
}

static res_t cmd_system(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;

    // TODO: Implement in Task 7.7
    char *msg = talloc_strdup(ctx, "TODO: /system not yet implemented");
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
    return OK(NULL);
}
