/**
 * @file commands_bg.h
 * @brief Background process slash command handlers (/ps, /pinspect, /pkill, /pwrite, /pclose)
 */

#ifndef IK_COMMANDS_BG_H
#define IK_COMMANDS_BG_H

#include "shared/error.h"

typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * List background processes in tabular format.
 * Columns: ID, PID, STATUS, AGE, TTL LEFT, OUTPUT, COMMAND
 */
res_t ik_cmd_ps(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Inspect a background process: show status header and output lines.
 * Usage: /pinspect <id> [--tail=N | --lines=S-E | --since-last | --full]
 * Default: last 50 lines.
 */
res_t ik_cmd_pinspect(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Terminate a background process.
 * Usage: /pkill <id>
 */
res_t ik_cmd_pkill(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Write text to a background process's stdin.
 * Usage: /pwrite <id> [--raw] [--eof] <text>
 * --raw: do not append newline
 * --eof: close stdin after write
 */
res_t ik_cmd_pwrite(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Send EOF (Ctrl-D) to a background process's stdin.
 * Usage: /pclose <id>
 */
res_t ik_cmd_pclose(void *ctx, ik_repl_ctx_t *repl, const char *args);

#endif /* IK_COMMANDS_BG_H */
