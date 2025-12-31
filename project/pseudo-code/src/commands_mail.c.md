## Overview

Mail command handlers for the REPL. Implements five mail operations: sending messages to other agents, checking inbox, reading individual messages, deleting messages, and filtering inbox by sender. All commands validate input, interact with the database, and display results through the scrollback (terminal output).

## Code

```
function send(context, repl_session, arguments):
    validate input is not empty
    parse sender UUID and quoted message from arguments

    locate recipient agent by UUID in current REPL session
    if recipient not found, display error and return

    query database for recipient agent status
    if recipient is not running, display error and return

    if message body is empty, display error and return

    create mail message object with sender, recipient, and body
    insert mail message into database

    display confirmation "Mail sent to [recipient]"
    return success

function check_mail(context, repl_session, arguments):
    retrieve all messages in current agent's inbox from database

    if inbox is empty, display "No messages" and return

    count unread messages
    display summary header: "Inbox (N messages, M unread):"

    display each message in the inbox (formatted list)
    return success

function read_mail(context, repl_session, arguments):
    parse message index (1-based) from arguments
    if index is missing or invalid, display error and return

    retrieve all messages in current agent's inbox from database
    if inbox is empty or index is out of range, display error and return

    get the message at the specified index

    display message header showing sender UUID (truncated)
    display blank line for readability
    display message body

    mark the message as read in the database
    return success

function delete_mail(context, repl_session, arguments):
    parse message index (1-based) from arguments
    if index is missing or invalid, display error and return

    retrieve all messages in current agent's inbox from database
    if inbox is empty or index is out of range, display error and return

    get the message at the specified index

    delete message from database (validates ownership internally)
    if deletion fails because message not found or not owned, display error and return

    display confirmation "Mail deleted"
    return success

function filter_mail(context, repl_session, arguments):
    parse --from <uuid> flag from arguments
    if flag missing or malformed, display error and return

    locate sender agent by UUID (supports partial matches)
    if sender not found, display error (check for ambiguous UUID)
    if sender not found, display error and return

    retrieve messages in current agent's inbox from the specified sender

    if no messages from sender, display "No messages from [sender]" and return

    count unread messages
    display filtered header: "Inbox (filtered by [sender], N messages, M unread):"

    display each message in the filtered inbox (same format as check_mail)
    return success
```
