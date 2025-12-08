# Observe Working Sub-Agent

## Description

User can navigate to a working sub-agent and watch its progress in real-time. The sub-agent's scrollback shows streaming responses and tool executions.

## Transcript

```text
───────── ↑- ←1/ [0/] →1/ ↓0/0 ─────────
[Agent 0/ blocked on spawn_sub_agent tool...]

> [Ctrl+Down]

───────── ↑0/ ←0/0 [0/0] →0/0 ↓- ─────────

[System: You are a security analyst.]

Analyze this code for SQL injection vulnerabilities.

I'll analyze the code systematically...

[Streaming response in progress...]
The code has several potential SQL injection points:

1. Line 42: `query = f"SELECT * FROM users WHERE id = {user_id}"`
   This directly interpolates user input...

[Tool use: read_file]
  path: "src/db/queries.py"

[Tool result]: "..."

2. Line 87: Another vulnerable query...

> _  [observing - input disabled]
```

## Walkthrough

1. User is on agent 0/ which is blocked waiting for sub-agent 0/0

2. Separator shows `↓0/0` indicating child exists

3. User presses Ctrl+Down to navigate to sub-agent

4. User switches to agent 0/0's view

5. Sub-agent's scrollback is displayed

6. User sees sub-agent's conversation history

7. If sub-agent is streaming, user sees live output

8. If sub-agent uses tools, user sees tool calls and results

9. Input area shows sub-agent is autonomous (input disabled indicator)

10. User can watch progress without interfering

11. User can navigate away (Ctrl+Up) at any time

12. Sub-agent continues working regardless of user observation
