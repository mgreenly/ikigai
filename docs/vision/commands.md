# Command Reference

Complete reference for all ikigai slash commands.

## Command Syntax

All commands start with `/` and support tab completion. Arguments in square brackets `[arg]` are optional.

```bash
/command [arg1] [arg2]
```

## Agent Management

### /agent new [name] [--worktree] [--branch=<branch>]

Create new helper agent.

**Arguments:**
- `name` - Agent name (default: random like "BouncyGonzo")
- `--worktree` - Create git worktree for this agent
- `--branch=<name>` - Branch name (default: same as agent name)

**Examples:**
```bash
/agent new                          # Random name, no worktree
/agent new oauth-impl --worktree    # Agent with worktree
/agent new refactor --worktree --branch=feature/error-refactor
```

**Effect:** Creates agent with isolated conversation, optional worktree in `.worktrees/<name>/`, switches to agent

### /agent list

Show all active agents with status, branch info, and activity.

### /agent <name>

Switch to named agent. Supports fuzzy matching (e.g., "oauth" matches "oauth-impl").

### /agent main

Shortcut to switch to root agent.

### /agent rename <name>

Rename current agent. Updates worktree directory and branch if applicable.

### /agent close [--keep-branch] [--no-merge]

Close current helper agent.

**Flags:**
- `--keep-branch` - Don't delete branch after merge
- `--no-merge` - Don't merge branch (implies --keep-branch)

**Default Behavior:** Prompts for merge, merges to main, deletes worktree, deletes branch, preserves memory docs, switches to root agent.

**Restrictions:** Cannot close root agent.

### /agent merge [<target-branch>]

Merge current agent's branch to target (default: main). Requires agent to have worktree/branch.

## Conversation Control

### /mark [label]

Create checkpoint at current position.

**Examples:**
```bash
/mark                           # Anonymous checkpoint
/mark before-refactor           # Named checkpoint
```

**Effect:** Creates checkpoint in session, visible as separator in scrollback, persisted to database, added to LIFO mark stack.

**Use Cases:** Before risky refactors, exploring multiple approaches, nested exploration.

### /rewind [label]

Rollback to checkpoint (default: most recent mark).

**Effect:** Truncates session messages to mark position, removes marks at/after target, rebuilds scrollback, persists rewind to DB.

**Non-Destructive:** Database retains all messages for audit trail.

### /clear

Clear current session context (scrollback, messages, marks). Database unchanged. Other agents unaffected.

## Memory Documents

### /memory list [--filter=<pattern>]

List all memory documents. Filter by alias/title pattern.

**Example Output:**
```
#mem-124 (openai/streaming) - OpenAI Streaming API Research
  Created: 2025-11-15 by agent 'research'
  Size: 2.4KB | References: 3
```

### /memory show <id|alias>

Display memory document content (doesn't merge into conversation context).

### /memory merge <id|alias>

Pull memory document into conversation context. Becomes part of LLM context.

### /memory delete <id|alias>

Permanently delete memory document (prompts for confirmation).

### /memory alias <id> <alias>

Create or update alias for memory document. Supports hierarchical organization (e.g., `openai/streaming`, `patterns/errors`).

## Git Integration

### /branch [--all]

Show current branch and status.

**With --all:** Shows all branches across all worktrees.

**Example Output:**
```bash
Current Branch: feature-oauth
Status:
  Modified:   src/auth.c
  Untracked:  tests/unit/auth_test.c
Ahead of main by 3 commits
```

### /commit [message]

Create git commit in current worktree. Without message, opens editor.

### /diff [file]

Show git diff in current worktree (all changes or specific file).

## Session Management

### /history [--limit=N]

Show conversation history from database (default: 50 recent messages). Includes rewound-from messages.

### /sessions

List recent conversation sessions with message counts, marks, and last activity.

### /load <session-id>

Load previous session into current context. Rebuilds scrollback and restores marks.

## System Commands

### /help [command]

Show help information for all commands or specific command.

### /quit

Exit ikigai. Prompts if uncommitted work in worktrees.

## Future Commands (v2.0+)

### /model <name>

Switch LLM model (e.g., `/model gpt-4`, `/model claude-sonnet`).

### /search <query>

Full-text search across all conversations.

### /export <format>

Export conversation to file (markdown, JSON).

## Command Design Principles

### 1. Explicit Actions
Commands represent explicit user intent with no hidden side effects.

### 2. Composable
Commands work together (e.g., `/agent new` + `/mark` + `/rewind`). Each does one thing well.

### 3. Reversible
Most operations are non-destructive. `/rewind` doesn't delete messages, database keeps audit trail.

### 4. Discoverable
`/help` shows all commands, `/help <command>` shows details, tab completion for names and arguments.

### 5. Git-Aware
Commands understand git context. `/agent new --worktree` creates branch, `/commit` works in current worktree.

## Command Implementation

All commands implemented in `src/commands.c` using registry pattern:

```c
static ik_command_t commands[] = {
    {
        .name = "agent",
        .description = "Manage multiple agents",
        .handler = cmd_agent,
        .subcommands = agent_subcommands,
    },
    {
        .name = "mark",
        .description = "Create checkpoint",
        .handler = cmd_mark,
    },
    // ... more commands
};
```

**Benefits:** Auto-generated `/help`, easy to add new commands, consistent error handling, tab completion support.
