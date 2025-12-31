# What is Ikigai

**An Agent Platform for Building Systems that Compound**

---

## The Short Version

Ikigai is an agent platform - an operating environment where you build, deploy, and coordinate living agents.

Instead of using one AI assistant, you build agent systems: trees of coordinating agents that fork like processes, communicate through mailboxes, learn continuously through structured memory, and run autonomously. These aren't chatbots waiting for prompts - they're living processes that understand your project, coordinate with each other, and evolve over time.

You invest in building and refining these agent systems. The terminal (ikigai) is where you design agents interactively. The runtime (iki-genba) is where they run autonomously. Through structured memory, agents learn from every conversation. Through forking, they spawn children to delegate work. Through mailboxes, they coordinate without you. Your efforts compound because you're not just building features - you're building agent capabilities that improve themselves.

---

## The Problem

You have an ambitious idea: a new service, a complex platform, a real-world system. You can see where it needs to go. But there are only so many hours and only so many people.

The usual options are limiting:

- **Do everything yourself**: Every feature, deployment, and on-call incident routes through you. You become the bottleneck.
- **Hire a larger team**: Slow to spin up, expensive to maintain, and you spend more time coordinating than creating.
- **Glue together existing tools**: You assemble a stack of services, each with its own dashboards, configs, and quirks. You spend a large fraction of your time wiring systems together instead of moving your core idea forward.

What if your project came with its own "operational brain"? Not a passive assistant that waits for instructions, but agents that understand your system, act within clear boundaries, ask for guidance when needed, and get better every week?

---

## How It Works

Ikigai is a platform for building agent systems. It requires a shift in thinking: you're not using an assistant, you're building a team of coordinating agents.

**1. The Agent Platform**

You work in the ikigai terminal - an interactive environment for designing agents. Let's say you're building "Videos.com." You create agents that understand the codebase, deployment, infrastructure, and operations. This is your agent system - the collection of coordinating agents that help you build and operate Videos.com.

**2. Process Model**

Agents are processes. They fork children (`/fork`), send messages via mailboxes (`/send`), receive signals (`/kill`), and share state through StoredAssets. Like Unix processes, but for AI agents. Like Erlang actors, but with structured memory.

A parent agent delegates work by forking children with specific prompts. Children inherit the parent's conversation history and StoredAssets. They work autonomously, coordinate through messages, and report back.

**3. Structured Memory**

Agents don't just have conversation history - they have structured memory:
- **Pinned blocks** (100k tokens): Always-visible curated knowledge
- **Auto-summary** (10k): Index to archival memory
- **Sliding window** (90k): Recent conversation, auto-evicts oldest
- **Archival** (unlimited): Everything forever, searchable

Agents learn through `/remember` (extract knowledge), `/compact` (compress blocks), and `/recall` (search archival). Knowledge compounds over time.

**4. Dual Runtime**

The terminal (ikigai) is where you design agents interactively - human-driven. The runtime (iki-genba) is where agents run autonomously - code-driven. Both share the same database, the same process model, the same memory system. Human agents and autonomous agents coordinate seamlessly.

Deploy agents to iki-genba to run continuously: monitoring logs, processing queues, responding to webhooks, coordinating with other agents. They run whether you're at the terminal or not.

**5. Unified Interface**

The same tools work everywhere. `ikigai:///blocks/decisions.md` is a StoredAsset in the database. `src/auth.c` is a file on disk. Same read/write/edit tools for both. Agents don't care about storage backends - paths are paths.

Commands work on both too: `/compact` compresses StoredAssets or filesystem files. `/remember` extracts knowledge to blocks or docs. `/forget` removes content from either. Same operations, different storage.

**6. Continuous Evolution**

There's no single "launch day" for your agent system. You build it incrementally. You start with one agent in the terminal, fork children for specific tasks, pin knowledge they discover, deploy some to iki-genba for continuous operation.

As the system grows, agents accumulate knowledge in StoredAssets. Parent agents delegate more to children. Autonomous agents handle routine work. You guide the system, refine agent prompts, curate StoredAssets. The agent capabilities compound.

---

## What Kinds of Projects?

Ikigai isn't for one specific type of project. It's for any situation where a small team wants to operate at a larger scale:

- **A startup** building a new service, where a handful of developers need the operational capacity of a larger team
- **A research group** managing experiments, literature, and data pipelines that would otherwise require dedicated staff
- **A business** building internal tools (logistics, customer management, analytics) without hiring a full IT department
- **A consultancy** creating client-facing platforms that need to run reliably without constant attention

The common thread: you have something ambitious to build, limited people to build it, and you want your effort to compound over time rather than staying linear.

---

## What Makes This Different

**vs AI Coding Assistants** (Cursor, Copilot, Claude Code):
Those are single-agent assistants that help *you* code. Ikigai is a platform where you build *agent systems* - trees of coordinating agents that work together.

**vs Agent Frameworks** (LangChain, CrewAI, AutoGPT):
Those are libraries for building agents. Ikigai is a complete platform: builder (terminal), runtime (iki-genba), memory system (structured memory), and process model (fork, mailbox, signals).

**vs Memory Systems** (Letta/MemGPT):
Letta pioneered memory blocks. Ikigai extends this with: larger blocks (100k budget vs 2k chars), sliding window eviction, active compression (`/compact`), unified file interface (`ikigai://`), and agent process model.

**vs Workflow Orchestration** (Temporal, Airflow):
Those run predefined workflows. Ikigai runs intelligent agents that adapt, learn, coordinate, and evolve. Agents fork children, send messages, share memory - they're living processes, not static DAGs.

**The Key Distinction**:
Most tools give you one agent. Ikigai gives you an operating environment for building agent systems. Like Erlang/OTP gave you a platform for building concurrent systems, ikigai gives you a platform for building multi-agent systems that learn, coordinate, and run autonomously.

---

## The Deeper Idea

When you build with ikigai, you're not just building software. You're building an agent system that grows with you.

Traditional development is linear: you put in effort, you get output, repeat. Agent platform development is compounding: you build agents, those agents produce output, you refine their memory and capabilities, they fork children for specialized work, they accumulate knowledge, they coordinate autonomously. Your efforts multiply through the agent tree.

Over time, your agent system develops institutional knowledge through structured memory. Pinned blocks contain patterns and decisions. Auto-summary indexes what fell off the sliding window. Archival stores everything forever. Agents `/remember` important learnings, `/compact` knowledge as it accumulates, `/recall` old discussions when relevant. StoredAssets grow and evolve as agents learn.

Parent agents delegate to children through forking. Children coordinate through mailboxes. Some agents run in the terminal with you. Others run autonomously in iki-genba, processing queues or monitoring systems. The agent tree evolves to match your project's needs.

You stay in control. You design agents in the terminal. You curate what gets pinned as StoredAssets. You decide what runs autonomously. But the coordination, the delegation, the knowledge management, the continuous operation - that's handled by the agent system.

Ikigai amplifies expertise. An expert builds better agent systems than a novice. The platform extends what you already know how to do - it doesn't replace the need to know it. Your domain knowledge becomes the agent system's domain knowledge, compounding over time.

---

Ikigai is something I'm building because I think it can help people do more with less.

*The rest of the documentation covers the technical details: how agents work, how the platform is structured, how deployment works. But the core idea is what you've just read: amplification through agents, compounding over time.*
