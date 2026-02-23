# Roadmap to MVP

Releases rel-01 through rel-12 are complete. See [CHANGELOG.md](../CHANGELOG.md) for details.

### rel-13: Dynamic Sliding Context Window (future)

**Objective**: Manage context window efficiently with automatic history management

**Features**:
- History clipping
- Recent summary


### rel-14: Parallel Tool Execution (future)

**Objective**: Parallelize read only tool calls

**Features**:
- Parallelize read only tool calls


### rel-15: Per-Agent Configuration (future)

**Objective**: Implement a runtime per-agent configuration system

**Features**:
- Default configuration defined purely in code (no config files)
- `/config get|set KEY=value` slash commands for modifying the current agent's config
- Deep copy inheritance on fork (children inherit parent's config at fork time)
- Support for named config templates managed via `/config template` commands
- Extended fork capabilities (`--from`, `--config`) to use templates or other agents as config sources
- Per-agent workspace at `ik://agent/<UUID>/projects/` for repo checkouts and isolated work directories


### rel-16: Additional Deployment Targets (future)

**Objective**: Extend build and packaging support to macOS and Ubuntu

**Features**:
- macOS support (Homebrew packaging, platform-specific adaptations)
- Ubuntu support (apt packaging, systemd integration)
- CI matrix covering Linux, macOS, and Ubuntu


### rel-17: User Experience (future)

**Objective**: Polish configuration, discoverability, and customization workflows

**Features**:
- lots of tab auto completion
- improved status bar
- type while agent is thinking (buffer input, flush on response completion)
- paste like Claude Code (bracketed paste with preview before submit)
- auto-complete model names in `/model` command
- auto-complete agent UUIDs in `/switch`, `/kill`, `/send`, and similar commands


### rel-18: Codebase Refactor & MVP Release (future)

**Objective**: Improve code organization, reduce complexity, and clean up technical debt before MVP release.
