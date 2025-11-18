# Command Reference

Complete reference for all ikigai slash commands.

## Command Syntax

All commands start with `/` and support tab completion. Arguments in square brackets `[arg]` are optional.

```bash
/command [arg1] [arg2]
```

## Agent Management

### /agent new [name] [--worktree] [--branch=<branch>]

Create a new helper agent.

**Arguments:**
- `name` - Agent name (default: random like "BouncyGonzo")
- `--worktree` - Create git worktree for this agent
- `--branch=<name>` - Branch name (default: same as agent name)

**Examples:**
```bash
/agent new                          # Random name, no worktree
/agent new research                 # Named agent, no worktree
/agent new oauth-impl --worktree    # Agent with worktree on branch oauth-impl
/agent new refactor --worktree --branch=feature/error-refactor
```

**Effect:**
- Creates new agent with isolated conversation
- Optionally creates git worktree in `.worktrees/<name>/`
- Optionally creates and checks out new branch
- Switches to new agent immediately
- Agent greeting message displayed

**Worktree Behavior:**
- Created in `.worktrees/<agent-name>/`
- New branch created from current HEAD
- Agent's working directory is worktree path
- All file operations relative to worktree

### /agent list

Show all active agents.

**Example Output:**
```
Active Agents:
• main (you are here) - Branch: main
  Worktree: ~/projects/ikigai/main

• oauth-impl (+mark: 2) - Branch: feature-oauth
  Worktree: ~/projects/ikigai/main/.worktrees/oauth-impl
  Last activity: 5 minutes ago

• research - No worktree
  Last activity: 15 minutes ago
```

**Indicators:**
- `(you are here)` - Current agent
- `+mark: N` - Has N active marks
- Branch and worktree info (when applicable)
- Last activity timestamp

### /agent <name>

Switch to named agent.

**Arguments:**
- `name` - Agent name (supports fuzzy matching)

**Examples:**
```bash
/agent main                  # Switch to root agent
/agent oauth                 # Fuzzy match to "oauth-impl"
/agent Bouncy                # Fuzzy match to "BouncyGonzo"
```

**Effect:**
- Switches conversation context
- Changes working directory (if agent has worktree)
- Updates status line
- No context pollution between agents

**Fuzzy Matching:**
- Case-insensitive substring match
- "oauth" matches "oauth-impl"
- "Bouncy" matches "BouncyGonzo"
- Ambiguous matches show options

### /agent main

Shortcut to switch to root agent.

**Example:**
```bash
/agent main
```

Equivalent to `/agent <root-agent-name>` but always works regardless of root agent's name.

### /agent rename <name>

Rename current agent.

**Arguments:**
- `name` - New agent name

**Examples:**
```bash
/agent rename dev              # Rename root to "dev"
/agent rename api-work         # Rename current helper
```

**Effect:**
- Changes agent name
- Updates status line
- If agent has worktree: renames worktree directory
- If agent has branch: renames branch

**Restrictions:**
- Name must be unique
- Cannot use reserved names (main, list, new, etc.)

### /agent close [--keep-branch] [--no-merge]

Close current helper agent.

**Flags:**
- `--keep-branch` - Don't delete branch after merge
- `--no-merge` - Don't merge branch (implies --keep-branch)

**Examples:**
```bash
/agent close                    # Close, merge to main, clean up
/agent close --keep-branch      # Close, merge, but keep branch
/agent close --no-merge         # Close without merging (discard work)
```

**Default Behavior:**
1. Prompt for merge confirmation (if agent has worktree)
2. Merge branch to main (if confirmed)
3. Delete worktree
4. Delete branch (unless --keep-branch)
5. Destroy agent and conversation
6. Memory documents persist
7. Switch to root agent

**Restrictions:**
- Cannot close root agent
- Merge conflicts halt process (manual resolution required)

### /agent merge [<target-branch>]

Merge current agent's branch to target.

**Arguments:**
- `target-branch` - Target branch name (default: main)

**Examples:**
```bash
/agent merge                    # Merge to main
/agent merge develop            # Merge to develop
```

**Effect:**
1. Switches to target branch
2. Merges current agent's branch
3. Switches back to agent's branch
4. Shows merge result

**Restrictions:**
- Agent must have a worktree/branch
- Merge conflicts require manual resolution
- On conflict: agent stays open for resolution

## Conversation Control

### /mark [label]

Create checkpoint at current position.

**Arguments:**
- `label` - Optional descriptive label

**Examples:**
```bash
/mark                           # Anonymous checkpoint
/mark before-refactor           # Named checkpoint
/mark approach-a                # Multiple named checkpoints
```

**Effect:**
- Creates checkpoint in session message array
- Visible as separator in scrollback
- Persisted to database
- Added to LIFO mark stack
- Can have multiple marks

**Use Cases:**
- Before risky refactors
- When exploring multiple approaches
- Before complex changes
- Nested exploration (marks stack)

### /rewind [label]

Rollback to checkpoint.

**Arguments:**
- `label` - Target label (default: most recent mark)

**Examples:**
```bash
/rewind                         # Rewind to last mark
/rewind before-refactor         # Rewind to specific label
```

**Effect:**
- Truncates session messages to mark position
- Removes marks at and after target
- Rebuilds scrollback from remaining messages
- Persists rewind operation to DB (audit trail)
- All messages preserved in database

**Non-Destructive:**
- Database retains all messages
- Can view rewound-from messages with `/history`
- Complete audit trail preserved

**Error Cases:**
- No marks: "No marks to rewind to"
- Label not found: "Mark 'label' not found"

### /clear

Clear current session context.

**Effect:**
- Clears scrollback display
- Clears session message array (LLM context)
- Clears all marks
- Database unchanged (messages preserved)
- Fresh conversation context

**Use Cases:**
- Start fresh conversation
- Clear accumulated context
- Reset after long session

**Note:** Other agents unaffected.

## Memory Documents

### /memory list [--filter=<pattern>]

List all memory documents.

**Flags:**
- `--filter=<pattern>` - Filter by alias/title pattern

**Examples:**
```bash
/memory list                    # All documents
/memory list --filter=openai/*  # Documents with alias openai/*
/memory list --filter=*auth*    # Documents with "auth" in alias
```

**Example Output:**
```
Memory Documents:
#mem-124 (openai/streaming) - OpenAI Streaming API Research
  Created: 2025-11-15 by agent 'research'
  Size: 2.4KB | References: 3

#mem-125 (patterns/error-handling) - Error Handling Patterns
  Created: 2025-11-15 by agent 'main'
  Size: 1.8KB | References: 7

Total: 2 documents
```

### /memory show <id|alias>

Display memory document content.

**Arguments:**
- `id` - Document ID (e.g., `124` or `mem-124`)
- `alias` - Document alias (e.g., `openai/streaming`)

**Examples:**
```bash
/memory show 124                    # By ID
/memory show mem-124                # By ID with prefix
/memory show openai/streaming       # By alias
```

**Effect:**
- Displays document content in scrollback
- Shows metadata (creator, date, size, references)
- Does NOT merge into conversation context

### /memory merge <id|alias>

Pull memory document into conversation context.

**Arguments:**
- `id|alias` - Document identifier

**Examples:**
```bash
/memory merge 124                   # Merge by ID
/memory merge openai/streaming      # Merge by alias
```

**Effect:**
- Adds document to session messages
- Becomes part of LLM context
- Persisted as message in database
- Visible in scrollback as merged content

**Use Case:** Make document permanently available in current conversation.

### /memory delete <id|alias>

Permanently delete memory document.

**Arguments:**
- `id|alias` - Document identifier

**Examples:**
```bash
/memory delete 124                  # Delete by ID
/memory delete openai/streaming     # Delete by alias
```

**Effect:**
- Permanently removes from database
- No undo available
- Prompts for confirmation

**Restrictions:**
- Cannot delete if currently merged in any active agent
- Shows warning if document has references

### /memory alias <id> <alias>

Create or update alias for memory document.

**Arguments:**
- `id` - Document ID
- `alias` - New alias (can use `/` for hierarchy)

**Examples:**
```bash
/memory alias 124 openai/streaming
/memory alias 125 patterns/errors
/memory alias 126 research/oauth
```

**Effect:**
- Creates or updates alias
- Enables organized namespacing
- Multiple docs can share prefix (e.g., `openai/*`)

**Alias Format:**
- Alphanumeric, hyphens, underscores, slashes
- Case-sensitive
- Max 64 characters

## Git Integration

### /branch [--all]

Show current branch and status.

**Flags:**
- `--all` - Show all branches across all worktrees

**Example Output:**
```bash
/branch

Current Branch: feature-oauth
Worktree: ~/projects/ikigai/main/.worktrees/oauth-impl

Status:
  Modified:   src/auth.c
  Modified:   src/auth.h
  Untracked:  tests/unit/auth_test.c

Ahead of main by 3 commits:
  abc1234 Implement OAuth token validation
  def5678 Add OAuth configuration
  ghi9012 Create auth module structure
```

**With --all:**
```bash
/branch --all

All Branches:
• main - ~/projects/ikigai/main
  Last commit: 5ae6b6e Implement OpenAI client module

• feature-oauth - ~/projects/ikigai/main/.worktrees/oauth-impl (agent: oauth-impl)
  Last commit: abc1234 Implement OAuth token validation
  Ahead of main by 3 commits

• refactor-errors - ~/projects/ikigai/main/.worktrees/refactor
  Last commit: jkl3456 Refactor error handling (agent: refactor)
  Ahead of main by 1 commit
```

### /commit [message]

Create git commit in current worktree.

**Arguments:**
- `message` - Commit message (if omitted, opens editor)

**Examples:**
```bash
/commit "Add OAuth authentication"
/commit                             # Opens editor
```

**Effect:**
- Runs `git commit` in current working directory
- Shows commit result
- Updates branch status

**Interactive Mode:**
- Without message: opens $EDITOR for commit message
- Shows staged changes before commit
- Allows review before committing

### /diff [file]

Show git diff in current worktree.

**Arguments:**
- `file` - Optional file path (default: all changes)

**Examples:**
```bash
/diff                               # All changes
/diff src/auth.c                    # Specific file
```

**Effect:**
- Displays git diff in scrollback
- Syntax highlighted (future)
- Shows both staged and unstaged changes

## Session Management

### /history [--limit=N]

Show conversation history from database.

**Flags:**
- `--limit=N` - Limit to N recent messages (default: 50)

**Examples:**
```bash
/history                            # Recent 50 messages
/history --limit=100                # Recent 100 messages
```

**Effect:**
- Queries database for session history
- Shows all messages including rewound-from
- Displays mark/rewind operations
- Does NOT add to current context

**Use Case:** Audit trail, review rewound messages

### /sessions

List recent conversation sessions.

**Example Output:**
```
Recent Sessions:
• [Active] Session 47 - Started 2 hours ago
  Messages: 23 | Marks: 2 | Last: "Implement OAuth"

• Session 46 - 1 day ago
  Messages: 15 | Marks: 0 | Last: "Fix memory leak in parser"

• Session 45 - 2 days ago
  Messages: 42 | Marks: 5 | Last: "Complete REPL implementation"
```

### /load <session-id>

Load previous session into current context.

**Arguments:**
- `session-id` - Session ID from `/sessions`

**Examples:**
```bash
/load 46                            # Load session 46
```

**Effect:**
- Loads all messages from session
- Rebuilds scrollback
- Restores marks
- Continues in same database session

## System Commands

### /help [command]

Show help information.

**Arguments:**
- `command` - Optional specific command

**Examples:**
```bash
/help                               # All commands
/help agent                         # Help for /agent
/help mark                          # Help for /mark
```

**Effect:**
- Displays command reference
- Auto-generated from command registry
- Shows usage examples

### /quit

Exit ikigai.

**Effect:**
- Closes all agents
- Commits pending database transactions
- Saves state
- Exits cleanly

**Confirmation:**
- Prompts if uncommitted work in worktrees
- Shows what will be lost
- Requires explicit confirmation

## Future Commands (v2.0+)

### /model <name>

Switch LLM model.

**Examples:**
```bash
/model gpt-4                        # Switch to GPT-4
/model claude-sonnet                # Switch to Claude
/model gemini-pro                   # Switch to Gemini
```

### /search <query>

Full-text search across all conversations.

**Examples:**
```bash
/search OAuth implementation
/search error handling patterns
```

### /export <format>

Export conversation to file.

**Examples:**
```bash
/export markdown                    # Export as markdown
/export json                        # Export as JSON
```

## Command Design Principles

### 1. Explicit Actions

Commands represent explicit user intent:
- No hidden side effects
- No automatic branching
- No magic behaviors

### 2. Composable

Commands work together:
- `/agent new research --worktree` + `/mark` + `/rewind`
- Create agent, checkpoint, explore, rollback
- Each command does one thing well

### 3. Reversible

Most operations are non-destructive:
- `/rewind` doesn't delete messages
- `/agent close --no-merge` preserves branch
- Database keeps audit trail

### 4. Discoverable

Help is built-in:
- `/help` shows all commands
- `/help <command>` shows details
- Tab completion for command names
- Tab completion for arguments

### 5. Git-Aware

Commands understand git context:
- `/agent new --worktree` creates branch
- `/commit` works in current worktree
- `/branch` shows multi-worktree status
- Agents track their branches

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

**Benefits:**
- Auto-generated `/help`
- Easy to add new commands
- Consistent error handling
- Tab completion support
