## Overview

Send and wait command handlers for the REPL. Implements two operations: sending messages to other agents, and waiting for messages with optional fan-in semantics. Both commands are available as slash commands (human-initiated) and as internal tools (agent-initiated).

## Code

```
function send(context, repl_session, arguments):
    validate input is not empty
    parse recipient UUID and quoted message from arguments

    locate recipient agent by UUID in current REPL session
    if recipient not found, display error and return

    query database for recipient agent status
    if recipient is dead, display error and return

    if message body is empty, display error and return

    create mail message object with sender, recipient, and body
    insert mail message into database

    NOTIFY agent_event_<recipient_uuid> to wake any waiting recipient

    display confirmation "Message sent to [recipient]"
    return success

function wait_command(context, repl_session, arguments):
    parse timeout (required) from arguments
    parse optional list of agent UUIDs from arguments

    put current agent into tool-executing state
    spawn worker thread with wait_core logic
    (main loop continues — spinner runs, other agents serviced)
    (escape interrupts — same as any tool execution)

    on completion: render result to scrollback

function wait_core(agent, timeout, from_agents):
    // This runs on the worker thread
    // Shared by both slash command and internal tool paths

    LISTEN agent_event_<my_uuid> on worker_db_ctx

    if from_agents is empty (mode 1 — next message):
        check for existing mail (consume first match)
        if found, UNLISTEN, return message as result

        loop:
            select() on PG socket fd with remaining timeout
            if interrupted, UNLISTEN, return empty/error
            if timeout expired, UNLISTEN, return empty/error

            process PG notifications
            check for mail (consume first match)
            if found, UNLISTEN, return message as result

    else (mode 2 — fan-in):
        initialize results array with one entry per agent_id (status: running)

        check for existing mail from each agent (consume matches, mark received)
        check if any target agents are dead (mark dead)
        if all resolved (received or dead), UNLISTEN, return results

        loop:
            select() on PG socket fd with remaining timeout
            if interrupted, UNLISTEN, return current state
            if timeout expired, UNLISTEN, return current state

            process PG notifications
            check for new mail from target agents (consume, mark received)
            check for newly dead agents (mark dead)
            if all resolved, UNLISTEN, return results

    UNLISTEN before returning
```
