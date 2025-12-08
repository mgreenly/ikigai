# Separator Shows Agent

## Description

The separator line displays the current agent ID, providing visual feedback about which agent is active.

## Transcript

```text
[scrollback content here]

───────────────────────── agent 0/ ─────────────────────────
> _
```

After switching to agent 1/:

```text
[different scrollback content]

───────────────────────── agent 1/ ─────────────────────────
> _
```

## Walkthrough

1. Separator layer renders between scrollback and input

2. Separator includes agent ID in center of line

3. Format: dashes + " agent {id} " + dashes

4. Agent ID comes from `current_agent->agent_id`

5. On agent switch, separator re-renders with new ID

6. User always knows which agent is current

7. Separator width adjusts to terminal width

8. Agent ID is the only dynamic content in Phase 1

9. Future: separator may show state, tokens, model
