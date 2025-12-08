# Sub-Agent Receives Context

## Description

Sub-agent's entire starting context is defined by the `spawn_sub_agent` tool parameters. The sub-agent starts with a fresh scrollback containing only the system prompt and task prompt.

## Transcript

Parent agent calls:
```text
[Tool use: spawn_sub_agent]
  system_prompt: "You are a code reviewer. Be thorough but concise. Focus on security issues."
  prompt: "Review this authentication code for vulnerabilities:\n\n```python\ndef login(user, pwd):\n    if db.check(user, pwd):\n        return create_token(user)\n```"
```

Sub-agent 0/0 sees:
```text
───────── ↑0/ ←0/0 [0/0] →0/0 ↓- ─────────

[System: You are a code reviewer. Be thorough but concise. Focus on security issues.]

Review this authentication code for vulnerabilities:

```python
def login(user, pwd):
    if db.check(user, pwd):
        return create_token(user)
```

> _  [autonomous - no user input accepted]
```

## Walkthrough

1. Parent agent calls spawn_sub_agent with two parameters

2. `system_prompt` defines sub-agent's role and constraints

3. `prompt` defines the specific task

4. Sub-agent created with fresh, empty scrollback

5. System prompt set as system message (where API supports) or prepended

6. Prompt becomes first user message in conversation

7. Sub-agent has NO access to parent's conversation history

8. Sub-agent has NO knowledge of parent's context

9. This isolation is intentional (clean delegation boundary)

10. Sub-agent proceeds to work on its specific task

11. Sub-agent has same tool access as parent (including spawn_sub_agent)
