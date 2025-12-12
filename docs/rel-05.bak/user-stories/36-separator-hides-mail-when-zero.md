# Separator Hides Mail Indicator When Zero

## Description

When the current agent has no unread mail, the separator does not display any mail indicator. This keeps the UI clean when there's nothing to act on.

## Transcript

Agent 0/ has no unread messages:

```text
[scrollback content]

───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
> _
```

After reading the last unread message:

```text
> /mail read 5

From: 1/
Time: just now

Task completed successfully.

───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
> _
```

## Walkthrough

1. Agent's inbox has `unread_count == 0`

2. Separator layer queries current agent's `unread_count`

3. Count is zero, so no `[mail:N]` indicator is rendered

4. Separator shows only navigation context and dashes

5. When user reads the last unread message:
   - `unread_count` decrements to 0
   - Separator re-renders without mail indicator
   - Transition is immediate (same render cycle)

6. When switching to agent with no unread mail:
   - New agent's `unread_count` is 0
   - Separator renders without indicator

7. No "[mail:0]" is ever shown - absence of indicator means zero

8. Clean separator indicates no pending action required
