-> **Ikigai** is a nights & weekends, personal project. <-

-> All thoughts presented here are my own. <-

-> So, Blame me not anyone else! <-

---

-> **Ikigai:** <-

-> The intersection of what you love, what you are good at, <-

-> what the world needs, and what you can be paid for. <-

---

# What is Ikigai?

The easiest place to start, is to say that **Ikigai** is yet another coding agent just like
Claude-Code, Gemini-CLI, Codex-CLI, OpenCode, etc, etc, etc...

The rest of this presentation will be dedicated to convincing you there are meaningful
differences.

---

-> Also, as we discuss this, you should know that <-

-> **Ikigai** is 100% context engineered, that's part of the experiment. <-

-> I want to understand the limits of these techniques. <-

---

# Context Management

Most people now understand that agentic development is all about carefully controlling
context. Some people call this "Context Engineering", and recently Google coined the term
"Context Driven Development" which I really like.

I also think the world is quickly waking up to the realization that sub-agents are an
invaluable tool in helping to manage context and as a bonus they can do work in parallel!

I would also add that access to the right tools is critical.

Pretty much everything in **Ikigai** is about making it easier and more efficient to take
advantage of those ideas.


---

-> # Some Foreshadowing <-

-> Multi-line UTF-8 REPL/Editor <-
-> Multiple AI Providers <-
-> Permanent Message History <-
-> Hierarchical Agents <-
-> Inter Agent Communication <-
-> Database File Store <-
-> Internal & External Tools <-
-> Skill Sets, Tool Sets and Modes <-
-> Structured Context <-
-> Historical Message Summaries <-
-> Sliding Context Window <-
-> External Agents <-

---

# UTF-8 REPL/Editor

This is pretty fundamental, but Ikigai will have full UTF-8 and international keyboard
support. The only thing it's currently lacking is support for multi-codepoint zero width
joiners, used for things like the family emoji and in complex languages like Arabic.

works: 🐶 🐱 🦊 🐼 🦁
broke: 👨‍👩‍👧‍👦

---

# Multiple Providers

I don't think this needs much explanation, anyone using a proprietary tool, limited to
one providers models, wishes they had access to a larger catalog of models.

**Ikigai** currently supports Anthropic, OpenAI, and Google and that list will expand.
The goal isn't just to support them though, the goal is to fully support them!  This means
using an internal superset representation of their capabilities and maybe even polyfilling
missing functionality when necessary.

---

# Permanent Message History

One of the first things that I decided I wanted was permanent message history. I wanted
to move away from the session based in-memory representation of conversations that other
agents seemed to be using to a permanent database backed representation of events.

I'm running on a computer with gigabytes of ram and terabytes of storage, why should my
agent ever lose access to any history unless I intentionally want it to?

To be fair, Claude Code has addressed this some what, it has --fork-session, --continue,
and --resume but native forever history is still a much better plan.

---

# Hierarchical Agents

Explicit Agent Management

Using `/fork` and `/kill`

---

# Inter Agent Communication

Using `/send` and `/wait`

---

# Pinned Documents

Using `/pin` and `/unpin` to build the system prompt from documents

---

# External Tools

---

# Internal Tools

---

# Skill Sets

---

# Tool Sets

---

# Modes

---

# Structured Context

  * System prompt, system tools, memory files, messages.
  * It lists skills.

  [ [system-prompt],
    [pinned-files],
    [skill-set],
    [tool-set],
    [historical-summary],
    [message-history] ]

---

# Historical Message Summaries

--

# Sliding Context Window

Using **forget** and **recall**
