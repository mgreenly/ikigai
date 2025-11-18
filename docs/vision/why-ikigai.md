# Why ikigai?

**The power user's AI coding agent.** Git-native, terminal-first, massively parallel.

## The Problem

Current AI coding agents treat complex workflows as an afterthought:

**Single Conversation:**
- One AI agent, one task at a time
- Want to research while coding? Open a new window
- Want to try multiple approaches? Start over from scratch
- Context gets polluted, conversations get messy

**Git Ignorance:**
- Agents don't understand branches
- Manual branch switching required
- Can't work on multiple features in parallel
- No awareness of git context

**No Exploration Safety:**
- Try risky approach → doesn't work → lose context
- No checkpoint/rollback
- Start new conversation, re-establish context
- Wasted time, wasted API calls

**Slow Workflows:**
- Sequential feature development
- Manual git management
- Context switching overhead
- Mental overhead tracking state

## The ikigai Solution

### 1. Multi-Agent Parallelization

**Run multiple AI agents in parallel, each with isolated context.**

```bash
# Create three agents for three features
/agent new oauth --worktree       # Works on OAuth
/agent new rate-limit --worktree  # Works on rate limiting
/agent new audit --worktree       # Works on audit logging

# Switch between them instantly
Ctrl-\ oauth        # Check OAuth progress
Ctrl-\ rate         # Check rate limiting
Ctrl-\ audit        # Check audit logging
Ctrl-\ m            # Back to main

# Each agent works independently:
- Own conversation history
- Own git worktree
- Own branch
- No context pollution

# Merge when ready
/agent oauth
/agent merge
/agent close

# Result: 3x parallelization, 10x less overhead
```

**Use Cases:**
- Parallel feature development (3+ features simultaneously)
- Background research while coding
- Experimental branches (try multiple approaches)
- Long-running refactors in isolation

### 2. Git-Native Workflows

**ikigai understands git. Agents = Branches = Worktrees.**

```bash
# Agent automatically creates worktree and branch
/agent new feature-x --worktree

What happens:
✓ Creates branch feature-x from HEAD
✓ Creates worktree in .worktrees/feature-x/
✓ Creates agent with working directory in worktree
✓ Switches to agent

# Agent works in complete isolation
[Agent: feature-x] [Branch: feature-x] [Worktree: .worktrees/feature-x]

# No branch switching conflicts
# No "what branch am I on?" confusion
# No manual worktree management

# Merge and clean up automatically
/agent merge   # Merge to main
/agent close   # Delete worktree, delete branch

# Result: Git workflows at the speed of thought
```

**Benefits:**
- Physical isolation (separate directories)
- No branch switching
- No merge conflicts during development
- Automatic cleanup
- Git-aware status line

**Comparison:**

| Task | Other Agents | ikigai |
|------|--------------|--------|
| Create feature branch | Manual | `/agent new feature --worktree` |
| Switch branches | `git checkout` | `Ctrl-\ feature` |
| Work on feature | Manual context | Agent maintains context |
| Merge to main | Manual git | `/agent merge` |
| Clean up | Manual | `/agent close` (auto) |

### 3. Mark/Rewind Exploration

**Checkpoint and rollback for safe exploration.**

```bash
# Before risky refactor
/mark before-refactor

# Try approach A
[Deep conversation exploring approach A...]

# Doesn't work? Instant rollback.
/rewind before-refactor

# Try approach B from same starting point
[Clean context, no pollution from approach A]

# Result: Fearless exploration
```

**Use Cases:**
- Try multiple implementation approaches
- Explore risky refactors safely
- Compare alternatives objectively
- Document exploration paths

**Comparison:**

| Scenario | Other Agents | ikigai |
|----------|--------------|--------|
| Try risky approach | New conversation (lose context) | `/mark` → try → `/rewind` |
| Compare alternatives | Multiple conversations | `mark` → try A → `rewind` → try B |
| Dead end recovery | Start over | `/rewind` (instant) |
| Audit trail | Manual notes | Database preserves everything |

### 4. Shared Knowledge Layer

**Memory documents persist across agents and sessions.**

```bash
# Research agent creates knowledge
[Agent: research]
[You]
Research OAuth best practices

[Assistant]
[Researches comprehensively]
Created #mem-oauth/patterns

/agent close

# Implementation agent uses knowledge
[Agent: impl]
[You]
Implement OAuth using #mem-oauth/patterns

[Assistant]
[Loads memory doc]
Based on the research...

# Result: Research once, use everywhere
```

**Benefits:**
- Knowledge survives agent destruction
- Searchable, referenceable
- Organized with aliases (e.g., `oauth/*`)
- Cross-agent collaboration

## Killer Features

### 1. Parallel Feature Development

**Develop 3+ features simultaneously without branch switching.**

```bash
/agent new feature-a --worktree
/agent new feature-b --worktree
/agent new feature-c --worktree

# All work in parallel
# Each in own worktree
# Merge when ready

# Time saved: 60-70%
```

### 2. Experimental Branches

**Try multiple approaches, keep the best.**

```bash
/agent new experiment-a --worktree
[Try approach A, benchmark]

/agent new experiment-b --worktree
[Try approach B, benchmark]

# Compare objectively
# Keep winner, discard loser
# No wasted commits
```

### 3. Background Research

**Research in one agent, implement in another.**

```bash
/agent new research
[You]
Research WebSocket libraries and create memory doc

# Switch back to main, continue coding
Ctrl-\ m
[Continue working]

# Check research later
Ctrl-\ research
[Assistant]
Research complete! #mem-websocket/research

# Implement using findings
/agent new websocket-impl --worktree
[You]
Implement using #mem-websocket/research
```

### 4. Long-Running Refactors

**Major refactor in isolation while main stays stable.**

```bash
/agent new refactor-errors --worktree
[Refactor runs for hours/days in separate worktree]

# Meanwhile, main agent continues normal work
Ctrl-\ m
[Work on features, bug fixes]

# When refactor done
Ctrl-\ refactor
/agent merge
/agent close

# Result: No disruption to main development
```

### 5. Safe Exploration

**Try anything, rewind if it doesn't work.**

```bash
/mark safe-point
[Try risky changes]
# Doesn't work
/rewind safe-point
[Try different approach]

# Result: Fearless experimentation
```

## Power User Workflows

### The "3 Features in 2 Hours" Workflow

```bash
# Traditional approach: ~6 hours sequential

# ikigai approach:
/agent new auth --worktree
[Specify auth work]

/agent new api --worktree
[Specify API work]

/agent new logging --worktree
[Specify logging work]

# All agents work in parallel
# Each in own worktree
# Switch between to check progress
# Merge each when complete

# Result: 2 hours, 3 features done
```

### The "Exploration Then Implementation" Workflow

```bash
# Research without worktree
/agent new research
[You]
Research OAuth, WebSocket, and caching strategies
Create memory docs for each

# Implement from research
/agent new oauth --worktree
[You]
Implement using #mem-oauth/patterns

/agent new websocket --worktree
[You]
Implement using #mem-websocket/patterns

/agent new cache --worktree
[You]
Implement using #mem-cache/patterns

# Result: Clean separation of research and implementation
```

### The "Try Everything, Keep the Best" Workflow

```bash
/mark baseline

/agent new approach-a --worktree
[Implement and benchmark approach A]

/agent new approach-b --worktree
[Implement and benchmark approach B]

/agent new approach-c --worktree
[Implement and benchmark approach C]

# Compare all three
# Keep best, discard others
/agent approach-b  # Winner
/agent merge

/agent approach-a
/agent close --no-merge

/agent approach-c
/agent close --no-merge

# Result: Objective comparison, best approach chosen
```

## Comparison Matrix

### vs Cursor

| Feature | Cursor | ikigai |
|---------|--------|--------|
| Multiple agents | ✗ | ✓ |
| Git worktree integration | ✗ | ✓ |
| Parallel features | ✗ | ✓ |
| Mark/rewind | ✗ | ✓ |
| Terminal-native | ✗ | ✓ |
| Memory documents | ✗ | ✓ |

### vs GitHub Copilot

| Feature | Copilot | ikigai |
|---------|---------|--------|
| Multiple agents | ✗ | ✓ |
| Git worktree integration | ✗ | ✓ |
| Conversation context | Limited | Full |
| Mark/rewind | ✗ | ✓ |
| Parallel development | ✗ | ✓ |

### vs Aider

| Feature | Aider | ikigai |
|---------|-------|--------|
| Multiple agents | ✗ | ✓ |
| Git awareness | Basic | Native |
| Worktree support | ✗ | ✓ |
| Mark/rewind | ✗ | ✓ |
| Parallel features | ✗ | ✓ |
| Memory documents | ✗ | ✓ |

### vs Claude Code / Claude Desktop

| Feature | Claude Code | ikigai |
|---------|-------------|--------|
| Multiple agents | ✗ | ✓ |
| Git worktrees | ✗ | ✓ |
| Mark/rewind | ✗ | ✓ |
| Parallel work | ✗ | ✓ |
| Local-first | ✗ | ✓ |
| Terminal UI | Basic | Native |

## Design Philosophy

### Terminal-First

**No Electron. No web UI. Pure terminal.**

Benefits:
- Fast startup (~100ms)
- Low memory (< 50MB)
- Works over SSH
- No GUI dependencies
- Power user efficiency

### Power User Focus

**Keyboard-driven, no hand-holding.**

- Fuzzy matching for speed
- Vim-inspired key bindings
- Tmux-inspired multiplexing
- No unnecessary confirmations
- Maximum control

### Git-Native

**Git is a first-class citizen.**

- Status line shows branch
- Agents map to branches
- Worktrees for isolation
- Auto-merge workflows
- Git-aware everywhere

### Local-First

**Your machine, your data, your control.**

- All data in local PostgreSQL
- Direct API calls to LLMs
- No telemetry
- No external servers (except LLM APIs)
- Full trust model

## Target Users

### Senior/Staff Engineers

**Who:**
- 5+ years experience
- Comfortable with terminal
- Heavy git users
- Work on multiple features simultaneously

**Why ikigai:**
- Matches mental model (branches = features)
- Eliminates git overhead
- Enables parallel workflows
- Terminal-native speed

### Open Source Maintainers

**Who:**
- Manage complex projects
- Multiple feature branches
- Need to context switch frequently
- Value git hygiene

**Why ikigai:**
- Multiple agents for multiple PRs
- Each PR in own worktree
- Easy context switching
- Clean branch management

### Platform/Infrastructure Engineers

**Who:**
- Build tools and frameworks
- Deep technical exploration
- Experiment with approaches
- Need comprehensive documentation

**Why ikigai:**
- Mark/rewind for safe exploration
- Memory docs for patterns
- Parallel experimental branches
- Research agents for investigation

### Technical Founders/CTOs

**Who:**
- Wear multiple hats
- Work on multiple features
- Need to move fast
- Can't afford context switching overhead

**Why ikigai:**
- Massive parallelization
- Minimal mental overhead
- Fast workflows
- Maximum productivity

## Getting Started

### Installation

```bash
# Install ikigai
git clone https://github.com/mgreenly/ikigai.git
cd ikigai
make install

# Configure
cat > ~/.ikigai/config.json <<EOF
{
  "llm": {
    "default_provider": "openai",
    "openai_api_key": "sk-..."
  },
  "database": {
    "connection_string": "postgresql://localhost/ikigai"
  }
}
EOF

# Run
ikigai
```

### First Session

```bash
# Start ikigai
$ ikigai

[Agent: main] [Branch: main]

[You]
Help me implement OAuth authentication

[Assistant]
I'll help you implement OAuth. Let me start by...

# Create research agent
/agent new oauth-research

[You]
Research OAuth 2.0 best practices and create memory doc

# Create implementation agent
Ctrl-\ m
/agent new oauth-impl --worktree

[You]
Implement OAuth using patterns from oauth-research agent when ready

# You're now parallelized!
```

### Learning Resources

- [Getting Started Guide](../getting-started.md)
- [Command Reference](commands.md)
- [Workflow Examples](workflows.md)
- [Key Bindings](keybindings.md)

## The Future

### Roadmap

**v1.0 (2025 Q2):**
- Single agent
- Mark/rewind
- OpenAI integration
- Database persistence

**v2.0 (2025 Q3):**
- Multi-agent support
- Git worktree integration
- Memory document store
- Advanced navigation

**v2.1 (2025 Q4):**
- Multi-LLM support (Anthropic, Google)
- Visual branch tree
- GitHub/GitLab integration
- Enhanced search

**v3.0 (2026):**
- Distributed agents (remote execution)
- Team collaboration features
- Advanced AI orchestration
- Plugin system

### Vision

**The terminal-native AI coding agent for power users.**

- Massively parallel workflows
- Git-native by design
- Terminal speed and efficiency
- Local-first, user-controlled
- Open source, extensible

**Why it matters:**

Current AI coding agents are built for beginners. ikigai is built for experts.

We believe expert developers deserve expert tools:
- Tools that match their mental model
- Tools that enhance their workflows
- Tools that don't get in the way
- Tools that maximize productivity

ikigai is that tool.

## Try It

```bash
git clone https://github.com/mgreenly/ikigai.git
cd ikigai
make
./build/bin/ikigai

# Welcome to the future of AI-assisted coding
```

## Join Us

- GitHub: https://github.com/mgreenly/ikigai
- Issues: https://github.com/mgreenly/ikigai/issues
- Discussions: https://github.com/mgreenly/ikigai/discussions
- Discord: [Coming soon]

**Built by power users, for power users.**

---

*ikigai (生き甲斐) - "reason for being"*

*Your reason for coding efficiently.*
