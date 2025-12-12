# Agent Spawns Sub-Agent

## Description

An agent uses the `spawn_sub_agent` tool to create a child agent for task delegation. The parent blocks until the sub-agent completes and receives its result.

## Transcript

```text
───────── ↑- ←1/ [0/] →1/ ↓- ─────────
> Research OAuth 2.0 patterns and then implement login

I'll delegate the research to a sub-agent while I prepare the implementation.

[Tool use: spawn_sub_agent]
  system_prompt: "You are a security researcher. Be concise."
  prompt: "Research OAuth 2.0 best practices for web applications. Focus on PKCE flow."

[Spawning sub-agent 0/0...]

───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────

[Sub-agent 0/0 working...]
[Sub-agent 0/0 completed]

[Tool result from 0/0]:
"OAuth 2.0 with PKCE is recommended for SPAs. Key points:
1. Use authorization code flow with PKCE
2. Never store tokens in localStorage
3. Use httpOnly cookies for refresh tokens
..."

Based on the research, I'll implement the login flow using PKCE...
```

## Walkthrough

1. User asks agent 0/ to research and implement

2. Agent 0/ decides to delegate research to sub-agent

3. Agent 0/ calls `spawn_sub_agent` tool with system_prompt and prompt

4. Tool handler validates depth < 4 and agent count < 20

5. Tool handler generates agent_id: "0/0" (parent_id + "/" + serial)

6. Tool handler creates new `ik_agent_ctx_t` for sub-agent

7. Sub-agent initialized with fresh scrollback, system_prompt, and prompt

8. Sub-agent added to parent's children array

9. Parent enters EXECUTING_TOOL state (blocked)

10. Separator updates to show `↓0/0` (new child exists)

11. Sub-agent executes autonomously (LLM calls, possible tool use)

12. Sub-agent reaches completion (would-wait-for-human state)

13. Sub-agent's final response captured as tool result

14. Sub-agent resources cleaned up

15. Parent exits EXECUTING_TOOL, receives result

16. Parent continues working with the research information
