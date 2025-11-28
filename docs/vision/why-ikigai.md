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

**No Exploration Safety:**
- Try risky approach → doesn't work → lose context
- No checkpoint/rollback
- Wasted time, wasted API calls

**Slow Workflows:**
- Sequential feature development
- Manual git management
- Mental overhead tracking state

## The ikigai Solution

### 1. Multi-Agent Parallelization

**Run multiple AI agents in parallel, each with isolated context.**

```bash
# Create three agents for three features
/agent new oauth --worktree
/agent new rate-limit --worktree
/agent new audit --worktree

# Switch instantly with Ctrl-\ <name>
# Each works independently with own conversation, worktree, and branch

# Merge when ready
/agent merge && /agent close
```

**Use Cases:** Parallel features, background research, experimental branches, long-running refactors

### 2. Git-Native Workflows

**ikigai understands git. Agents = Branches = Worktrees.**

```bash
/agent new feature-x --worktree
# Automatically: creates branch, creates worktree, sets working dir

# Work in complete isolation - no branch switching
/agent merge   # Merge to main
/agent close   # Delete worktree, delete branch
```

**Benefits:** Physical isolation, no branch switching, no merge conflicts during development, automatic cleanup

### 3. Mark/Rewind Exploration

**Checkpoint and rollback for safe exploration.**

```bash
/mark before-refactor
[Try approach A...]
/rewind before-refactor  # Instant rollback
[Try approach B from same starting point]
```

**Use Cases:** Try multiple approaches, explore risky refactors, compare alternatives, document exploration

### 4. Shared Knowledge Layer

**Memory documents persist across agents and sessions.**

```bash
[Agent: research]
# Research creates knowledge
Created #mem-oauth/patterns

[Agent: impl]
# Implementation uses knowledge
[You] Implement OAuth using #mem-oauth/patterns
```

## Killer Features

### Parallel Feature Development
Develop 3+ features simultaneously without branch switching. Time saved: 60-70%.

### Experimental Branches
Try multiple approaches, benchmark objectively, keep the best, discard losers without wasted commits.

### Background Research
Research in one agent while continuing to code in another. Check findings later, implement when ready.

### Long-Running Refactors
Major refactor in isolation while main branch stays stable and continues development.

### Safe Exploration
Try anything, rewind if it doesn't work. Fearless experimentation.

## Power User Workflows

### The "3 Features in 2 Hours" Workflow
Traditional: ~6 hours sequential. ikigai: Create 3 agents with worktrees, all work in parallel, merge each when complete. Result: 2 hours, 3 features done.

### The "Exploration Then Implementation" Workflow
Research agent creates memory docs for multiple topics, then separate implementation agents use those docs for clean implementation.

### The "Try Everything, Keep the Best" Workflow
Mark baseline, create agents for approaches A/B/C, compare objectively, keep winner, discard losers.

## Comparison Matrix

| Feature | Other Agents | ikigai |
|---------|--------------|--------|
| Multiple agents | ✗ | ✓ |
| Git worktree integration | ✗ | ✓ |
| Parallel features | ✗ | ✓ |
| Mark/rewind | ✗ | ✓ |
| Terminal-native | Limited | ✓ |
| Memory documents | ✗ | ✓ |
| Local-first | Varies | ✓ |

## Design Philosophy

**Terminal-Native:** Fast startup (~100ms), low memory (<50MB), works over SSH, maximum responsiveness

**Power User Focus:** Keyboard-driven, fuzzy matching, vim-inspired bindings, tmux-inspired multiplexing

**Git-Native:** Status line shows branch, agents map to branches, worktrees for isolation, git-aware everywhere

**Local-First:** PostgreSQL storage, direct LLM API calls, complete privacy, full control

## Target Users

**Senior/Staff Engineers:** 5+ years experience, comfortable with terminal, heavy git users, work on multiple features simultaneously

**Open Source Maintainers:** Manage complex projects, multiple feature branches, frequent context switching, value git hygiene

**Platform/Infrastructure Engineers:** Build tools and frameworks, deep exploration, experiment with approaches, need comprehensive documentation

**Technical Founders/CTOs:** Wear multiple hats, work on multiple features, need to move fast, can't afford context switching overhead

## Getting Started

```bash
# Install
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

First session example:
```bash
/agent new research      # Research agent
/agent new impl --worktree  # Implementation agent with worktree
# You're now parallelized!
```

## The Future

**v1.0 (2025 Q2):** Single agent, mark/rewind, OpenAI integration, database persistence

**v2.0 (2025 Q3):** Multi-agent support, git worktree integration, memory document store

**v2.1 (2025 Q4):** Multi-LLM support (Anthropic, Google), visual branch tree, GitHub/GitLab integration

**v3.0 (2026):** Distributed agents, team collaboration, advanced AI orchestration, plugin system

### Vision

**The terminal-native AI coding agent for power users.**

Current AI coding agents are built for beginners. ikigai is built for experts who deserve tools that:
- Match their mental model
- Enhance their workflows
- Stay out of the way
- Maximize productivity

## Try It

```bash
git clone https://github.com/mgreenly/ikigai.git
cd ikigai
make
./build/bin/ikigai
```

## Join Us

- GitHub: https://github.com/mgreenly/ikigai
- Issues: https://github.com/mgreenly/ikigai/issues
- Discussions: https://github.com/mgreenly/ikigai/discussions

**Built by power users, for power users.**

---

*ikigai (生き甲斐) - "reason for being"*

*Your reason for coding efficiently.*
