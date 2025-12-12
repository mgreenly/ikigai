# Notification Visible in Scrollback

## Description

Mail notifications appear in the scrollback and are visible to the user. They are styled differently (dimmed) to distinguish them from regular conversation content, but are never hidden.

## Transcript

```text
I've finished analyzing the repository structure.

[Notification: You have 1 unread message in your inbox]

> /mail

Inbox for agent 0/:
  #3 [unread] from 1/ - "Analysis complete. Found 3 security issues..."

> _
```

The notification line `[Notification: ...]` appears dimmed compared to regular text.

## Walkthrough

1. Notification is created as a message with special formatting

2. Message added to scrollback buffer like any other content

3. Scrollback renderer detects `[Notification: ` prefix

4. Renderer applies dim styling (ANSI dim attribute or reduced color)

5. Notification remains fully readable, just visually de-emphasized

6. Notification is part of scrollback history (can scroll up to see it)

7. Notification included in LLM context (agent sees it and can reason about it)

8. User can always see what triggered agent behavior

9. Principle: Nothing hidden from user - all system activity visible

10. Multiple notifications over time create visible history:
    ```
    [Notification: You have 1 unread message in your inbox]

    I'll check my inbox now.

    [Tool use: mail] action: inbox
    ...

    [Notification: You have 2 unread messages in your inbox]
    ```

11. Dim styling prevents notifications from dominating visual attention
