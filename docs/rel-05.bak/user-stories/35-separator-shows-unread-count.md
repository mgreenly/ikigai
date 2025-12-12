# Separator Shows Unread Count

## Description

When the current agent has unread mail, the separator displays an unread count indicator. This provides at-a-glance visibility into pending messages.

## Transcript

Agent 0/ has 2 unread messages:

```text
[scrollback content]

───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:2] ───────
> _
```

After receiving another message:

```text
[scrollback content]

───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:3] ───────
> _
```

## Walkthrough

1. Agent receives mail (via `/mail send` from another agent or `mail` tool)

2. Message added to agent's inbox with `read = false`

3. Agent's `unread_count` incremented

4. Separator layer queries current agent's `unread_count`

5. If `unread_count > 0`, separator appends `[mail:N]` indicator

6. Indicator positioned after navigation context, before trailing dashes

7. Format: `[mail:{count}]` - square brackets, literal "mail:", count

8. Count updates immediately when:
   - New mail arrives (count increases)
   - User reads a message (count decreases)
   - User switches to agent with different unread count

9. Indicator uses same styling as rest of separator (no special color in v1)

10. User can check mail with `/mail` to see message list
