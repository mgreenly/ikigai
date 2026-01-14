# Copying Ralph to Your Project

Ralph is a standalone agentic loop harness. You can copy it into your own project with minimal dependencies.

## Prerequisites

- **Ruby** (tested with 3.x, should work with 2.7+)
- **Claude Code CLI** (`claude` command available)
- **jj (Jujutsu)** for version control (or tell claude to change it back to git)

## Quick Setup

### 1. Copy the harness folder

```bash
cp -r .claude/harness/ralph /path/to/your/project/.claude/harness/ralph
```

This includes:
- `run` - The main Ruby script
- `prompt.md.erb` - ERB template for worker agent prompts
- `summarizer.md.erb` - ERB template for summarizer agent prompts
- `ralph.ascii` - ASCII art header (optional, use `--no-art` to skip)

### 2. Create a symlink for convenience

```bash
ln -s ../harness/ralph/run /path/to/your/project/.claude/scripts/ralph
```

Or place it wherever makes sense for your project:
```bash
# Alternative: use a bin directory
mkdir -p /path/to/your/project/.claude/bin
ln -s ../harness/ralph/run /path/to/your/project/.claude/bin/ralph
```

### 3. Add scripts to your PATH

I use [direnv](https://direnv.net/) with an `.envrc` file:

```bash
# .envrc
PATH_add .claude/scripts
```

Then `direnv allow` and you can run `ralph` from anywhere in your project.

**Alternatives:**
- Add to shell profile: `export PATH="$PATH:/path/to/project/.claude/scripts"`
- Run directly: `.claude/harness/ralph/run --goal=...`

### 4. Optional: Copy the skill documentation

If you use Claude Code's skill system, copy the skill file:

```bash
mkdir -p /path/to/your/project/.claude/library/ralph
cp .claude/library/ralph/SKILL.md /path/to/your/project/.claude/library/ralph/
```

Or place it in Claude's global skills folder:
```bash
cp .claude/library/ralph/SKILL.md ~/.claude/library/ralph/SKILL.md
```

## Customization

### Modify the prompt template (IMPORTANT)

**You will need to edit `prompt.md.erb`** - it contains project-specific content:
- References to project-specific harness scripts (`.claude/harness/compile/run`, etc.)
- Skill loading from `.claude/skillsets/implementor.json`
- Project-specific guidance and scripts table

At minimum, remove or replace:
- The `# Scripts` section listing project-specific check scripts
- The `# Skills` section if you don't use that system
- Any guidance that references project conventions

The template uses ERB with these variables:
- `<%= goal %>` - Contents of the goal file
- `<%= summary %>` - Condensed history from summarizer
- `<%= summary_end %>` - Last iteration covered by summary
- `<%= recent %>` - Recent iteration progress (not yet summarized)
- `<%= skills %>` - Loaded skill content (can be empty string)
- `<%= advertised_skills %>` - Available skills table (can be empty string)

### Modify the summarizer template

Edit `summarizer.md.erb` to change how progress gets condensed.

### Change version control

Ralph uses `jj commit` for committing each iteration. To use git instead, modify `commit_iteration`:

```ruby
def commit_iteration(progress)
  short_progress = progress.to_s[0, 200]
  short_progress += '...' if progress.to_s.length > 200

  msg = <<~MSG
    ralph: iteration #{@iteration}

    #{short_progress}
  MSG

  # Change this line:
  system("git add -A && git commit -m #{msg.shellescape} >/dev/null 2>&1")
end
```

### Change skillset integration

Ralph loads skills from `.claude/skillsets/implementor.json`. Modify `load_skills` and `build_advertised_skills` to match your skill structure, or remove skill loading entirely if you don't use that system.

### Notifications (optional)

Ralph can send push notifications via [ntfy](https://ntfy.sh/) when complete. **If these environment variables aren't set, notifications are silently skipped** - no configuration needed to disable them.

To enable notifications:
```bash
export NTFY_TOPIC=your-topic
export NTFY_API_KEY=your-key
```

Modify `send_notification` to use a different notification service, or remove it entirely.

## Directory Structure Flexibility

The paths used here are conventions, not requirements:

| This project uses | You could use |
|-------------------|---------------|
| `.claude/harness/ralph/` | `.claude/agents/ralph/` |
| `.claude/scripts/ralph` | `.claude/bin/ralph` |
| `.claude/library/ralph/` | `~/.claude/library/ralph/` |

The only hard requirement is that `prompt.md.erb` and `summarizer.md.erb` are in the same directory as `run` (they're loaded via `SCRIPT_DIR`).

### Add state files to .gitignore

Ralph creates progress and summary files alongside your goal files. Consider adding these patterns to `.gitignore`:

```gitignore
# Ralph loop state files (any directory)
**/*-progress.jsonl
**/*-summary.md
**/*-summary-meta.json
```

Or if you use a naming convention like `*-ralph-goal.md`:

```gitignore
# Ralph loop state files (any directory)
**/*-ralph-progress.jsonl
**/*-ralph-summary.md
**/*-ralph-summary-meta.json
```

Goal files themselves are typically worth keeping in version control.

## Running Ralph

```bash
ralph --goal=path/to/goal.md [--duration=4h] [--model=sonnet] [--reasoning=low]
```

See the skill documentation or run `ralph --help` for all options.

## Goal File Format

Create a markdown file with at least an `## Objective` section:

```markdown
## Objective

What you want accomplished.

## Reference

Optional: pointers to specs, plans, or context.

## Acceptance

Optional: how to verify completion (e.g., "tests pass").
```

State files (`-progress.jsonl`, `-summary.md`) are automatically created alongside the goal file.

## License

Do whatever you want with this code. No attribution required.
