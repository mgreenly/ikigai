# Roadmap to MVP

Releases rel-01 through rel-12 are complete. See [CHANGELOG.md](../CHANGELOG.md) for details.

### rel-13: Dynamic Sliding Context Window (future)

**Objective**: Proactive context management with a fixed history budget and automatic turn clipping

**Design**: [sliding-context-window.md](sliding-context-window.md)

**Features**:
- Token counting module with pluggable backends (character estimator on day one, exact tokenizers later)
- Turn grouping — user prompt + tool calls/results + assistant response as an atomic unit
- Per-turn token tracking via model-independent estimation
- Configurable history cap (`DEFAULT_HISTORY_TOKEN_LIMIT`, compile-time default 100k)
- Automatic clipping of oldest turns when history exceeds cap
- Context divider in chat UI showing what the model can see vs what's fallen out
- All messages retained in memory and database; clipping only affects API requests


### rel-14: Context Summary (future)

**Objective**: Compressed summary of clipped conversation history to preserve long-term context

**Features**:
- Summary budget (configurable cap, independent of history cap)
- Clipped turns feed into summary generation
- Summary included in API requests alongside live history


### rel-15: Parallel Tool Execution (future)

**Objective**: Parallelize read only tool calls

**Features**:
- Parallelize read only tool calls


### rel-16: Per-Agent Configuration (future)

**Objective**: Implement a runtime per-agent configuration system

**Features**:
- Default configuration defined purely in code (no config files)
- `/config get|set KEY=value` slash commands for modifying the current agent's config
- Deep copy inheritance on fork (children inherit parent's config at fork time)
- Support for named config templates managed via `/config template` commands
- Extended fork capabilities (`--from`, `--config`) to use templates or other agents as config sources
- Per-agent workspace at `ik://agent/<UUID>/projects/` for repo checkouts and isolated work directories


### rel-17: Additional Deployment Targets (future)

**Objective**: Extend build and packaging support to macOS and Ubuntu

**Features**:
- macOS support (Homebrew packaging, platform-specific adaptations)
- Ubuntu support (apt packaging, systemd integration)
- CI matrix covering Linux, macOS, and Ubuntu


### rel-18: User Experience (future)

**Objective**: Polish configuration, discoverability, and customization workflows

**Features**:
- lots of tab auto completion
- improved status bar
- type while agent is thinking (buffer input, flush on response completion)
- paste like Claude Code (bracketed paste with preview before submit)
- auto-complete model names in `/model` command
- auto-complete agent UUIDs in `/switch`, `/kill`, `/send`, and similar commands


### rel-19: Codebase Refactor & MVP Release (future)

**Objective**: Improve code organization, reduce complexity, and clean up technical debt before MVP release.
