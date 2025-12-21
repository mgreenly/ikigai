---
name: research
description: Research skill for the ikigai project
---

# Research

Methodology for gathering external knowledge and translating it into actionable task specifications.

## Mindset

**Verify, don't assume.** When uncertain about APIs, standards, or best practices, search first. Hallucinated technical details create broken tasks.

## When to Research

- External API integration (OpenAI, Anthropic, libcurl, PostgreSQL, etc.)
- Library capabilities and version-specific features
- Standards and RFCs (HTTP, SSE, Unicode, terminal escape codes)
- Best practices that evolve over time
- Anything you're not 100% confident about

## WebSearch Patterns

Use `WebSearch` for discovery - finding what exists and where authoritative sources live.

**Effective queries:**
- `"libcurl SSE streaming example"` - Specific library + feature
- `"OpenAI API tool_choice 2024"` - API + feature + recent year
- `"ANSI escape sequences CSI u specification"` - Standard + specific variant
- `"talloc destructor pattern"` - Library + specific pattern

**Avoid:**
- Vague queries (`"how to do streaming"`)
- Questions (`"how do I parse JSON in C?"`) - use keywords instead

## WebFetch Patterns

Use `WebFetch` for deep dives - reading official documentation, API references, specifications.

**Priority order:**
1. Official documentation (e.g., platform.openai.com, curl.se/libcurl)
2. Specification documents (RFCs, standards)
3. Source code repositories (GitHub READMEs, examples)
4. High-quality tutorials (verified against official docs)

**Avoid:**
- Stack Overflow answers without verification
- Blog posts from unknown authors
- AI-generated content without source citations

## Source Evaluation

| Source Type | Trust Level | Use For |
|-------------|-------------|---------|
| Official docs | High | API contracts, parameters, behavior |
| RFCs/Standards | High | Protocol details, format specifications |
| Library source | High | Actual implementation behavior |
| Reputable blogs | Medium | Patterns, gotchas, real-world experience |
| Forum answers | Low | Starting points only - verify elsewhere |

## Documentation Practice

When researching, document findings with sources:

```markdown
## Research Findings

### OpenAI Tool Choice API
Source: https://platform.openai.com/docs/api-reference/chat/create

- `tool_choice: "auto"` - Model decides whether to call tools
- `tool_choice: "required"` - Model must call at least one tool
- `tool_choice: {"type": "function", "function": {"name": "..."}}` - Force specific tool
```

## Handling Uncertainty

Be explicit about confidence levels:

- **Verified**: Found in official documentation with citation
- **Likely**: Multiple reputable sources agree, but no official confirmation
- **Assumed**: Reasonable inference, needs verification during implementation
- **Unknown**: Could not determine, flag for developer investigation

Mark assumptions in task files so developers know what to verify.

## Output Location

Research findings go in `scratch/research/`:

```
scratch/
├── README.md           # Release goals (high-level)
└── research/
    ├── README.md       # Overview of research contents
    ├── anthropic.md    # Per-topic technical specs
    ├── openai.md
    └── ...
```

- `scratch/research/README.md` - Table of contents with brief descriptions
- Individual files per topic - detailed technical specifications with sources

Update the README.md as you add research files to keep the overview current.

## Research to Task Translation

1. **Gather** - Search and fetch until you understand the problem space
2. **Synthesize** - Identify what the codebase needs vs. what exists
3. **Specify** - Write task files with concrete, testable goals
4. **Cite** - Include sources in task pre-reads or reference sections

## Anti-patterns

- Assuming API behavior without checking documentation
- Using outdated information (check dates on sources)
- Trusting a single source for critical details
- Skipping research because "it's probably fine"
- Writing tasks with vague requirements that force developers to research
