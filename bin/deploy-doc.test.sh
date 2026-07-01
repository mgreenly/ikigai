#!/usr/bin/env bash
#
# deploy-doc.test.sh — guards the deploy runbook against the old deploy model.
set -uo pipefail

DOC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/deploy.md"
fails=0

assert_contains() {
	local label="$1" needle="$2"
	if grep -Fq -- "$needle" "$DOC"; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: missing [$needle]"
		fails=$((fails + 1))
	fi
}

assert_not_contains() {
	local label="$1" needle="$2"
	if grep -Fq -- "$needle" "$DOC"; then
		echo "FAIL - $label: stale text present [$needle]"
		fails=$((fails + 1))
	else
		echo "ok   - $label"
	fi
}

assert_not_matching() {
	local label="$1" regex="$2"
	if grep -Eiq -- "$regex" "$DOC"; then
		echo "FAIL - $label: stale pattern matched [$regex]"
		fails=$((fails + 1))
	else
		echo "ok   - $label"
	fi
}

assert_contains "documents bump ship stage deploy sequence" "bump → ship → stage → deploy"
assert_contains "documents tar.gz bundle staging" "tar.gz bundle"
assert_contains "documents versioned remote bundle" "/tmp/<app>-v<ver>+<sha>.tar.gz"
assert_contains "documents unconditional backup" "unconditional"
assert_contains "documents rollback -N by recency" "rollback <app> -N"
assert_contains "documents snapshot recency" "selected by recency"
assert_contains "documents state database path" "state/<svc>.db"
assert_contains "documents IKIGENBA_ROOT" "IKIGENBA_ROOT"
assert_contains "documents three-symlink swap" "three-symlink swap"
assert_contains "documents nginx through etc/current" "reloads the nginx fragment through \`etc/current\`"
assert_contains "documents no manifest generation step" 'There is no `manifest`-verb / on-box manifest-generation step'

assert_not_contains "omits stale manifest regeneration wording" 'regenerate `etc/manifest.env`'
assert_not_matching "omits schema-conditional backup wording" 'back[[:space:]]+up[^[:cntrl:]]*schema advances|backup[^[:cntrl:]]*schema advances'
assert_not_contains "omits local pre-migration backup path" "backups/"
assert_not_contains "omits stale releases version path" "releases/<v>/"
assert_not_contains "omits stale current indirection path" "releases/current"
assert_not_contains "omits old data directory path" "/opt/<svc>/data/"

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
