#!/usr/bin/env bash
#
# ship.test.sh — self-contained tests for bin/ship's build/version contract.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHIP="$HERE/ship"

fails=0
ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT

assert_eq() {
	local label="$1" want="$2" got="$3"
	if [ "$want" = "$got" ]; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: want [$want] got [$got]"
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

mkdir -p "$ROOT/bin" "$ROOT/appkit" "$ROOT/ledger/cmd/ledger" "$ROOT/ledger/etc"
cp "$SHIP" "$ROOT/bin/ship"
chmod +x "$ROOT/bin/ship"

cat >"$ROOT/appkit/go.mod" <<'EOF_APPKIT_MOD'
module appkit

go 1.26
EOF_APPKIT_MOD
cat >"$ROOT/appkit/appkit.go" <<'EOF_APPKIT'
package appkit

var (
	version = "dev"
	commit  = "none"
)

func VersionString() string {
	if commit == "" || commit == "none" {
		return version
	}
	return version + " (" + commit + ")"
}
EOF_APPKIT

cat >"$ROOT/ledger/go.mod" <<'EOF_LEDGER_MOD'
module ledger

go 1.26

require appkit v0.0.0

replace appkit => ../appkit
EOF_LEDGER_MOD
cat >"$ROOT/ledger/cmd/ledger/main.go" <<'EOF_LEDGER'
package main

import (
	"fmt"
	"os"

	"appkit"
)

func main() {
	if len(os.Args) == 2 && os.Args[1] == "version" {
		fmt.Println(appkit.VersionString())
		return
	}
	fmt.Println("ledger")
}
EOF_LEDGER
cat >"$ROOT/ledger/etc/deploy.env" <<'EOF_DEPLOY'
ACCOUNT=int
SSH_USER=ec2-user
SSH_KEY=~/.ssh/id_ed25519_int_ikigenba_com
APEX_SUFFIX=ikigenba.com
EOF_DEPLOY
printf '%s\n' v0.7.1 >"$ROOT/ledger/VERSION"

git -C "$ROOT" init -q
git -C "$ROOT" config user.email t@t.t
git -C "$ROOT" config user.name t
git -C "$ROOT" symbolic-ref HEAD refs/heads/main
git -C "$ROOT" add -A
git -C "$ROOT" commit -qm baseline

SHA="$(git -C "$ROOT" rev-parse --short HEAD)"
FULL="v0.7.1+${SHA}"

# R-45PQ-GAF2
OUT="$(DRY_RUN=1 "$ROOT/bin/ship" ledger 2>&1)"
status=$?
if [ "$status" -ne 0 ]; then
	echo "FAIL - ship dry-run build exited $status"
	printf '%s\n' "$OUT"
	fails=$((fails + 1))
else
	echo "ok   - ship dry-run build succeeds"
fi

ARTIFACT="$(printf '%s\n' "$OUT" | sed -n 's/^>> dry-run complete\. Artifact: //p' | tail -n 1)"
if [ -x "$ARTIFACT" ]; then
	echo "ok   - artifact exists and is executable"
else
	echo "FAIL - missing executable artifact [$ARTIFACT]"
	fails=$((fails + 1))
fi

assert_eq "artifact filename uses full SemVer" "ledger-${FULL}" "$(basename "$ARTIFACT")"
assert_eq "artifact is under libexec" "libexec" "$(basename "$(dirname "$ARTIFACT")")"
if [ -x "$ARTIFACT" ]; then
	assert_eq "binary version output equals full SemVer" "$FULL" "$("$ARTIFACT" version)"
fi
assert_contains "ship output uses full remote artifact" "/tmp/ledger-${FULL}" "$OUT"
assert_contains "ship output uses full stage release" "sudo opsctl stage  ledger ${FULL}" "$OUT"
assert_contains "ship output uses source commit sha" "+${SHA}" "$OUT"

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
