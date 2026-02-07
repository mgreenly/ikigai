# What Makes Ikigai Different

Four architectural choices that set ikigai apart from other coding agents.

## 1. Explicit Context Management

Most agents hide context management. You don't know what's being sent to the LLM or how to control it. Eventually you hit token limits and lose your conversation.

Ikigai gives you control:
- `/clear` - Start fresh
- `/mark` and `/rewind` - Checkpoint and rollback
- `/forget <criteria>` - Remove specific topics while keeping the rest
- `/remember <criteria>` - Keep only specific topics, forget everything else

The LLM helps you filter. You ask to forget "database schema discussions" and it shows you what matches before applying. The planning conversation itself gets cleaned up after.

Context never changes implicitly. You always know what's in the conversation and why.

## 2. Minimal Tools + External Extension

Most agents have dozens of internal tools. Every file operation, git command, and search pattern is a custom tool. Adding your own requires understanding their plugin system.

Ikigai keeps internal tools minimal — only agent operations (fork, kill, send, wait) that require in-process access to shared state. Everything else is external tools:
- 6 core external tools: `bash`, `file_read`, `file_write`, `file_edit`, `glob`, `grep`
- Tools auto-discovered from directories (system, user, project)
- JSON protocol for tool communication (stdin/stdout)
- Self-describing via `--schema` flag

**User extensibility** is trivial. Drop an executable in `~/.ikigai/tools/` or `.ikigai/tools/`, implement the JSON protocol, done. No registration, no restart, no special integration required.

**Skills** are markdown files that teach domain knowledge and mention relevant tools. Load the database skill, get schema docs plus references to database query tools.

This makes user extension trivial. Everything in ikigai is built around external tools being the norm, not the exception.

## 3. Agent Process Model

Most agents are single-threaded or have opaque parallelism. You can't fork a conversation to explore two approaches simultaneously. You can't send work to an agent and keep working while it completes.

Ikigai treats agents as processes:
- `/fork` creates a child agent with inherited history
- `/fork "task"` delegates work to the child
- Fork without a prompt creates a divergence point - try two approaches
- Both human-spawned and LLM-spawned agents
- Message passing via `send` and `wait`
- Lifecycle control via `kill` (always cascades to descendants)
- Registry tracks all agents across restarts

Parent continues working while child handles its task. Agents coordinate through messages or shared StoredAssets. The database is the source of truth - agents survive restarts.

Inspired by Unix (fork, signals, process table) and Erlang (lightweight processes, mailboxes).

## 4. Iki-genba: Runtime for Autonomous Agents

Most agents require a human at the terminal. Close the window, the agent dies.

**Iki-genba** is a PaaS where code-driven agents run continuously:
- Respond to webhooks
- Process queues
- Watch logs
- Run on schedules
- Coordinate long-running work

Same database, same messaging, same identity model as terminal agents. Internal (human-driven) and external (code-driven) agents coordinate through shared infrastructure.

Built on boring tech:
- Deno for TypeScript runtime
- runit for process supervision
- PostgreSQL for coordination
- nginx for HTTP routing

Optional. If you only want terminal work, you don't need it. But if you want agents that run independently, iki-genba gives them a place to live.

---

## Why These Choices

**Context management** - Long conversations are powerful but need management. Explicit control beats black-box limits.

**Minimal tools** - External tools for user-extensible capabilities. Internal tools only for agent operations requiring in-process shared state access.

**Process model** - Parallelism and coordination enable complex workflows. Agents shouldn't be single-threaded in 2025.

**Iki-genba** - Autonomous agents need runtime infrastructure. PaaS approach makes deployment straightforward.

These aren't promises of future features. They're design decisions that shape what ikigai is today.
