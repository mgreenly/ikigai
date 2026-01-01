/**
 * @file completion.h
 * @brief Tab completion data structures and matching logic
 *
 * Provides command completion functionality for the REPL. When the user types
 * a command prefix (e.g., "/m") and presses TAB, this module finds all matching
 * commands and provides navigation through the suggestions.
 *
 * Ownership: Completion context is created dynamically when TAB pressed,
 * freed when dismissed.
 */

#ifndef IK_COMPLETION_H
#define IK_COMPLETION_H

#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

/**
 * Completion context structure.
 *
 * Contains the list of matching candidates (commands or arguments),
 * the current selection, and the original prefix that triggered completion.
 *
 * Ownership: All strings are talloc'd children of this structure.
 */
typedef struct {
    char **candidates;    // Array of matching strings (talloc'd)
    size_t count;         // Number of matches
    size_t current;       // Currently selected index (0-based)
    char *prefix;         // Original prefix that triggered completion
    char *original_input; // Text before first Tab press (for ESC revert)
} ik_completion_t;

/**
 * Create completion context for command matching.
 *
 * Given a prefix starting with '/', finds all commands that match.
 * Results are case-sensitive and alphabetically sorted.
 * Maximum 10 suggestions are returned.
 *
 * @param ctx Parent talloc context (completion becomes child of this)
 * @param prefix Input prefix (must start with '/')
 * @return Completion context, or NULL if no matches found
 *
 * Ownership: Returned context is allocated on ctx. Caller must free.
 *
 * Preconditions:
 * - ctx != NULL
 * - prefix != NULL
 * - prefix[0] == '/'
 */
ik_completion_t *ik_completion_create_for_commands(TALLOC_CTX *ctx, const char *prefix);

/**
 * Get currently selected candidate.
 *
 * @param comp Completion context
 * @return Currently selected candidate string, or NULL if no candidates
 *
 * Preconditions:
 * - comp != NULL
 */
const char *ik_completion_get_current(const ik_completion_t *comp);

/**
 * Move selection to next candidate (wraps around).
 *
 * If currently at last candidate, wraps to first.
 *
 * @param comp Completion context
 *
 * Preconditions:
 * - comp != NULL
 * - comp->count > 0
 */
void ik_completion_next(ik_completion_t *comp);

/**
 * Move selection to previous candidate (wraps around).
 *
 * If currently at first candidate, wraps to last.
 *
 * @param comp Completion context
 *
 * Preconditions:
 * - comp != NULL
 * - comp->count > 0
 */
void ik_completion_prev(ik_completion_t *comp);

/**
 * Check if input still matches prefix (for re-triggering).
 *
 * Used to determine if the completion context is still valid as the
 * user continues typing.
 *
 * @param comp Completion context
 * @param current_input Current input buffer content
 * @return true if current_input starts with comp->prefix, false otherwise
 *
 * Preconditions:
 * - comp != NULL
 * - current_input != NULL
 */
bool ik_completion_matches_prefix(const ik_completion_t *comp, const char *current_input);

// Forward declarations for argument completion
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * Create completion context for command arguments.
 *
 * Given input like "/model ", finds all matching arguments for that command.
 * Different commands have different argument sources:
 * - /model: Available models from config or registry
 * - /rewind: Existing mark labels from repl->marks
 * - /debug: ["on", "off"]
 * - Other commands: NULL (no argument completion)
 *
 * Results are case-sensitive and alphabetically sorted.
 * Maximum 10 suggestions are returned.
 *
 * @param ctx Parent talloc context (completion becomes child of this)
 * @param repl REPL context (for accessing marks, config, etc.)
 * @param input Full input string (e.g., "/model " or "/rewind gp")
 * @return Completion context, or NULL if no matches or command has no arg completion
 *
 * Ownership: Returned context is allocated on ctx. Caller must free.
 *
 * Preconditions:
 * - ctx != NULL
 * - repl != NULL
 * - input != NULL
 * - input contains command and space (e.g., "/cmd " or "/cmd arg")
 */
ik_completion_t *ik_completion_create_for_arguments(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, const char *input);

#endif // IK_COMPLETION_H
