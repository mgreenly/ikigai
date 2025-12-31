## Overview

This file implements tab completion logic for the REPL, providing smart matching and suggestion cycling for both command names and command arguments. It uses fuzzy matching (fzy) to filter candidates and maintains completion state for interactive iteration through suggestions.

## Code

```
// Constants
MAX_COMPLETIONS: maximum number of suggestions to return (15)

// Provider Functions (per-command argument completers)

provide_model_args():
    return hardcoded list of available models
    (claude-opus-4-5, claude-sonnet-4-5, gpt-4o, etc.)

provide_debug_args():
    return hardcoded list ["off", "on"]

provide_rewind_args(repl):
    extract all labeled marks from the repl's mark history
    if no labeled marks exist, return empty result
    allocate array of mark labels
    iterate through repl marks and collect those with labels
    return collected mark labels

// Public Functions for Command Completion

ik_completion_create_for_commands(prefix):
    validate inputs (prefix must start with '/')
    extract search string by removing leading '/'

    fetch all registered commands from the command registry
    build array of command names as candidates

    use fzy fuzzy matching to filter and score candidates against search string
    limit results to MAX_COMPLETIONS

    if no matches found, return null

    allocate completion context structure
    copy matching candidates into the context (up to match count)
    store the original prefix
    set current selection index to 0

    return completion context

// Public Functions for Argument Completion

ik_completion_create_for_arguments(repl, input):
    validate inputs

    parse input to find the space separating command from arguments
    if no space found (command-only input), return null

    extract command name (skip leading '/')
    extract argument prefix (text after the space)

    dispatch to appropriate argument provider based on command name:
        if "model": call provide_model_args()
        if "debug": call provide_debug_args()
        if "rewind": call provide_rewind_args()
        else: return null (no completion for this command)

    if provider returned no arguments, return null

    use fzy fuzzy matching to filter and score arguments against prefix
    limit results to MAX_COMPLETIONS

    if no matches found, return null

    allocate completion context structure
    copy matching arguments into the context
    store the original input as prefix (for prefix validation)
    set current selection index to 0

    return completion context

// Completion Navigation and State Management

ik_completion_clear(completion):
    reset count and current index to 0
    free and clear the candidates array
    free and clear the prefix string
    free and clear the original_input string

ik_completion_get_current(completion):
    if no candidates available (count == 0), return null
    return the candidate at current index

ik_completion_next(completion):
    advance current index to next candidate (wrap around at end)

ik_completion_prev(completion):
    move current index to previous candidate
    if at start, wrap to last candidate

// Prefix Validation

ik_completion_matches_prefix(completion, current_input):
    check if current input still matches the stored prefix
    (used to determine if completion is still valid as user types)

    if current input is shorter than prefix length, return false
    return true if current input starts with the stored prefix
```
