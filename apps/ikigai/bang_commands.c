/**
 * @file bang_commands.c
 * @brief Bang command dispatcher for !load/!unload skill management
 */

#include "apps/ikigai/bang_commands.h"

#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "shared/error.h"
#include "shared/panic.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

static res_t handle_load_(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    (void)args;
    const char *msg = "!load: not yet implemented";
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return OK(NULL);
    (void)ctx;
}

static res_t handle_unload_(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    (void)args;
    const char *msg = "!unload: not yet implemented";
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return OK(NULL);
    (void)ctx;
}

res_t ik_bang_dispatch(void *ctx, ik_repl_ctx_t *repl, const char *input)
{
    assert(ctx != NULL);    /* LCOV_EXCL_BR_LINE */
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(input != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(input[0] == '!'); /* LCOV_EXCL_BR_LINE */

    // Skip leading '!'
    const char *cmd_start = input + 1;

    // Skip leading whitespace after '!'
    while (isspace((unsigned char)*cmd_start)) {  /* LCOV_EXCL_BR_LINE */
        cmd_start++;
    }

    // Echo command to scrollback
    ik_scrollback_append_line(repl->current->scrollback, input, strlen(input));

    // Empty command (just "!")
    if (*cmd_start == '\0') {  /* LCOV_EXCL_BR_LINE */
        char *msg = ik_scrollback_format_warning(ctx, "Empty command");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return ERR(ctx, INVALID_ARG, "Empty command");
    }

    // Find end of command name
    const char *args_start = cmd_start;
    while (*args_start && !isspace((unsigned char)*args_start)) {  /* LCOV_EXCL_BR_LINE */
        args_start++;
    }

    // Extract command name
    size_t cmd_len = (size_t)(args_start - cmd_start);
    char *cmd_name = talloc_strndup(ctx, cmd_start, cmd_len);
    if (!cmd_name) {  /* LCOV_EXCL_BR_LINE */
        PANIC("OOM");  /* LCOV_EXCL_LINE */
    }

    // Skip whitespace before args
    while (isspace((unsigned char)*args_start)) {  /* LCOV_EXCL_BR_LINE */
        args_start++;
    }

    // Args is NULL if no arguments
    const char *args = (*args_start == '\0') ? NULL : args_start;  /* LCOV_EXCL_BR_LINE */

    res_t result;
    if (strcmp(cmd_name, "load") == 0) {
        result = handle_load_(ctx, repl, args);
    } else if (strcmp(cmd_name, "unload") == 0) {
        result = handle_unload_(ctx, repl, args);
    } else {
        char *err_text = talloc_asprintf(ctx, "Unknown bang command '%s'", cmd_name);
        if (!err_text) {  /* LCOV_EXCL_BR_LINE */
            PANIC("OOM");  /* LCOV_EXCL_LINE */
        }
        char *msg = ik_scrollback_format_warning(ctx, err_text);
        talloc_free(err_text);
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        result = ERR(ctx, INVALID_ARG, "Unknown bang command '%s'", cmd_name);
    }

    talloc_free(cmd_name);
    return result;
}
