// REPL action processing - completion functionality
#include "repl_actions_internal.h"

#include "agent.h"
#include "completion.h"
#include "input_buffer/core.h"
#include "panic.h"
#include "repl.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
/**
 * @brief Dismiss active completion
 *
 * Helper function to safely dismiss the completion layer.
 *
 * @param repl REPL context
 */
void ik_repl_dismiss_completion(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->current->completion != NULL) {
        talloc_free(repl->current->completion);
        repl->current->completion = NULL;
    }
}

/**
 * @brief Update completion after character insertion
 *
 * Creates or updates completion context based on the current input buffer.
 * Typing "/" triggers completion display with all matching commands.
 * Typing more characters filters the list.
 * Input not starting with "/" dismisses completion.
 *
 * When replacing completion, preserve original_input if it exists.
 *
 * @param repl REPL context
 */
void ik_repl_update_completion_after_char(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);

    // Check if input starts with '/'
    if (text_len > 0 && text[0] == '/') {  // LCOV_EXCL_BR_LINE - defensive: text_len > 0 checked
        // Preserve original_input if it exists (for ESC revert)
        char *original_input = NULL;
        if (repl->current->completion != NULL && repl->current->completion->original_input != NULL) {
            original_input = talloc_strdup(repl, repl->current->completion->original_input);
            if (original_input == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }

        // Create null-terminated string for new prefix
        char *new_prefix = talloc_zero_size(repl, text_len + 1);
        if (new_prefix == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        memcpy(new_prefix, text, text_len);
        new_prefix[text_len] = '\0';

        // Try to create new completion with updated prefix
        ik_completion_t *new_comp = ik_completion_create_for_commands(repl, new_prefix);
        talloc_free(new_prefix);

        // Replace old completion with new one (or NULL if no matches)
        ik_repl_dismiss_completion(repl);
        repl->current->completion = new_comp;

        // If new completion created and we have original_input, preserve it
        if (repl->current->completion != NULL && original_input != NULL) {
            repl->current->completion->original_input = original_input;
        } else if (original_input != NULL) {  // LCOV_EXCL_BR_LINE
            talloc_free(original_input);  // LCOV_EXCL_LINE
        }
    } else {  // LCOV_EXCL_BR_LINE
        // Input doesn't start with '/' - dismiss completion
        ik_repl_dismiss_completion(repl);  // LCOV_EXCL_LINE
    }
}

/**
 * @brief Build completion buffer text (without suffix)
 *
 * Helper to construct the text that should appear in the input buffer for a selected completion.
 * Does NOT handle talloc management or input buffer update - just builds the string.
 *
 * @param repl REPL context (for talloc)
 * @param completion Completion context
 * @param suffix Optional suffix to append (e.g. " " for Space commit), NULL for none
 * @return Allocated string to be freed by caller, or NULL on error
 */
static char *build_completion_buffer_text(ik_repl_ctx_t *repl, ik_completion_t *completion, const char *suffix)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(completion != NULL); /* LCOV_EXCL_BR_LINE */

    const char *selected = ik_completion_get_current(completion);
    if (selected == NULL) {  // LCOV_EXCL_BR_LINE - defensive
        return NULL;  // LCOV_EXCL_LINE
    }

    // Get the original input (what the user typed before Tab)
    // This helps us preserve the structure (command vs argument)
    const char *original = completion->original_input;
    if (original == NULL) {  // LCOV_EXCL_BR_LINE - defensive
        original = completion->prefix;
    }

    // Create null-terminated string for checking
    size_t original_len = strlen(original);
    char *current_input = talloc_zero_size(repl, original_len + 1);
    if (current_input == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    memcpy(current_input, original, original_len);
    current_input[original_len] = '\0';

    // Check if we're completing arguments (has space after command)
    const char *space_pos = strchr(current_input, ' ');
    char *replacement = NULL;
    size_t suffix_len = suffix != NULL ? strlen(suffix) : 0;

    if (space_pos != NULL) {
        // Argument completion - build "/command selected_arg" + suffix
        // cmd_part_len includes everything up to and including the space
        size_t cmd_part_len = (size_t)(space_pos - current_input) + 1;
        size_t selected_len = strlen(selected);
        size_t total_len = cmd_part_len + selected_len + suffix_len;  // cmd (with space) + arg + suffix

        replacement = talloc_zero_size(repl, total_len + 1);
        if (replacement == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        memcpy(replacement, current_input, cmd_part_len);
        memcpy(replacement + cmd_part_len, selected, selected_len);
        if (suffix_len > 0) {  // LCOV_EXCL_BR_LINE - defensive
            memcpy(replacement + cmd_part_len + selected_len, suffix, suffix_len);  // LCOV_EXCL_LINE
        }
        replacement[total_len] = '\0';
    } else {
        // Command completion - build "/" + selected + suffix
        size_t cmd_len = 1 + strlen(selected) + suffix_len;  // "/" + selected + suffix
        replacement = talloc_zero_size(repl, cmd_len + 1);
        if (replacement == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        replacement[0] = '/';
        memcpy(replacement + 1, selected, strlen(selected));
        if (suffix_len > 0) {
            memcpy(replacement + 1 + strlen(selected), suffix, suffix_len);
        }
        replacement[cmd_len] = '\0';
    }

    talloc_free(current_input);
    return replacement;
}

/**
 * @brief Update input buffer to show current completion selection
 *
 * Helper to update the input buffer with the currently selected completion.
 *
 * @param repl REPL context
 * @return res_t Result
 */
static res_t update_input_with_completion_selection(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(repl->current->completion != NULL); /* LCOV_EXCL_BR_LINE */

    char *replacement = build_completion_buffer_text(repl, repl->current->completion, NULL);
    if (replacement == NULL) {  // LCOV_EXCL_BR_LINE - defensive
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    // Replace input buffer with selected completion
    res_t res = ik_input_buffer_set_text(repl->current->input_buffer, replacement, strlen(replacement));
    talloc_free(replacement);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE - OOM in set_text
        return res;  // LCOV_EXCL_LINE
    }

    // Move cursor to end of line for completion acceptance
    res = ik_input_buffer_cursor_to_line_end(repl->current->input_buffer);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE - OOM in cursor move
        return res;  // LCOV_EXCL_LINE
    }

    return OK(NULL);
}

/**
 * @brief Handle Space key when completion is active - commit selection
 *
 * Commits the current selection by adding a space, then dismisses completion.
 * Only meant to be called when completion is active.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_completion_space_commit(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->current->completion == NULL) {  // LCOV_EXCL_BR_LINE - defensive
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    // Build text with space suffix
    char *replacement = build_completion_buffer_text(repl, repl->current->completion, " ");
    if (replacement == NULL) {  // LCOV_EXCL_BR_LINE - defensive
        ik_repl_dismiss_completion(repl);  // LCOV_EXCL_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    ik_repl_dismiss_completion(repl);

    // Replace input buffer with selected completion + space
    res_t res = ik_input_buffer_set_text(repl->current->input_buffer, replacement, strlen(replacement));
    talloc_free(replacement);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE - OOM in set_text
        return res;  // LCOV_EXCL_LINE
    }

    // Move cursor to end of line for space commit
    res = ik_input_buffer_cursor_to_line_end(repl->current->input_buffer);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE - OOM in cursor move
        return res;  // LCOV_EXCL_LINE
    }

    return OK(NULL);
}

/**
 * @brief Handle TAB action - completion trigger or cycle to next
 *
 * If completion is active: cycle to next match and update input buffer
 * If completion is not active: trigger completion
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_tab_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    // If completion is already active, cycle to next
    if (repl->current->completion != NULL) {
        // If original_input not set yet (first Tab), set it now
        if (repl->current->completion->original_input == NULL) {  // LCOV_EXCL_BR_LINE
            size_t text_len = 0;
            const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
            char *original = talloc_strndup(repl, text, text_len);
            if (original == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            repl->current->completion->original_input = original;
        }

        // Move to next candidate
        ik_completion_next(repl->current->completion);

        // Update input buffer with new selection
        res_t res = update_input_with_completion_selection(repl);
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
            ik_repl_dismiss_completion(repl);  // LCOV_EXCL_LINE
            return res;  // LCOV_EXCL_LINE
        }

        // Dismiss completion after accepting selection
        ik_repl_dismiss_completion(repl);
        return OK(NULL);
    }

    // No completion active - check if input starts with '/'
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);

    // Empty input or doesn't start with '/' - do nothing
    if (text_len == 0 || text[0] != '/') {
        return OK(NULL);
    }

    // Create null-terminated string for prefix
    char *prefix = talloc_zero_size(repl, text_len + 1);
    if (prefix == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    memcpy(prefix, text, text_len);
    prefix[text_len] = '\0';

    // Store original input for ESC revert (what user typed before first Tab)
    char *original_input = talloc_strndup(repl, prefix, text_len);
    if (original_input == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Check if input has a space (argument completion) or not (command completion)
    const char *space_pos = strchr(prefix, ' ');
    ik_completion_t *comp = NULL;

    if (space_pos != NULL) {
        // Argument completion
        comp = ik_completion_create_for_arguments(repl, repl, prefix);
    } else {
        // Command completion
        comp = ik_completion_create_for_commands(repl, prefix);
    }

    talloc_free(prefix);

    if (comp != NULL) {
        // Set original_input for ESC revert
        comp->original_input = original_input;
        repl->current->completion = comp;

        // Update input buffer with first selection
        res_t res = update_input_with_completion_selection(repl);
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
            ik_repl_dismiss_completion(repl);  // LCOV_EXCL_LINE
            return res;  // LCOV_EXCL_LINE
        }

        // Dismiss completion after accepting first selection
        ik_repl_dismiss_completion(repl);
    } else {
        talloc_free(original_input);
    }

    return OK(NULL);
}
