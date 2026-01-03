%title: Ikigai
%author: Michael Greenly
%date: 12/20/2025

---

-> This is a personal nights & weekends project. <-

-> All thoughts presented here are my own. <-

---

# Why am I building this?

Started because I wanted to understand how AI agents actually work.

Then I realized how fundamental this shift is.

I decided I'd rather ride the wave than watch it from shore.

---

# Two halves of one platform

**Ikigai** - the terminal
Where you build and run internal agents interactively.

**Iki Genba** - the runtime
Where you deploy long lived autonomous external agents to.

---

# The idea

Build agents at the terminal.
Deploy them to run on their own.

Persistent memory. Sub-agent coordination.
Agents that grow into their task.

---

# The goal

Not disposable chat sessions.

Agents that remember.
Agents that learn.
Agents that run on their own.

---

# Ikigai: at its core

A coding agent. Like Claude Code.

But built differently.

---

# Built for the terminal

Native C application.

Alternate buffer mode - your shell stays clean.

Mouse scrolling without blocking your terminal emulator.

Full UTF-8: accents, skin tones, emoji, CJK, combining characters.

International keyboard support via XKB.

Built for performance. Built to last.

---

# The REPL

You've seen this before.

Type a prompt. Get a response. Tools execute.

Nothing surprising here.

---

# Philosophy

If the user can do it, the agent can do it.

Minimal building blocks. Maximum composability.

---

# Minimal tools

-> bash <-
-> web_search <-
-> slash_command <-

That's it.

Everything else goes through bash.
The model already knows grep, git, jq, curl.
Why rebuild what works?

---

# Permanent history

Conversations persist to the database.

Close the terminal. Reopen days later.

Your conversation is still there.

---

# Replay into memory

On startup, history replays into the model's context.

The agent picks up where it left off.

No "remind me what we were doing."

---

# Multiple models

Switch models mid-conversation.

/model gpt-4
/model claude-sonnet
/model claude-opus

History stays. Context rebuilds.

---

# Sub-agents

Agents can spawn agents.

/fork "research the API"
/fork "write the tests"

Parent keeps working. Children report back.

---

# Agent coordination

/agents         - list all agents
/switch 2       - jump to agent 2
/send 1 "done"  - message another agent
/kill 3         - terminate an agent

Agents are processes. Fork, message, signal, kill.

---

# Sub-agents replay too

Each sub-agent has its own history.

Restart the app. All agents restore.

Parent and children resume their work.

---

# Context management

Long conversations fill up.

Most agents hide this. You hit a wall and start over.

---

# Explicit context control

/mark           - checkpoint
/rewind         - rollback to checkpoint
/forget <topic> - remove specific content
/remember <topic> - keep only this, forget the rest
/clear          - start fresh

You control what's in context. Always.

---

# The model helps

> /forget the database schema discussion

The model finds matching messages.
Shows you what it will remove.
You confirm. Done.

The planning conversation cleans itself up after.

---

# Unified paths

Files:    src/main.c
Memory:   ikigai:///blocks/decisions.md

Same tools work on both.

Read, write, edit - the agent doesn't care where it lives.

---

# Memory blocks

Persistent documents in the database.

ikigai:///blocks/patterns.md
ikigai:///blocks/api-notes.md
ikigai:///blocks/project-decisions.md

Agents read them. Agents write them.
Knowledge accumulates.

---

# Internal vs External agents

**Internal**: You're at the terminal. Human-driven.

**External**: Code-driven. Runs on its own.

Both share the same database.
The same tools.
The same identity model.

---

# Iki-genba

The runtime for external agents.

(more on this next...)

---
