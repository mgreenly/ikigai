# Ikigai Elevator Pitch

Ikigai is an agent platform - an operating environment for living agents.

Build agent systems interactively through the terminal (ikigai), deploy them to run autonomously (iki-genba), and coordinate them through process primitives: agents fork like Unix processes, communicate via Erlang-style mailboxes, and learn continuously through structured memory. Memory blocks evolve through `/remember` and `/compact`, the sliding window auto-evicts old exchanges, and everything persists in PostgreSQL archival.

The same tools work on filesystem and database (`ikigai://` URIs), agents spawn children to delegate work (`/fork`), send messages between processes (`/send`), and share knowledge through pinned memory blocks. Human-driven agents in the terminal coordinate seamlessly with autonomous agents running in iki-genba.

Most tools give you one agent. Ikigai gives you a platform to build agent systems - process trees that fork, coordinate, learn, and run autonomously.

Like Erlang/OTP for AI agents: the process model, memory system, and runtime where living agents operate.
