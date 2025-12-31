# rel-08 User Stories

User-perceived feature descriptions documenting terminal interactions for web search functionality.

## User Stories

| Story | Description | Status |
|-------|-------------|--------|
| [first-time-discovery.md](first-time-discovery.md) | User discovers web search feature when LLM tries to use it without credentials configured | Complete |
| [successful-search.md](successful-search.md) | Normal operation showing search tool returning multiple results and LLM synthesizing answer | Complete |
| [rate-limit-exceeded.md](rate-limit-exceeded.md) | Graceful handling when user exhausts free search quota with actionable alternatives | Complete |

## Format

Each user story file includes:
- **Title** - Feature name
- **Description** - Brief overview
- **Transcript** - Terminal output showing user interaction
- **Walkthrough** - Step-by-step explanation
- **Reference** - Full JSON payloads (if applicable)

See `.claude/library/user-stories/SKILL.md` for detailed format specification.
