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

# write_manifest <root> <name> <contents>
write_manifest() {
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
write_manifest "$ROOT" crm $'# crm producer\nAPP=crm\nMOUNT="/srv/crm/"\nPORT=3001\nMCP=true\nFEED=/feed'
# consumer with MCP + CONSUMES, no FEED
write_manifest "$ROOT" notify $'APP=notify\nMOUNT=/srv/notify/\nPORT=3003\nMCP=true\nCONSUMES=crm'
# plain non-MCP service
write_manifest "$ROOT" dashboard $'APP=dashboard\nMOUNT=/\nDEFAULT=true\nPORT=3000'
# service missing PORT
write_manifest "$ROOT" noport $'APP=noport\nMOUNT=/srv/noport/\nMCP=true'

assert_eq "port crm" "3001" "$("$REGISTRY" port crm)"
assert_eq "addr crm" "127.0.0.1:3001" "$("$REGISTRY" addr crm)"
assert_eq "mount crm (quote-stripped)" "/srv/crm/" "$("$REGISTRY" mount crm)"
assert_eq "feed-url crm" "http://127.0.0.1:3001/feed" "$("$REGISTRY" feed-url crm)"
assert_eq "resource-url crm" "https://int.ikigenba.com/srv/crm/mcp" "$("$REGISTRY" resource-url int.ikigenba.com crm)"
assert_eq "mount dashboard" "/" "$("$REGISTRY" mount dashboard)"
# noport also has MCP=true, so it is listed too (list-mcp keys only on MCP=true).
assert_eq "list-mcp (sorted, mcp only)" $'crm\nnoport\nnotify' "$("$REGISTRY" list-mcp)"

assert_fails "feed-url on non-producer (notify)" "$REGISTRY" feed-url notify
assert_fails "resource-url on non-MCP (dashboard)" "$REGISTRY" resource-url int.ikigenba.com dashboard
assert_fails "port on missing service" "$REGISTRY" port nope
assert_fails "port on service missing PORT" "$REGISTRY" port noport
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
