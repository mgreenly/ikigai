## Overview

This file implements the kill command handler for terminating agents in the REPL. It provides two main operations: self-kill (killing the current agent) and targeted kill (killing another agent with optional cascade deletion of descendants). The kill command maintains database consistency through transactional semantics and synchronization barriers.

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

        // Record kill event in parent's history
        create metadata JSON with target UUID
        insert kill event message into database

        mark current agent as dead in registry
        switch navigation to parent agent
        remove current agent from memory
        update navigation context
        display "Agent terminated"

        return success

    // Case 2: Targeted kill with optional --cascade flag
    parse UUID and cascade flag from arguments

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

    // If cascade flag set, kill target and all descendants
    if cascade flag present:
        begin database transaction

        collect all descendants in depth-first order

        mark all descendants as dead in registry
        mark target agent as dead in registry

        create metadata JSON with cascade information
        insert kill event with cascade metadata

        commit transaction

        remove all victims from memory
        update navigation context
        display "Killed N agents"

        return success or rollback on error

    // Standard kill: target only
    record kill event in current agent's history
    mark target agent as dead in registry
    remove target from memory
    update navigation context
    display "Agent terminated"

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
