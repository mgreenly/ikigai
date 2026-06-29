#!/usr/bin/env bash
#
# bump.test.sh — self-contained tests for bin/bump and committed VERSION shape.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
BUMP="$HERE/bump"

fails=0
FIXES=()

assert_eq() {
	local label="$1" want="$2" got="$3"
	if [ "$want" = "$got" ]; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: want [$want] got [$got]"
		fails=$((fails + 1))
	fi
}

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

new_fixture() {
	local version="${1-v0.7.1}"
	FIX="$(mktemp -d)"
	FIXES+=("$FIX")
	mkdir -p "$FIX/bin" "$FIX/sites"
	cp "$BUMP" "$FIX/bin/bump"
	chmod +x "$FIX/bin/bump"
	git -C "$FIX" init -q
	git -C "$FIX" config user.email t@t.t
	git -C "$FIX" config user.name t
	git -C "$FIX" symbolic-ref HEAD refs/heads/main
	printf '%s\n' "$version" >"$FIX/sites/VERSION"
	git -C "$FIX" add -A
	git -C "$FIX" commit -qm baseline
	REMOTE="$(mktemp -d)"
	FIXES+=("$REMOTE")
	git -C "$REMOTE" init -q --bare
	git -C "$FIX" remote add origin "$REMOTE"
	git -C "$FIX" push -q -u origin main
}

trap 'for f in "${FIXES[@]}"; do rm -rf "$f"; done' EXIT

# R-44HU-2IOD
new_fixture v0.7.1
if "$FIX/bin/bump" sites patch >/dev/null 2>&1; then
	assert_eq "bump patch carries v prefix" "v0.7.2" "$(cat "$FIX/sites/VERSION")"
else
	echo "FAIL - bump patch carries v prefix: command failed"
	fails=$((fails + 1))
fi

new_fixture 0.7.1
assert_fails "bump refuses old bare VERSION" "$FIX/bin/bump" sites patch

# R-439X-OQXO
for bad in 0.7.1 v0.7 vnot-semver v0.7.x ""; do
	new_fixture "$bad"
	assert_fails "bump refuses invalid VERSION [$bad]" "$FIX/bin/bump" sites patch
done

count=0
for version_file in "$REPO"/{crm,cron,dashboard,dropbox,gmail,ledger,notify,prompts,scripts,sites,webhooks,wiki}/VERSION; do
	count=$((count + 1))
	value="$(tr -d '[:space:]' < "$version_file")"
	if [[ "$value" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
		echo "ok   - ${version_file#$REPO/} is v-prefixed SemVer"
	else
		echo "FAIL - ${version_file#$REPO/}: invalid VERSION [$value]"
		fails=$((fails + 1))
	fi
done
assert_eq "all committed service VERSION files checked" "12" "$count"

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
