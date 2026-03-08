#pragma once

#include "apps/ikigai/repl.h"
#include "shared/error.h"

/**
 * @brief Dispatch a bang command (! prefix)
 *
 * Routes !load and !unload to their stub handlers.
 * Mirrors ik_cmd_dispatch() patterns: null-terminated input copy before buffer
 * clear (done by caller), echo to scrollback, whitespace skipping, error for
 * empty command.
 *
 * @param ctx  Talloc parent context
 * @param repl REPL context
 * @param input Full input text including leading '!' (null-terminated)
 * @return OK on success, ERR on failure
 */
res_t ik_bang_dispatch(void *ctx, ik_repl_ctx_t *repl, const char *input);
