## Overview

This file provides message creation functionality for the mail subsystem. It defines a single function that allocates and initializes a mail message object with sender and recipient identifiers, message content, and timestamp. Messages are consumed on read (via `wait`) so there is no read/unread status.

## Code

```
function create_mail_message(memory_context, from_user_id, to_user_id, message_text):
    allocate a new message object from the memory context

    if allocation fails:
        panic - out of memory condition

    populate the message:
        set sender user ID (allocate a copy)
        set recipient user ID (allocate a copy)
        set message body text (allocate a copy)
        record current timestamp
        initialize message ID to zero (will be assigned by database)

    return the new message object
```
