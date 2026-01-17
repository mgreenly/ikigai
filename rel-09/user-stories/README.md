# rel-09 User Stories

User-perceived feature descriptions documenting terminal interactions for web tools functionality.

## User Stories

| Story | Description | Status |
|-------|-------------|--------|
| [first-time-discovery.md](first-time-discovery.md) | User discovers web search feature when LLM tries to use it without credentials configured | Complete |
| [successful-search.md](successful-search.md) | Normal operation showing Brave search tool returning multiple results and LLM synthesizing answer | Complete |
| [web-fetch.md](web-fetch.md) | User asks LLM to fetch and analyze URL content, demonstrating HTML to markdown conversion | Complete |
| [google-search.md](google-search.md) | Using Google Custom Search as alternative provider with two-credential setup | Complete |
| [rate-limit-exceeded.md](rate-limit-exceeded.md) | Graceful handling when user exhausts free search quota with actionable alternatives | Complete |

## Format

Each user story file includes:
- **Title** - Feature name
- **Description** - Brief overview
- **Transcript** - Terminal output showing user interaction
- **Walkthrough** - Step-by-step explanation
- **Reference** - Full JSON payloads (if applicable)

See `.claude/library/user-stories/SKILL.md` for detailed format specification.
