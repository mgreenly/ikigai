## Overview

This file provides helper functions for restoring agent state during replay operations. It reconstructs the agent's conversation history, scrollback buffer, and navigation marks from replay event data, allowing the agent to resume from a previous point in execution with full context restored.

## Code

```
function populate_conversation_from_replay(agent, replay_context, logger):
    validate that agent and replay_context exist

    for each message in the replay context:
        if the message is a conversation-related kind:
            take ownership of the message and add it to the agent's conversation

            if adding the message fails:
                log a warning event with:
                    - event type: "conversation_add_failed"
                    - the agent's UUID
                    - the error message
                    skip to next message

    (all conversation messages have been replayed into the agent's context)


function populate_scrollback_from_replay(agent, replay_context, logger):
    validate that agent and replay_context exist

    for each message in the replay context:
        attempt to render the message to the scrollback buffer
        (rendering interprets the message kind and content to update UI state)

        if rendering fails:
            log a warning event with:
                - event type: "scrollback_render_failed"
                - the agent's UUID
                - the error message
            skip to next message

    (all messages have been rendered to scrollback)


function restore_marks_from_replay(agent, replay_context):
    validate that agent and replay_context exist

    if replay_context has saved marks:
        allocate a mark array in the agent

        for each mark in the replay mark stack:
            allocate a new mark

            copy the message index from the replay mark

            if the replay mark has a label:
                copy the label string
            else:
                mark has no label

            clear the timestamp field

            add the mark to the agent's mark array

    (agent marks have been restored to their saved state)
```
