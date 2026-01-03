---
name: release
description: Release skill for the ikigai project
---

# Release

Release management for the ikigai project. One commit per release on main, with corresponding tag.

## Git Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main
- **Release pattern**: One squashed commit per release

## Branch Structure

```
main
├── rel-01  (tag)
├── rel-02  (tag)
├── rel-03  (tag)
└── rel-04  (tag)
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

Ensure all work is on a feature branch or accumulated commits are ready.

### 2. Squash to Single Commit

```bash
# From main, soft reset to previous release
git reset --soft <previous-release-commit>

# Create release commit with full changelog
git commit -m "$(cat <<'EOF'
rel-##: Description

<changelog body>
EOF
)"
```

### 3. Tag the Release

```bash
git tag rel-##
```

### 4. Push

```bash
git push --force origin main
git push origin rel-##
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

## Rebuilding Commit Messages

To update a commit message from the changelog:

```bash
git commit --amend -m "$(cat <<'EOF'
<new message from CHANGELOG.md>
EOF
)"
git push --force origin main
```

## Rewriting History

To rewrite multiple commit messages:

```bash
# Use filter-branch with msg-filter
git filter-branch -f --msg-filter 'script.sh' -- --all
git push --force origin main
```
