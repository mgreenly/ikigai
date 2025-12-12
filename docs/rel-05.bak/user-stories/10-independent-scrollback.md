# Independent Scrollback

## Description

Each agent maintains its own conversation history. Messages sent in one agent do not appear in another.

## Transcript

```text
───────────────────────── agent 0/ ─────────────────────────
> Hello from agent 0

Hi! I'm responding to agent 0.

> [Ctrl+Right]

───────────────────────── agent 1/ ─────────────────────────
> What have we discussed?

This is a fresh conversation. We haven't discussed anything yet.

> _
```

## Walkthrough

1. User is on agent 0/

2. User sends "Hello from agent 0"

3. LLM responds, message added to agent 0/'s scrollback and conversation

4. User presses Ctrl+Right to switch to agent 1/

5. Agent 1/'s scrollback is displayed (empty or different history)

6. User sends "What have we discussed?"

7. LLM responds based on agent 1/'s conversation (empty context)

8. Agent 1/'s response confirms no prior context

9. Each agent has independent `ik_scrollback_t` and `ik_openai_conversation_t`

10. Messages are persisted to database with respective agent_id
