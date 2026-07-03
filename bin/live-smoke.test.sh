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
SERVICE_PORTS=(dashboard:3000 crm:3100 ledger:3101 notify:3201 prompts:3002 wiki:3001 dropbox:3200 cron:3005 gmail:3202 scripts:3003 sites:3004 webhooks:3006 nginx:8080)

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

port_pids() {
	ss -ltnp "sport = :$1" 2>/dev/null | grep -o 'pid=[0-9][0-9]*' | cut -d= -f2 | sort -u
}

port_owners() {
	ss -ltnp "sport = :$1" 2>/dev/null || true
}

pid_cmdline() {
	tr '\0' ' ' <"/proc/$1/cmdline" 2>/dev/null || true
}

pid_from_this_worktree() {
	local pid="$1" cmd
	cmd="$(pid_cmdline "$pid")"
	[[ "$cmd" == "$RUN_DIR/bin/"* ]] || [[ "$cmd" == *" $RUN_DIR/bin/"* ]] || [[ "$cmd" == *"$REPO/nginx"* ]]
}

worktree_pid_for_port() {
	local port="$1" pid
	while read -r pid; do
		[ -n "$pid" ] || continue
		if pid_from_this_worktree "$pid"; then
			echo "$pid"
			return 0
		fi
	done < <(port_pids "$port")
	return 1
}

tracked_stack_running() {
	local pf pid
	for pf in "$RUN_DIR"/*.pid; do
		[ -e "$pf" ] || continue
		pid="$(cat "$pf" 2>/dev/null || true)"
		[ -n "$pid" ] && kill -0 "$pid" 2>/dev/null && return 0
	done
	return 1
}

recover_worktree_pidfiles() {
	local spec name port pid recovered=1

	for spec in "${SERVICE_PORTS[@]}"; do
		name="${spec%%:*}"
		port="${spec##*:}"
		pid="$(worktree_pid_for_port "$port" || true)"
		[ -n "$pid" ] || continue

		mkdir -p "$RUN_DIR"
		echo "$pid" >"$RUN_DIR/$name.pid"
		recovered=0
	done

	return "$recovered"
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

recover_worktree_pidfiles || true
if tracked_stack_running; then
	"$HERE/stop" --clean
fi

for port in "${PORTS[@]}"; do
	if port_open "$port"; then
		echo "FAIL - port :$port is already in use by another worktree; not stopping it"
		port_owners "$port" | sed 's/^/       /'
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
