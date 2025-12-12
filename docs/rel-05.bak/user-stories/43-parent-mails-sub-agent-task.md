# Parent Mails Sub-Agent Task

## Description

A user (through their agent) sends a task to another agent via mail. This demonstrates the async delegation pattern where work is assigned without blocking the sender.

## Transcript

User on agent 0/ delegates a task to agent 1/:

```text
───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
> /mail send 1/ "Build the project and run all tests. Report back with results."

Mail sent to agent 1/

> I'll continue working on the documentation while agent 1/ handles the build.

Sure, I can help with the documentation. What would you like to...
```

Meanwhile, agent 1/ (which was IDLE) receives notification:

```text
───────── ↑- ←0/ [1/] →- ↓- ─────── [mail:1] ───────

[Notification: You have 1 unread message in your inbox]

> _
```

If agent 1/ has an autonomous system prompt, it may automatically check and act:

```text
[Notification: You have 1 unread message in your inbox]

I'll check my inbox.

[Tool use: mail]
  action: inbox

[Tool result]:
{"messages": [{"id": 8, "from": "0/", "unread": true, "preview": "Build the project and run all..."}], "unread_count": 1}

I have a task from agent 0/. Let me read the full message.

[Tool use: mail]
  action: read
  id: 8

[Tool result]:
{"id": 8, "from": "0/", "body": "Build the project and run all tests. Report back with results."}

I'll execute the build and tests now.

[Tool use: bash]
  command: "make check"
...
```

## Walkthrough

1. User on agent 0/ types `/mail send 1/ "Build the project..."`

2. Mail delivered to agent 1/'s inbox

3. User continues working on agent 0/ (not blocked)

4. Agent 1/ is IDLE, so notification is injected

5. If agent 1/ is autonomous (has appropriate system prompt):
   - Agent sees notification in context
   - Agent uses `mail` tool to check inbox
   - Agent reads the task message
   - Agent executes the requested work

6. If agent 1/ requires human:
   - User switches to agent 1/ (Ctrl+Right)
   - User sees notification and unread indicator
   - User can instruct agent or run `/mail read`

7. This pattern enables non-blocking task delegation

8. Results flow back via mail (see story 44)
