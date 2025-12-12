# Max Depth Spawn Error

## Description

When an agent at maximum depth (4) attempts to spawn a sub-agent, the tool returns an error. The spawning agent can handle this gracefully.

## Transcript

```text
Agent 0/0/0/0 (depth 4) attempts to spawn:

[Tool use: spawn_sub_agent]
  system_prompt: "You are a helper."
  prompt: "Do this small task."

[Tool error]:
"Error: Maximum agent depth (4) exceeded. Cannot spawn sub-agent."

I apologize, but I cannot delegate this task further as we've reached
the maximum nesting depth. I'll handle it directly instead...
```

## Walkthrough

1. Agent 0/0/0/0 is at depth 4 (maximum allowed)

2. Agent attempts to call spawn_sub_agent tool

3. Tool handler calculates current depth from agent_id

4. Depth calculation: count path separators + 1 = 4

5. New sub-agent would be depth 5, exceeds max (4)

6. Tool handler returns error result (not exception)

7. Error message: "Error: Maximum agent depth (4) exceeded. Cannot spawn sub-agent."

8. Agent receives error as tool result

9. Agent can handle gracefully (do task itself, inform user, etc.)

10. No sub-agent created, no resources allocated

11. Same error applies if global agent count >= 20
