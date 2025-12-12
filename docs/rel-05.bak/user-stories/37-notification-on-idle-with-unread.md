# Notification on IDLE with Unread Mail

## Description

When an agent transitions to IDLE state and has unread mail, a notification is automatically injected into the conversation. This prompts the agent (or user) to check their inbox.

## Transcript

Agent 1/ finishes a task and becomes IDLE with unread mail:

```text
[Previous conversation...]

I've completed the file analysis. The codebase has 47 source files.

[Notification: You have 2 unread messages in your inbox]

> _
```

## Walkthrough

1. Agent 1/ completes LLM response (no tool call pending)

2. Agent transitions to IDLE state

3. IDLE transition handler checks `agent->inbox->unread_count`

4. `unread_count > 0` (agent has unread mail)

5. Handler checks `agent->mail_notification_pending` flag

6. Flag is false (no pending notification)

7. Handler creates notification message:
   - Content: `[Notification: You have N unread messages in your inbox]`
   - Singular/plural: "message" vs "messages" based on count

8. Notification injected as `role: user` message into conversation

9. Notification appended to scrollback (visible to user)

10. `mail_notification_pending` set to true (prevents repeat)

11. Notification included in next LLM context (agent can act on it)

12. If agent is autonomous (sub-agent), it may use `mail` tool to check inbox

13. If user is viewing this agent, they see the notification and can act
