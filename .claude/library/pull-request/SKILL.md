---
name: pull-request
description: Creating pull requests with concise descriptions
---

# Pull Request

## Creating PRs

Use `gh pr create` with a concise description. No test plan section - CI runs the full quality suite automatically.

## Template

```markdown
<concise description of what changed and why>

---
🤖 Generated with [Claude Code](https://claude.ai/code)
```

## Command

```bash
gh pr create --title "<title>" --body "$(cat <<'EOF'
<description>

---
🤖 Generated with [Claude Code](https://claude.ai/code)
EOF
)"
```

## Guidelines

- **Title:** Imperative mood, concise (e.g., "Add user authentication", "Fix memory leak in parser")
- **Description:** One line or short paragraph explaining what and why
- **No headers:** They add noise for typical PRs
- **No test plan:** Implicit - CI runs quality checks
- **Footer:** Attribution preserved

## Examples

**Simple change:**
```
Remove dead code: ik_content_block_thinking

---
🤖 Generated with [Claude Code](https://claude.ai/code)
```

**Feature addition:**
```
Add JSON export for metrics data

Enables users to export their usage metrics in JSON format
for integration with external tools.

---
🤖 Generated with [Claude Code](https://claude.ai/code)
```

**Bug fix:**
```
Fix null pointer dereference in config parser

The parser didn't handle missing optional fields correctly
when the config file used the legacy format.

---
🤖 Generated with [Claude Code](https://claude.ai/code)
```
