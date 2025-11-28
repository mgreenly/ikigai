/**
 * @file commands_mark.h
 * @brief Mark and rewind command declarations
 */

#ifndef IK_COMMANDS_MARK_H
#define IK_COMMANDS_MARK_H

#include "error.h"
#include "repl.h"

/**
 * Command handler for /mark
 *
 * Creates a mark at the current conversation position.
 *
 * @param ctx Context for allocations (unused, required by dispatcher signature)
 * @param repl REPL context
 * @param args Optional label for the mark (NULL for auto-numbered mark)
 * @return OK(NULL) on success, ERR on failure
 */
res_t ik_cmd_mark(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Command handler for /rewind
 *
 * Rewinds conversation to a previously created mark.
 *
 * @param ctx Context for allocations
 * @param repl REPL context
 * @param args Optional label to rewind to (NULL for most recent mark)
 * @return OK(NULL) on success, ERR on failure
 */
res_t ik_cmd_rewind(void *ctx, ik_repl_ctx_t *repl, const char *args);

#endif // IK_COMMANDS_MARK_H
