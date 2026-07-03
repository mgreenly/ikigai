#!/usr/bin/env bash
#
# live-smoke.test.sh — starts the local suite, verifies the staged runtime
# manifest root and the dashboard's public /services inventory, then tears down
# only the suite started by this script.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
RUN_DIR="$REPO/tmp"
MANIFEST_ROOT="$RUN_DIR/opt"

SERVICES=(dashboard crm ledger notify prompts wiki dropbox cron gmail scripts sites webhooks)
MCP_SERVICES=(crm ledger notify prompts wiki dropbox cron gmail scripts sites webhooks)
PORTS=(3000 3100 3101 3201 3002 3001 3200 3005 3202 3003 3004 3006 8080)

fails=0
started=0

cleanup() {
	if [ "$started" -eq 1 ]; then
		"$HERE/stop" --clean >/dev/null 2>&1 || true
	fi
}
trap cleanup EXIT

port_open() {
	(exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null
}

assert_ok() {
	local label="$1"
	shift
	if "$@"; then
		echo "ok   - $label"
	else
		echo "FAIL - $label"
		fails=$((fails + 1))
	fi
}

assert_contains() {
	local label="$1" needle="$2" haystack="$3"
	if [[ "$haystack" == *"$needle"* ]]; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: missing [$needle]"
		fails=$((fails + 1))
	fi
}

for port in "${PORTS[@]}"; do
	if port_open "$port"; then
		echo "FAIL - port :$port is already in use; not stopping a suite this test did not start"
		exit 1
	fi
done

started=1
if ! "$HERE/start"; then
	echo "FAIL - bin/start exited non-zero"
	exit 1
fi

# R-YRNV-ET9B
for svc in "${SERVICES[@]}"; do
	current="$MANIFEST_ROOT/$svc/etc/current"
	manifest="$current/manifest.env"
	target="$(readlink "$current" 2>/dev/null || true)"

	assert_ok "$svc current is a symlink" test -L "$current"
	assert_ok "$svc current manifest exists" test -f "$manifest"
	assert_ok "$svc current symlink is relative" test "${target#/}" = "$target"
done

dashboard_pid="$(cat "$RUN_DIR/dashboard.pid" 2>/dev/null || true)"
if [ -n "$dashboard_pid" ] && [ -r "/proc/$dashboard_pid/environ" ]; then
	dashboard_env="$(tr '\0' '\n' <"/proc/$dashboard_pid/environ")"
	assert_contains "dashboard uses staged manifest root" "DASHBOARD_MANIFEST_ROOT=$MANIFEST_ROOT" "$dashboard_env"
	assert_contains "dashboard inherits staged registry root" "REGISTRY_ROOT=$MANIFEST_ROOT" "$dashboard_env"
else
	echo "FAIL - cannot inspect dashboard process environment"
	fails=$((fails + 1))
fi

services_json="$(curl -fsS http://127.0.0.1:3000/services 2>/dev/null || true)"
for svc in "${MCP_SERVICES[@]}"; do
	assert_contains "/services lists $svc" "\"name\":\"$svc\"" "$services_json"
done

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
