## Overview

This file implements the kill and reap command handlers for terminating and cleaning up agents in the REPL. Kill marks agents (and all descendants) as dead but keeps them in the nav rotation for user review. Reap removes all dead agents. The kill command maintains database consistency through transactional semantics and synchronization barriers.

## Code

```
function kill_command(repl, args):
    // Sync barrier: wait for any pending fork operations to complete
    wait until fork operations are not pending

    // Case 1: No arguments - kill self
    if no args or empty args:
        if current agent is root:
            display error "Cannot kill root agent"
            return success

        get parent agent context
        if parent not found:
            return error "Parent agent not found"

        // Kill self + all descendants (always cascades)
        victims = collect_descendants(repl, current_agent_uuid)
        append current agent to victims

        begin database transaction

        for each victim in victims:
            mark victim as dead in registry
            NOTIFY agent_event_<victim_parent_uuid> (wake any waiting parent)

        create metadata JSON with target UUID and victim list
        insert kill event message into database

        commit transaction

        // Mark dead in memory (keep in agents[] for /reap)
        for each victim in victims:
            set victim.dead = true

        switch navigation to parent agent
        display "Agent terminated"

        return success

    // Case 2: Targeted kill
    parse UUID from arguments

    find target agent by UUID (partial match allowed)
    if target not found:
        if UUID is ambiguous:
            display error "Ambiguous UUID prefix"
        else:
            display error "Agent not found"
        return success

    if target is root agent:
        display error "Cannot kill root agent"
        return success

    // If targeting self, use self-kill logic
    if target is current agent:
        recursively call kill with no args
        return

    // Kill target + all descendants (always cascades)
    victims = collect_descendants(repl, target_uuid)
    append target to victims

    begin database transaction

    for each victim in victims:
        mark victim as dead in registry
        NOTIFY agent_event_<victim_parent_uuid> (wake any waiting parent)

    create metadata JSON with cascade information
    insert kill event with victim metadata

    commit transaction

    // Mark dead in memory (keep in agents[] for /reap)
    for each victim in victims:
        set victim.dead = true

    display "Killed N agents"

    return success

function reap_command(repl, args):
    // Remove all dead agents from nav rotation

    count dead agents in repl->agents[]
    if count is 0:
        display "Nothing to reap"
        return success

    // If currently viewing a dead agent, switch first
    if current agent is dead:
        find first non-dead agent
        switch navigation to it

    // Remove all dead agents from array
    for each agent in repl->agents[] (reverse order):
        if agent.dead:
            remove from array
            free memory (talloc handles descendants)

    display "Reaped N dead agents"
    return success

function collect_descendants(repl, agent_uuid):
    // Recursively collect all child agents in depth-first order
    result = empty array

    for each agent in agents:
        if agent's parent UUID matches given UUID:
            recursively collect this agent's descendants
            add to result
            add this agent to result

    return result
```
