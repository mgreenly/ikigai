# Task Order

## Shared Context DI Refactor

Prerequisite infrastructure for multi-agent support. Pure refactor - no behavioral changes.

- [shared-ctx-struct.md](shared-ctx-struct.md) - Create empty ik_shared_ctx_t structure
- [shared-ctx-cfg.md](shared-ctx-cfg.md) - Migrate cfg field, update repl_init signature
- [shared-ctx-term-render.md](shared-ctx-term-render.md) - Migrate term and render fields
- [shared-ctx-db-session.md](shared-ctx-db-session.md) - Migrate db_ctx and session_id fields
- [shared-ctx-history.md](shared-ctx-history.md) - Migrate history field
- [shared-ctx-debug.md](shared-ctx-debug.md) - Migrate debug infrastructure
- [shared-ctx-test-helpers.md](shared-ctx-test-helpers.md) - Create test fixture helpers
- [shared-ctx-cleanup.md](shared-ctx-cleanup.md) - Final verification and documentation

## Agent Context Extraction

Extract per-agent state into `ik_agent_ctx_t`. Pure refactor - still single agent, no multi-agent behavior yet.

- [agent-ctx-struct.md](agent-ctx-struct.md) - Create ik_agent_ctx_t with identity fields
- [agent-ctx-display.md](agent-ctx-display.md) - Migrate scrollback, layer_cake, layers, viewport
- [agent-ctx-input.md](agent-ctx-input.md) - Migrate input_buffer, visibility flags
- [agent-ctx-conversation.md](agent-ctx-conversation.md) - Migrate conversation, marks
- [agent-ctx-llm.md](agent-ctx-llm.md) - Migrate curl_multi, state, streaming buffers
- [agent-ctx-tool.md](agent-ctx-tool.md) - Migrate tool thread state
- [agent-ctx-spinner.md](agent-ctx-spinner.md) - Migrate spinner_state
- [agent-ctx-completion.md](agent-ctx-completion.md) - Migrate completion
- [agent-ctx-cleanup.md](agent-ctx-cleanup.md) - Final verification and documentation

## Multi-Agent Implementation

Implement multi-agent behavior: spawn, switch, kill, and concurrent execution.

### Foundation

- [repl-agent-array.md](repl-agent-array.md) - Convert single agent to agents[] array
- [db-agent-id-schema.md](db-agent-id-schema.md) - Add agent_id column to messages table
- [db-agent-id-queries.md](db-agent-id-queries.md) - Filter all DB operations by agent_id
- [agent-event-loop.md](agent-event-loop.md) - Per-agent event loop (agents always run)

### Navigation

- [input-ctrl-arrow-actions.md](input-ctrl-arrow-actions.md) - Add Ctrl+Arrow input actions
- [agent-switch.md](agent-switch.md) - Implement switching mechanics + lazy reflow

### User Interface

- [separator-agent-id.md](separator-agent-id.md) - Display agent ID in separator

### Commands

- [cmd-spawn.md](cmd-spawn.md) - /spawn command (create new agent)
- [cmd-kill.md](cmd-kill.md) - /kill command (terminate agent)
- [cmd-agents.md](cmd-agents.md) - /agents command (list all agents)

## Agent-Spawned Sub-Agents

LLM-spawned sub-agents for task delegation. Agents can spawn child agents via tool call.

### Foundation

- [subagent-hierarchy-fields.md](subagent-hierarchy-fields.md) - Add parent/children/is_sub_agent fields to agent context
- [subagent-depth-calc.md](subagent-depth-calc.md) - Implement agent depth calculation
- [subagent-child-array.md](subagent-child-array.md) - Implement child array management (add/remove)

### Tool Implementation

- [subagent-tool-schema.md](subagent-tool-schema.md) - Define spawn_sub_agent tool schema
- [subagent-tool-handler.md](subagent-tool-handler.md) - Implement tool handler (validation, creation, blocking)
- [subagent-completion-detect.md](subagent-completion-detect.md) - Implement completion detection (would-wait-for-human)
- [subagent-result-delivery.md](subagent-result-delivery.md) - Implement result extraction and delivery to parent

### Navigation

- [subagent-nav-up-down.md](subagent-nav-up-down.md) - Implement Ctrl+Up/Down for hierarchy traversal
- [subagent-separator-nav.md](subagent-separator-nav.md) - Update separator for full navigation context display

### Lifecycle

- [subagent-cascade-kill.md](subagent-cascade-kill.md) - Implement cascade kill (depth-first)
- [subagent-input-disable.md](subagent-input-disable.md) - Disable input for sub-agents (observe only)

### Display

- [subagent-agents-tree.md](subagent-agents-tree.md) - Update /agents command for tree hierarchy display

## Inter-Agent Mailboxes

Asynchronous message passing between agents via inbox system.

### Data Structures

- [mail-msg-struct.md](mail-msg-struct.md) - Create ik_mail_msg_t structure
- [mail-inbox-struct.md](mail-inbox-struct.md) - Create ik_inbox_t with operations
- [mail-agent-inbox.md](mail-agent-inbox.md) - Add inbox field to ik_agent_ctx_t

### Database Layer

- [mail-db-schema.md](mail-db-schema.md) - Create mail table migration
- [mail-db-operations.md](mail-db-operations.md) - Insert, query, mark read operations

### Core Operations

- [mail-send.md](mail-send.md) - Send operation (validate, DB write, inbox update)
- [mail-list.md](mail-list.md) - List inbox messages with sorting
- [mail-read.md](mail-read.md) - Read message and mark as read

### Slash Command

- [mail-cmd-register.md](mail-cmd-register.md) - Register /mail command with subcommand dispatch

### Tool Interface

- [mail-tool-schema.md](mail-tool-schema.md) - Build mail tool JSON schema
- [mail-tool-handler.md](mail-tool-handler.md) - Tool execution handler

### Separator Enhancement

- [mail-separator-unread.md](mail-separator-unread.md) - Display [mail:N] indicator

### Notification System

- [mail-notification-inject.md](mail-notification-inject.md) - Inject notification on IDLE with unread
- [mail-notification-style.md](mail-notification-style.md) - Dim styling for notifications
