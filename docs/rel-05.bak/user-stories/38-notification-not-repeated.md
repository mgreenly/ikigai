# Notification Not Repeated

## Description

Once a mail notification is injected, it is not repeated until the agent checks their inbox. This prevents notification spam when an agent repeatedly becomes IDLE.

## Transcript

Agent receives notification, ignores it, does other work:

```text
[Notification: You have 2 unread messages in your inbox]

> What's the weather like?

I don't have access to weather information, but I can help with...

> _
```

No second notification appears even though agent is IDLE again with unread mail.

## Walkthrough

1. Agent becomes IDLE with unread mail

2. Notification injected: "[Notification: You have 2 unread messages...]"

3. `agent->mail_notification_pending` set to true

4. User (or LLM) responds with unrelated query

5. Agent processes query, sends to LLM, receives response

6. Agent transitions back to IDLE

7. IDLE handler checks `unread_count > 0` (still true)

8. Handler checks `mail_notification_pending` (true)

9. Flag is true, so NO new notification is injected

10. User continues without notification spam

11. Notification flag is cleared when:
    - Agent uses `mail` tool with `action: inbox`
    - Agent uses `mail` tool with `action: read`
    - User runs `/mail` or `/mail read`

12. After checking mail, flag resets, next IDLE can trigger new notification
