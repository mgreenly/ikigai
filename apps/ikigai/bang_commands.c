/**
 * @file bang_commands.c
 * @brief Bang command dispatcher for future bang commands
 *
 * The skill management commands (load, unload, skills) have moved to the
 * slash command system. This dispatcher remains for future bang commands
 * that interact with message history.
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

    char *err_text = talloc_asprintf(ctx, "Unknown bang command '%s'", cmd_name);
    if (!err_text) {  /* LCOV_EXCL_BR_LINE */
        PANIC("OOM");  /* LCOV_EXCL_LINE */
    }
    char *msg = ik_scrollback_format_warning(ctx, err_text);
    talloc_free(err_text);
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    talloc_free(msg);
    res_t result = ERR(ctx, INVALID_ARG, "Unknown bang command '%s'", cmd_name);
    talloc_free(cmd_name);
    return result;
}
