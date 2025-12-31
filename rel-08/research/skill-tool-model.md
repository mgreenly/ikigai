# Skills + Tools Extension Model

Research on the extension architecture beyond rel-08's external tools.

## The Pattern

- **Skill** - Context: understands when/why to use a capability, domain knowledge, patterns
- **Tool** - Action: does exactly what we need, nothing more

We write both. We control both ends.

## Why This Wins

A skill understands the context of when to do a thing. A tool does the exact thing we want. We provide both.

No vendor floods our context with their entire offering. We expose precisely what we need.

## What About Vendor APIs?

Write a tool that calls exactly the API endpoints you need:
- The tool does the targeted thing
- You control what's exposed to the agent
- Direct API calls are cleaner than wrapping vendor abstractions

If a vendor offers MCP, the tool can wrap it internally - but there's usually no reason to. Just call the API.

## When Is MCP Useful?

Only in non-specialized situations where you didn't know you'd need the functionality ahead of time.

For planned work, purpose-built tools win. You've already decided what capabilities you need - write tools that do exactly that.

## Three-Tier Command System (Future)

```
/command         - ikigai system operations (/clear, /mark, /rewind, /fork)
!skill [params]  - user-defined content insertion (template expansion)
natural language - LLM work (tools, reasoning, conversation)
```

**Semantic distinction:**
- `/` talks to ikigai (manipulates conversation state)
- `!` talks to the context system (inserts domain knowledge)
- Natural language talks to the LLM (does work with tools)

## Skills = Knowledge + Tool References

A skill is a parameterized context template that teaches domain knowledge AND mentions relevant tools:

```markdown
# .ikigai/skills/database/SKILL.md

## Schema
{{#if table}}
Table: {{table}}
[schema for that specific table]
{{else}}
[full database schema]
{{/if}}

## Available Tools
- `database-query` - Execute SQL with result formatting
- `database-migrate` - Run migrations safely
- `database-backup` - Create timestamped backups

## Patterns
Use prepared statements for all queries...
Transaction management best practices...
```

**User workflow:**
```
> !database table=users
[Skill content inserted - LLM now knows users table schema]

> use database-query to show me all admin users
[LLM calls tool with domain understanding from skill]
```

## Design Questions

- Template engine: Handlebars? Simple substitution?
- Discovery: Tab completion? `/skills list`?
- Skill location: `.ikigai/skills/` or `.claude/library/`?
- Parameter validation: Typed schemas or freeform?

## Relationship to rel-08

External tools (rel-08) establish the capability layer. Skills complete the picture with the knowledge layer. But the key insight is: we control both. No dependency on vendor-defined abstractions.
