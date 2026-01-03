---
name: release
description: Release skill for the ikigai project
---

# Release

Release management for the ikigai project. One commit per release on main, with corresponding tag.

## Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main (bookmark)
- **Release pattern**: One squashed commit per release

## Branch Structure

```
main
├── rel-01  (bookmark/tag)
├── rel-02  (bookmark/tag)
├── rel-03  (bookmark/tag)
└── rel-04  (bookmark/tag)
```

Each release is a single commit. History is linear with no merge commits.

## Commit Message Format

### Title Line

```
rel-##: Short description of release
```

Examples:
- `rel-01: REPL terminal foundation`
- `rel-02: OpenAI integration and LLM streaming`
- `rel-03: Database integration and agent task system`
- `rel-04: Local tool execution, hooks system, and design refinements`

### Body Structure

Use Keep a Changelog format with markdown headers:

```
rel-##: Short description

### Added

#### Feature Category (Complete)
- Component: Description of what was added
- Component: Another addition

#### Another Category
- Item: Description

### Changed

#### Category
- Component: What changed and why

### Development

#### Testing & Quality Gates
- Metric: Value or description

#### Documentation
- Area: What was documented

### Technical Metrics
- Changes: X files modified, +Y/-Z lines
- Commits: N commits over development cycle
- Test coverage: 100% lines (X), functions (Y), and branches (Z)
- Code quality: All lint, format, and sanitizer checks pass
```

## Release Workflow

### 1. Prepare Release

Ensure all work is committed and ready. Use `jj log` to review history.

### 2. Squash to Single Commit

```bash
# Squash all commits since last release into one
jj squash --from <first-commit> --into <last-release>

# Or interactively fold commits
jj fold --from <range>

# Describe the release commit
jj describe -m "$(cat <<'EOF'
rel-##: Description

<changelog body>
EOF
)"
```

### 3. Create Release Bookmark

```bash
jj bookmark create rel-##
```

### 4. Push

```bash
jj git push --bookmark main
jj git push --bookmark rel-##
```

## CHANGELOG.md

Keep `CHANGELOG.md` in sync with commit messages. The changelog uses the same format as commit bodies.

## Attribution Policy

Do NOT include in commit messages:
- No "Co-Authored-By" lines
- No "Generated with Claude Code" footers

## Pre-Release Checklist

Before creating a release commit:

1. `make fmt` - Code formatted
2. `make check` - All tests pass
3. `make lint` - Complexity/size checks pass
4. `make coverage` - 100% coverage maintained
5. `make check-dynamic` - Sanitizers pass
6. Update `CHANGELOG.md` with release notes

## Updating Commit Messages

To update a commit message:

```bash
# Edit any mutable commit
jj describe <revision> -m "$(cat <<'EOF'
<new message from CHANGELOG.md>
EOF
)"

# Push updated bookmark
jj git push --bookmark main
```

## Rewriting History

jj makes history rewriting natural:

```bash
# Edit an old commit
jj edit <revision>
# Make changes, they're automatically part of that commit
jj new  # Return to tip

# Rebase descendants automatically happen
```

For bulk message updates, use `jj describe` on each commit or script with `jj log` templates.
