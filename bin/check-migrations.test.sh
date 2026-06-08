#!/usr/bin/env bash
#
# check-migrations.test.sh — self-contained tests for bin/check-migrations.
#
# Each case builds a throwaway git repo in a mktemp -d fixture, copies the script
# in, lays down service migration dirs (and, for the immutability cases, commits a
# baseline on `main` then mutates the tree), and asserts the expected exit status.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHECK="$HERE/check-migrations" # copied into each fixture's bin/ by new_fixture

fails=0

# The script resolves its repo root from its own BASH_SOURCE path, so each case
# runs the COPY placed in the fixture's bin/ (not the real one), which roots it at
# the fixture.

# assert_ok <label> — runs the check in $FIX, expects exit 0.
assert_ok() {
	local label="$1"
	if "$FIX/bin/check-migrations" >/dev/null 2>&1; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: expected exit 0, got non-zero"
		fails=$((fails + 1))
	fi
}

# assert_fails <label> — runs the check in $FIX, expects non-zero exit.
assert_fails() {
	local label="$1"
	if "$FIX/bin/check-migrations" >/dev/null 2>&1; then
		echo "FAIL - $label: expected non-zero exit, got 0"
		fails=$((fails + 1))
	else
		echo "ok   - $label"
	fi
}

# new_fixture — fresh mktemp -d git repo with a copy of the script at bin/.
new_fixture() {
	FIX="$(mktemp -d)"
	FIXES+=("$FIX")
	mkdir -p "$FIX/bin"
	cp "$CHECK" "$FIX/bin/check-migrations"
	chmod +x "$FIX/bin/check-migrations"
	git -C "$FIX" init -q
	git -C "$FIX" config user.email t@t.t
	git -C "$FIX" config user.name t
	git -C "$FIX" symbolic-ref HEAD refs/heads/main
}

# mig <service> <basename> <body> — write a migration file (creates the dir).
mig() {
	local svc="$1" base="$2" body="${3:-- noop}"
	mkdir -p "$FIX/$svc/internal/db/migrations"
	printf '%s\n' "$body" >"$FIX/$svc/internal/db/migrations/$base"
}

FIXES=()
trap 'for f in "${FIXES[@]}"; do rm -rf "$f"; done' EXIT

# --- 1. clean set passes (mixed legacy integer + timestamp) -----------------
new_fixture
mig crm 001_init.sql
mig crm 003_outbox.sql
mig crm 20260607143022_add_widgets.sql
mig ledger 001_init.sql
assert_ok "clean mixed set passes"

# --- 2. file with no numeric prefix fails naming ----------------------------
new_fixture
mig crm 001_init.sql
mig crm foo.sql
assert_fails "no-numeric-prefix file fails naming"

# --- 3. two files same prefix in one service fails uniqueness ---------------
new_fixture
mig crm 001_init.sql
mig crm 004_a.sql
mig crm 004_b.sql
assert_fails "duplicate prefix in one service fails uniqueness"

# --- 3b. same prefix across DIFFERENT services is fine ----------------------
new_fixture
mig crm 001_init.sql
mig ledger 001_init.sql
assert_ok "same prefix across different services passes"

# --- immutability fixtures: commit a baseline on main, then mutate ----------

# The immutability check diffs merge-base(origin/main|main, HEAD)..HEAD. Each
# fixture commits a baseline on `main`, branches to `work`, then mutates there so
# the merge-base is the baseline and the diff carries the mutation — mirroring a
# PR branch off main.

# 4. modifying an existing committed migration fails immutability.
new_fixture
mig crm 001_init.sql "CREATE TABLE a;"
git -C "$FIX" add -A && git -C "$FIX" commit -qm baseline
git -C "$FIX" checkout -q -b work
printf 'CREATE TABLE a; -- edited\n' >"$FIX/crm/internal/db/migrations/001_init.sql"
git -C "$FIX" add -A && git -C "$FIX" commit -qm edit
assert_fails "modified existing migration fails immutability"

# 5. deleting an existing committed migration fails immutability.
new_fixture
mig crm 001_init.sql "CREATE TABLE a;"
mig crm 002_more.sql "CREATE TABLE b;"
git -C "$FIX" add -A && git -C "$FIX" commit -qm baseline
git -C "$FIX" checkout -q -b work
git -C "$FIX" rm -q "crm/internal/db/migrations/002_more.sql"
git -C "$FIX" commit -qm delete
assert_fails "deleted existing migration fails immutability"

# 6. added-only diff passes immutability.
new_fixture
mig crm 001_init.sql "CREATE TABLE a;"
git -C "$FIX" add -A && git -C "$FIX" commit -qm baseline
git -C "$FIX" checkout -q -b work
mig crm 20260607143022_add_widgets.sql "CREATE TABLE w;"
git -C "$FIX" add -A && git -C "$FIX" commit -qm add
assert_ok "added-only diff passes immutability"

# 7. renaming an existing committed migration fails immutability.
new_fixture
mig crm 001_init.sql "CREATE TABLE a;"
git -C "$FIX" add -A && git -C "$FIX" commit -qm baseline
git -C "$FIX" checkout -q -b work
git -C "$FIX" mv "crm/internal/db/migrations/001_init.sql" "crm/internal/db/migrations/001_renamed.sql"
git -C "$FIX" commit -qm rename
assert_fails "renamed existing migration fails immutability"

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
