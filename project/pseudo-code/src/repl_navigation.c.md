## Overview

This module implements agent navigation within the REPL, allowing users to switch between agents and traverse the agent tree structure (parent, children, siblings). It maintains navigation context on the separator layer that displays the current navigation state visually.

## Code

```
function switch_agent(repl, new_agent):
    validate repl is not null

    if new_agent is null:
        return error "Cannot switch to NULL agent"

    if new_agent is already current agent:
        return success (no operation needed)

    # Agent state (input buffer, viewport) is already stored per-agent
    # so no explicit save/restore is needed

    update current agent pointer to new_agent
    update navigation context for new agent's separator display

    return success


function navigate_to_previous_sibling(repl):
    validate repl and current agent exist

    get parent uuid of current agent

    # Collect all siblings (agents with same parent)
    siblings = []
    for each agent in active agents:
        if agent shares same parent as current agent:
            add agent to siblings

    if only 0 or 1 sibling exists:
        return success (nothing to navigate to)

    # Find current index in sibling list and wrap around to previous
    current_index = find position of current agent in siblings
    previous_index = (current_index == 0) ? last_index : current_index - 1

    switch to siblings[previous_index]
    return success


function navigate_to_next_sibling(repl):
    validate repl and current agent exist

    get parent uuid of current agent

    # Collect all siblings (agents with same parent)
    siblings = []
    for each agent in active agents:
        if agent shares same parent as current agent:
            add agent to siblings

    if only 0 or 1 sibling exists:
        return success (nothing to navigate to)

    # Find current index in sibling list and wrap around to next
    current_index = find position of current agent in siblings
    next_index = (current_index + 1) % sibling_count

    switch to siblings[next_index]
    return success


function navigate_to_parent(repl):
    validate repl and current agent exist

    if current agent has no parent:
        return success (already at root)

    # Find parent agent by UUID (may be null if parent was killed)
    parent = find agent by UUID of current agent's parent

    if parent exists:
        switch to parent

    return success


function navigate_to_child(repl):
    validate repl and current agent exist

    # Find the most recently created running child of current agent
    newest_child = null
    newest_time = 0

    for each agent in active agents:
        if agent's parent is current agent:
            # If first candidate or newer than current best:
            if newest_child is null OR agent creation time > newest_time:
                newest_child = agent
                newest_time = agent creation time

    if a child was found:
        switch to newest_child

    return success


function update_navigation_context(repl):
    validate repl is not null

    if current agent or its separator layer is missing:
        return early

    parent_uuid = current agent's parent UUID
    prev_sibling = null
    next_sibling = null
    child_count = 0

    # Scan all agents to compute navigation state
    for each agent in active agents:
        skip if agent is current agent

        # Count children (agents whose parent is current agent)
        if agent's parent is current agent:
            increment child_count

        # Skip if not a sibling
        if agent doesn't have same parent as current agent:
            continue

        # This agent is a sibling - determine if previous or next
        if agent created before current agent:
            # This is a previous sibling candidate
            if prev_sibling is null:
                prev_sibling = agent's UUID
            else:
                # Keep the most recent previous sibling
                if agent's creation time > previous sibling's creation time:
                    prev_sibling = agent's UUID
        else:
            # This is a next sibling candidate
            if next_sibling is null:
                next_sibling = agent's UUID
            else:
                # Keep the earliest next sibling
                if agent's creation time < next sibling's creation time:
                    next_sibling = agent's UUID

    # Update separator layer with computed navigation context
    set separator navigation context to:
        parent UUID
        previous sibling UUID
        current agent UUID
        next sibling UUID
        child count
```
