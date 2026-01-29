/**
 * @file completion.c
 * @brief Tab completion data structures and matching logic implementation
 */

#include "completion.h"
#include "commands.h"
#include "repl.h"
#include "panic.h"
#include "fzy_wrapper.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

// Maximum number of completion suggestions to return
#define MAX_COMPLETIONS 15

// Argument provider function type
typedef struct {
    const char **args;
    size_t count;
} arg_provider_result_t;

/**
 * Provide arguments for /model command
 *
 * Returns a list of available models from all providers.
 */
static arg_provider_result_t provide_model_args(TALLOC_CTX *ctx, ik_repl_ctx_t *repl)
{
    (void)ctx;   // Unused - using static array
    (void)repl;  // Unused - using static array

    // Complete list of models from all providers
    static const char *model_list[] = {
        // Anthropic
        "claude-haiku-4-5",
        "claude-sonnet-4-5",
        "claude-opus-4-5",
        // OpenAI
        "gpt-4",
        "gpt-4-turbo",
        "gpt-4o",
        "gpt-4o-mini",
        "gpt-3.5-turbo",
        "gpt-5",
        "gpt-5-mini",
        "gpt-5-nano",
        "o1",
        "o1-mini",
        "o1-preview",
        "o3-mini",
        // Google
        "gemini-2.5-flash-lite",
        "gemini-3-flash-preview",
        "gemini-3-pro-preview"
    };

    arg_provider_result_t result;
    result.args = model_list;
    result.count = sizeof(model_list) / sizeof(model_list[0]);
    return result;
}

/**
 * Provide thinking level arguments for /model command
 *
 * Returns thinking levels: none, low, med, high
 */
static arg_provider_result_t provide_thinking_args(TALLOC_CTX *ctx, ik_repl_ctx_t *repl)
{
    (void)ctx;   // Unused - using static array
    (void)repl;  // Unused - using static array

    static const char *thinking_levels[] = {
        "none",
        "low",
        "med",
        "high"
    };

    arg_provider_result_t result;
    result.args = thinking_levels;
    result.count = sizeof(thinking_levels) / sizeof(thinking_levels[0]);
    return result;
}

/**
 * Provide arguments for /debug command
 *
 * Returns ["off", "on"]
 */
static arg_provider_result_t provide_debug_args(TALLOC_CTX *ctx, ik_repl_ctx_t *repl)
{
    (void)ctx;   // Unused - using static array
    (void)repl;  // Unused - using static array

    static const char *debug_args[] = {"off", "on"};

    arg_provider_result_t result;
    result.args = debug_args;
    result.count = sizeof(debug_args) / sizeof(debug_args[0]);
    return result;
}

/**
 * Provide arguments for /rewind command
 *
 * Returns labeled marks from repl->current->marks
 */
static arg_provider_result_t provide_rewind_args(TALLOC_CTX *ctx, ik_repl_ctx_t *repl)
{
    // Count labeled marks first
    size_t labeled_count = 0;
    for (size_t i = 0; i < repl->current->mark_count; i++) {     // LCOV_EXCL_BR_LINE
        if (repl->current->marks[i]->label != NULL) {     // LCOV_EXCL_BR_LINE
            labeled_count++;
        }
    }

    if (labeled_count == 0) {     // LCOV_EXCL_BR_LINE
        arg_provider_result_t result = {NULL, 0};
        return result;
    }

    // Allocate temporary array for mark labels
    const char **mark_labels = talloc_array(ctx, const char *, (unsigned int)labeled_count);
    if (mark_labels == NULL) PANIC("Out of memory");     // LCOV_EXCL_BR_LINE

    size_t collected = 0;
    for (size_t i = 0; i < repl->current->mark_count; i++) {     // LCOV_EXCL_BR_LINE
        if (repl->current->marks[i]->label != NULL) {     // LCOV_EXCL_BR_LINE
            mark_labels[collected++] = repl->current->marks[i]->label;
        }
    }

    arg_provider_result_t result;
    result.args = mark_labels;
    result.count = labeled_count;
    return result;
}

ik_completion_t *ik_completion_create_for_commands(TALLOC_CTX *ctx,
                                                   const char *prefix)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(prefix != NULL);    // LCOV_EXCL_BR_LINE
    assert(prefix[0] == '/');  // LCOV_EXCL_BR_LINE

    // Skip the leading '/' to get the search string
    const char *search = prefix + 1;

    // Get all registered commands
    size_t cmd_count;
    const ik_command_t *commands = ik_cmd_get_all(&cmd_count);

    // Build candidate array
    const char **candidates = talloc_array(ctx, const char *, (unsigned int)cmd_count);
    if (candidates == NULL) PANIC("OOM");     // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < cmd_count; i++) {     // LCOV_EXCL_BR_LINE
        candidates[i] = commands[i].name;
    }

    // Use fzy to filter and score
    size_t match_count = 0;
    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, cmd_count,
                                             search, MAX_COMPLETIONS, &match_count);
    talloc_free(candidates);

    if (results == NULL || match_count == 0) {     // LCOV_EXCL_BR_LINE
        return NULL;
    }

    // Build completion context from fzy results
    ik_completion_t *comp = talloc_zero(ctx, ik_completion_t);
    if (!comp) PANIC("OOM");     // LCOV_EXCL_BR_LINE

    comp->candidates = talloc_array(comp, char *, (unsigned int)match_count);
    if (!comp->candidates) PANIC("OOM");     // LCOV_EXCL_BR_LINE

    comp->prefix = talloc_strdup(comp, prefix);
    if (!comp->prefix) PANIC("OOM");     // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < match_count; i++) {     // LCOV_EXCL_BR_LINE
        comp->candidates[i] = talloc_strdup(comp, results[i].candidate);
        if (!comp->candidates[i]) PANIC("OOM");     // LCOV_EXCL_BR_LINE
    }

    comp->count = match_count;
    comp->current = 0;

    talloc_free(results);
    return comp;
}

const char *ik_completion_get_current(const ik_completion_t *comp)
{
    assert(comp != NULL);  // LCOV_EXCL_BR_LINE

    // Defensive: creation returns NULL for empty completions, so this should never happen
    if (comp->count == 0) return NULL;     // LCOV_EXCL_LINE

    return comp->candidates[comp->current];
}

void ik_completion_next(ik_completion_t *comp)
{
    assert(comp != NULL);       // LCOV_EXCL_BR_LINE
    assert(comp->count > 0);    // LCOV_EXCL_BR_LINE

    comp->current = (comp->current + 1) % comp->count;
}

void ik_completion_prev(ik_completion_t *comp)
{
    assert(comp != NULL);       // LCOV_EXCL_BR_LINE
    assert(comp->count > 0);    // LCOV_EXCL_BR_LINE

    if (comp->current == 0) {     // LCOV_EXCL_BR_LINE
        comp->current = comp->count - 1;
    } else {
        comp->current--;
    }
}

ik_completion_t *ik_completion_create_for_arguments(TALLOC_CTX *ctx,
                                                    ik_repl_ctx_t *repl,
                                                    const char *input)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(repl != NULL);   // LCOV_EXCL_BR_LINE
    assert(input != NULL);  // LCOV_EXCL_BR_LINE

    // Parse input to extract command and argument prefix
    // Input format: "/command arg_prefix" or "/command "

    // Find first space
    const char *space_pos = strchr(input, ' ');
    if (space_pos == NULL) {     // LCOV_EXCL_BR_LINE
        // No space - this is just a command, not arguments
        return NULL;
    }

    // Extract command name (without leading '/')
    size_t cmd_len = (size_t)(space_pos - input) - 1;  // -1 for leading '/'
    if (cmd_len == 0) {     // LCOV_EXCL_BR_LINE
        return NULL;
    }

    char *cmd_name = talloc_zero_size(ctx, cmd_len + 1);
    if (cmd_name == NULL) PANIC("Out of memory");     // LCOV_EXCL_BR_LINE
    memcpy(cmd_name, input + 1, cmd_len);  // +1 to skip '/'
    cmd_name[cmd_len] = '\0';

    // Extract argument prefix (everything after the space)
    const char *arg_prefix = space_pos + 1;

    // Get argument provider for this command (strategy pattern)
    arg_provider_result_t provider_result;

    if (strcmp(cmd_name, "model") == 0) {
        // Check if we're completing thinking level (after "/")
        const char *slash = strchr(arg_prefix, '/');
        if (slash != NULL) {
            // Complete thinking level after slash
            provider_result = provide_thinking_args(ctx, repl);
            // Update arg_prefix to start after the slash
            arg_prefix = slash + 1;
        } else {
            // Complete model name
            provider_result = provide_model_args(ctx, repl);
        }
    } else if (strcmp(cmd_name, "debug") == 0) {
        provider_result = provide_debug_args(ctx, repl);
    } else if (strcmp(cmd_name, "rewind") == 0) {
        provider_result = provide_rewind_args(ctx, repl);
    } else {
        // Commands without argument completion: mark, system, help, clear
        talloc_free(cmd_name);
        return NULL;
    }

    talloc_free(cmd_name);

    // Check if provider returned any arguments
    if (provider_result.count == 0) {     // LCOV_EXCL_BR_LINE
        return NULL;
    }

    // Use fzy to filter and score arguments
    size_t match_count = 0;
    ik_fzy_result_t *results = ik_fzy_filter(ctx, provider_result.args,
                                             provider_result.count,
                                             arg_prefix, MAX_COMPLETIONS, &match_count);

    if (results == NULL || match_count == 0) {     // LCOV_EXCL_BR_LINE
        return NULL;
    }

    // Allocate completion context
    ik_completion_t *comp = talloc_zero(ctx, ik_completion_t);
    if (!comp) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Allocate candidate array
    comp->candidates = talloc_array(comp, char *, (unsigned int)match_count);
    if (!comp->candidates) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Store the original input as prefix
    comp->prefix = talloc_strdup(comp, input);
    if (!comp->prefix) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Collect matching arguments from fzy results
    for (size_t i = 0; i < match_count; i++) {     // LCOV_EXCL_BR_LINE
        comp->candidates[i] = talloc_strdup(comp, results[i].candidate);
        if (!comp->candidates[i]) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
    }

    comp->count = match_count;
    comp->current = 0;

    talloc_free(results);
    return comp;
}
