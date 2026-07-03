#!/usr/bin/env bash
#
# registry.test.sh — self-contained tests for bin/registry.
#
# Builds a temp REGISTRY_ROOT with fixture manifests, exercises every subcommand,
# and asserts both success output and that error cases exit non-zero.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REGISTRY="$HERE/registry"

fails=0

# write_current_manifest <root> <name> <version> <contents>
write_current_manifest() {
	local root="$1" name="$2" version="$3" contents="$4"
	mkdir -p "$root/$name/etc/$version"
	printf '%s\n' "$contents" >"$root/$name/etc/$version/manifest.env"
	ln -s "$version" "$root/$name/etc/current"
}

# write_sibling_manifest <root> <name> <contents>
write_sibling_manifest() {
	local root="$1" name="$2" contents="$3"
	mkdir -p "$root/$name/etc"
	printf '%s\n' "$contents" >"$root/$name/etc/manifest.env"
}

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

ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT
export REGISTRY_ROOT="$ROOT"

# producer with FEED + MCP, quoted value to exercise quote-stripping
write_current_manifest "$ROOT" crm v1 $'# crm producer\nAPP=crm\nMOUNT="/srv/crm/"\nPORT=3100\nMCP=true\nFEED=/feed'
# consumer with MCP + CONSUMES, no FEED
write_current_manifest "$ROOT" notify v3 $'APP=notify\nMOUNT=/srv/notify/\nPORT=3201\nMCP=true\nCONSUMES=crm'
# plain non-MCP service
write_current_manifest "$ROOT" dashboard release-20260703 $'APP=dashboard\nMOUNT=/\nDEFAULT=true\nPORT=3000'
# service missing PORT
write_current_manifest "$ROOT" noport v2 $'APP=noport\nMOUNT=/srv/noport/\nMCP=true'
# sibling manifest only; current-relative registry must ignore it
write_sibling_manifest "$ROOT" legacy $'APP=legacy\nMOUNT=/srv/legacy/\nPORT=3999\nMCP=true\nFEED=/legacy-feed'

# R-YQFZ-11IM
assert_eq "port crm" "3100" "$("$REGISTRY" port crm)"
assert_eq "addr crm" "127.0.0.1:3100" "$("$REGISTRY" addr crm)"
assert_eq "mount crm (quote-stripped)" "/srv/crm/" "$("$REGISTRY" mount crm)"
assert_eq "feed-url crm" "http://127.0.0.1:3100/feed" "$("$REGISTRY" feed-url crm)"
assert_eq "resource-url crm" "https://int.ikigenba.com/srv/crm/mcp" "$("$REGISTRY" resource-url int.ikigenba.com crm)"
assert_eq "mount dashboard" "/" "$("$REGISTRY" mount dashboard)"
# noport also has MCP=true, so it is listed too (list-mcp keys only on MCP=true).
assert_eq "list-mcp (sorted, mcp only)" $'crm\nnoport\nnotify' "$("$REGISTRY" list-mcp)"

assert_fails "feed-url on non-producer (notify)" "$REGISTRY" feed-url notify
assert_fails "resource-url on non-MCP (dashboard)" "$REGISTRY" resource-url int.ikigenba.com dashboard
assert_fails "port on missing service" "$REGISTRY" port nope
assert_fails "port on service missing PORT" "$REGISTRY" port noport
assert_fails "addr ignores sibling-manifest-only service" "$REGISTRY" addr legacy
assert_fails "feed-url ignores sibling-manifest-only service" "$REGISTRY" feed-url legacy
assert_fails "resource-url ignores sibling-manifest-only service" "$REGISTRY" resource-url int.ikigenba.com legacy
assert_fails "unknown subcommand" "$REGISTRY" bogus
assert_fails "no subcommand" "$REGISTRY"

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
