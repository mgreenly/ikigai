## Overview

The `/agents` command displays a hierarchical tree view of all running and dead agents in the system, showing parent-child relationships and current status. It counts and summarizes agents by status at the end.

## Code

```
function cmd_agents(context, repl, args):
    allocate temporary memory

    display header: "Agent Hierarchy:"
    display blank line

    load all agents from database

    initialize counters for running and dead agents
    create a queue for processing agents in breadth-first order

    find all root agents (those with no parent) and add to queue

    while there are agents to process in queue:
        dequeue an agent and its depth level

        count the agent's status (running or dead)

        build display line:
            add marker (asterisk if current agent at root, space otherwise)

            if agent is a child (not root):
                indent with 4 spaces per depth level above root
                add tree branch characters "+-- "

            append full UUID

            append status in parentheses

            if agent is a root:
                append label " - root"

        display the formatted line

        find all child agents (with this agent as parent) and enqueue them
        (they will be processed at next depth level)

    display blank line before summary

    display summary: "X running, Y dead"

    clean up memory

    return success
```
