/**
 * @file commands_pin.h
 * @brief Pin command handlers for managing system prompt documents
 */

#ifndef IK_COMMANDS_PIN_H
#define IK_COMMANDS_PIN_H

#include "shared/error.h"

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * Pin command handler - manage pinned documents
 *
 * Without arguments: Lists currently pinned documents in FIFO order
 * With path argument: Adds path to pinned documents list
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (NULL to list, path to add)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_pin(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Unpin command handler - remove pinned document
 *
 * Removes specified path from pinned documents list
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (path to remove)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_unpin(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Persist pin/unpin command to database
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param command Command name ("pin" or "unpin")
 * @param path Path argument
 */
void ik_persist_pin_command(void *ctx, ik_repl_ctx_t *repl, const char *command, const char *path);

/**
 * List all pinned documents
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_pin_list(void *ctx, ik_repl_ctx_t *repl);

/**
 * Add a document to pinned list
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param path Path to pin
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_pin_add(void *ctx, ik_repl_ctx_t *repl, const char *path);

#endif // IK_COMMANDS_PIN_H
