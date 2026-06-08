#!/usr/bin/env bash
#
# new-migration.test.sh — self-contained tests for bin/new-migration.
#
# Builds a temp REPO with fixture service migration dirs, points the script at it
# via a wrapper that overrides $REPO, exercises validation/refusal/slugify, and
# asserts the generated filename shape. Cleans up all generated files on exit.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEWMIG="$HERE/new-migration"

fails=0

# assert_eq <label> <expected> <actual>
assert_eq() {
	local label="$1" want="$2" got="$3"
	if [ "$want" = "$got" ]; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: want [$want] got [$got]"
		fails=$((fails + 1))
	fi
}

# assert_match <label> <regex> <actual>
assert_match() {
	local label="$1" re="$2" got="$3"
	if [[ "$got" =~ $re ]]; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: [$got] does not match /$re/"
		fails=$((fails + 1))
	fi
}

# assert_fails <label> <cmd...> — expects non-zero exit.
assert_fails() {
	local label="$1"
	shift
	if "$@" >/dev/null 2>&1; then
		echo "FAIL - $label: expected non-zero exit, got 0"
		fails=$((fails + 1))
	else
		echo "ok   - $label"
	fi
}

# Build a throwaway repo with a couple of fixture service migration dirs. The
# script resolves $REPO from its own location (bin/..), so we copy it into the
# fixture's bin/ and run it from there — that makes the fixture the repo root and
# guarantees the test never touches a real service migrations directory.
ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT

mkdir -p "$ROOT/bin"
cp "$NEWMIG" "$ROOT/bin/new-migration"
chmod +x "$ROOT/bin/new-migration"
RUN="$ROOT/bin/new-migration"

mkdir -p "$ROOT/crm/internal/db/migrations"
mkdir -p "$ROOT/ledger/internal/db/migrations"

# --- usage / validation errors ---------------------------------------------
assert_fails "no args" "$RUN"
assert_fails "missing name" "$RUN" crm
assert_fails "nonexistent service" "$RUN" nope add_widgets

# --- refusal of frozen shared files ----------------------------------------
assert_fails "refuse outbox" "$RUN" crm outbox
assert_fails "refuse feed_offset" "$RUN" crm feed_offset
assert_fails "refuse 'Feed Offset' (slugifies to feed_offset)" "$RUN" crm "Feed Offset"

# --- successful generation + filename shape --------------------------------
OUT="$("$RUN" crm "Add Widgets Table")"
assert_match "stdout is a path under crm migrations" \
	'^crm/internal/db/migrations/[0-9]{14}_add_widgets_table\.sql$' "$OUT"
BASE="$(basename "$OUT")"
assert_match "basename matches naming convention" \
	'^[0-9]{14}_[a-z0-9_]+\.sql$' "$BASE"
[ -f "$ROOT/$OUT" ] && echo "ok   - file exists on disk" || { echo "FAIL - file not on disk: $OUT"; fails=$((fails+1)); }

# header template present
if grep -q "IMMUTABLE" "$ROOT/$OUT" && grep -q "adr-migration-timestamps" "$ROOT/$OUT"; then
	echo "ok   - header template present"
else
	echo "FAIL - header template missing in $OUT"
	fails=$((fails + 1))
fi

# --- slugification edge cases ----------------------------------------------
OUT2="$("$RUN" ledger "  Foo--BAR  baz!! ")"
S2="$(basename "$OUT2")"
assert_match "slugify collapses/trims/lowercases" '^[0-9]{14}_foo_bar_baz\.sql$' "$S2"

# empty-after-slugify name fails
assert_fails "name of only punctuation fails" "$RUN" crm "!!!"

# --- collision guard -------------------------------------------------------
# The timestamp drives the filename, so pre-create the exact target for "now" and
# (within the same second) the script must refuse rather than overwrite. To make
# this deterministic regardless of which second the script runs in, pre-create a
# file for both the current second and the next, covering a one-second boundary.
NOW="$(date -u +%Y%m%d%H%M%S)"
NEXT="$(date -u -d '+1 second' +%Y%m%d%H%M%S 2>/dev/null || echo "$NOW")"
echo existing >"$ROOT/crm/internal/db/migrations/${NOW}_collide_guard.sql"
[ "$NEXT" != "$NOW" ] && echo existing >"$ROOT/crm/internal/db/migrations/${NEXT}_collide_guard.sql"
assert_fails "collision guard refuses existing target" "$RUN" crm collide_guard
# confirm the script did not clobber the pre-existing content
assert_eq "pre-existing file untouched" "existing" "$(cat "$ROOT/crm/internal/db/migrations/${NOW}_collide_guard.sql")"

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
