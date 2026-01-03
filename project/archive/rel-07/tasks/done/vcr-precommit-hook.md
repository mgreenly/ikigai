# Task: VCR Pre-Commit Hook for Credential Leak Detection

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/none
**Depends on:** vcr-fixtures-setup.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Security Constraint

VCR fixtures must NEVER contain real API credentials. This pre-commit hook enforces credential redaction by scanning fixtures for:
- Unredacted Bearer tokens (`Bearer [^R]` - any Bearer token not followed by "REDACTED")
- OpenAI API key patterns (`sk-`)
- Anthropic API key patterns (`sk-ant-`)
- Google API key patterns (`AIza`)
- Brave API key patterns (`BSA`)

Any commit containing these patterns in `tests/fixtures/` will be rejected.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `tests/fixtures/` directory exists with VCR cassettes
- [ ] VCR credential redaction is implemented (from vcr-core.md Phase 1.6)

## Pre-Read

**Plan:**
- `scratch/plan/vcr-cassettes.md` - Credential redaction rules
- `scratch/vcr-tasks.md` - Phase 9 specification

## Objective

Create `.githooks/pre-commit` script to detect credential leaks in VCR fixtures before they are committed to version control.

**Note:** This project uses `.githooks/` (project-local hooks directory) instead of `.git/hooks/` for version-controlled, team-shared hooks. Developers must configure git to use this directory:

```bash
git config core.hooksPath .githooks
```

## Hook Behavior

### On Each Commit

1. Find all staged files in `tests/fixtures/`
2. Scan each file for credential patterns
3. If any pattern found:
   - Print error message showing filename and matched pattern
   - Exit with code 1 (reject commit)
4. If no patterns found:
   - Exit with code 0 (allow commit)

### Credential Patterns to Detect

| Pattern | Description | Example Match |
|---------|-------------|---------------|
| `Bearer [^R]` | Bearer token not followed by R (REDACTED) | `Bearer sk-abc123` |
| `sk-` | OpenAI API key prefix | `sk-proj-xyz789` |
| `sk-ant-` | Anthropic API key prefix | `sk-ant-api03-abc123` |
| `AIza` | Google API key prefix | `AIzaSyAbc123` |
| `BSA` | Brave API token prefix | `BSAabc123xyz` |

### False Positives Allowed

The patterns are conservative to minimize false positives:
- `Bearer REDACTED` will NOT match (correct redaction)
- Documentation files mentioning pattern examples are OK (hooks only scan `tests/fixtures/`)

## Script Implementation

Create `.githooks/pre-commit`:

```bash
#!/usr/bin/env bash
#
# Pre-commit hook for VCR fixture credential leak detection
#
# This hook scans VCR cassette files in tests/fixtures/ for API credentials
# that should have been redacted but weren't.
#
# To enable: git config core.hooksPath .githooks

set -euo pipefail

# Color output
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get list of staged fixture files
staged_fixtures=$(git diff --cached --name-only --diff-filter=ACM | grep '^tests/fixtures/' || true)

if [ -z "$staged_fixtures" ]; then
    # No fixture files staged, allow commit
    exit 0
fi

echo "Scanning VCR fixtures for credential leaks..."

# Track if any leaks found
found_leak=0

# Patterns to detect
declare -A patterns=(
    ["Unredacted Bearer token"]='Bearer [^R]'
    ["OpenAI API key"]='sk-'
    ["Anthropic API key"]='sk-ant-'
    ["Google API key"]='AIza'
    ["Brave API token"]='BSA'
)

# Scan each staged fixture
for file in $staged_fixtures; do
    for pattern_name in "${!patterns[@]}"; do
        pattern="${patterns[$pattern_name]}"

        # Search for pattern in staged content
        if git diff --cached --diff-filter=ACM -- "$file" | grep -E "$pattern" > /dev/null 2>&1; then
            echo -e "${RED}ERROR: $pattern_name detected in $file${NC}"
            echo -e "${YELLOW}Pattern: $pattern${NC}"

            # Show matching lines (context)
            echo "Matching lines:"
            git diff --cached --diff-filter=ACM -- "$file" | grep -E "$pattern" | head -n 3
            echo ""

            found_leak=1
        fi
    done
done

if [ $found_leak -eq 1 ]; then
    echo -e "${RED}COMMIT REJECTED: Credential leak detected in fixtures${NC}"
    echo ""
    echo "VCR fixtures must have API credentials redacted:"
    echo "  - Bearer tokens: 'Bearer REDACTED'"
    echo "  - x-api-key headers: 'REDACTED'"
    echo "  - x-goog-api-key headers: 'REDACTED'"
    echo ""
    echo "To fix:"
    echo "  1. Re-record fixtures with VCR credential redaction enabled"
    echo "  2. Manually edit the fixture to replace credentials with REDACTED"
    echo "  3. Verify with: grep -rE 'Bearer [^R]|sk-|sk-ant-|AIza|BSA' tests/fixtures/"
    exit 1
fi

echo "No credential leaks detected in fixtures."
exit 0
```

## Directory Structure

```
.githooks/
    pre-commit    # New file (executable)
```

## Git Configuration

After creating the hook, developers must configure git to use `.githooks/`:

```bash
# One-time setup (per clone)
git config core.hooksPath .githooks

# Verify
git config --get core.hooksPath  # Should output: .githooks
```

This configuration is **not** stored in the repository - each developer must run it after cloning.

## Testing the Hook

### Manual Test

```bash
# 1. Make hook executable
chmod +x .githooks/pre-commit

# 2. Configure git to use .githooks
git config core.hooksPath .githooks

# 3. Create a fixture with leaked credential
echo '{"_request": {"headers": {"Authorization": "Bearer sk-test123"}}}' > tests/fixtures/test_leak.jsonl
git add tests/fixtures/test_leak.jsonl

# 4. Attempt commit (should be rejected)
git commit -m "Test commit"
# Expected: ERROR message, exit code 1

# 5. Fix the leak
echo '{"_request": {"headers": {"Authorization": "Bearer REDACTED"}}}' > tests/fixtures/test_leak.jsonl
git add tests/fixtures/test_leak.jsonl

# 6. Attempt commit (should succeed)
git commit -m "Test commit"
# Expected: Success, exit code 0

# 7. Clean up test
git reset HEAD~1
rm tests/fixtures/test_leak.jsonl
```

### Automated Test

If test suite includes hook testing:

```bash
# Run from tests/hooks/ or similar
./test_precommit_hook.sh
```

## Postconditions

- [ ] `.githooks/pre-commit` exists and is executable (`chmod +x`)
- [ ] Hook script contains all 5 credential patterns
- [ ] Hook only scans `tests/fixtures/` directory
- [ ] Hook exits 0 when no leaks found
- [ ] Hook exits 1 when leaks detected
- [ ] Hook prints clear error messages with pattern and filename
- [ ] Hook prints remediation instructions
- [ ] Manual test passes (leak detection and remediation)
- [ ] `git config core.hooksPath .githooks` is documented (in commit message or README)
- [ ] Changes committed to git with message: `task: vcr-precommit-hook.md - add credential leak detection for VCR fixtures`
  - If hook works correctly: success message
  - If hook fails: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).

## Notes

- Hook runs on every commit touching `tests/fixtures/`
- Hook does NOT scan other directories (allows pattern examples in docs)
- Hook scans staged content (via `git diff --cached`), not working tree
- Developers can bypass with `git commit --no-verify` (not recommended)
- Pattern matching is case-sensitive (API keys are case-sensitive)
- Hook is version-controlled in `.githooks/` (shared across team)
- Git config is local (each developer must configure after clone)
