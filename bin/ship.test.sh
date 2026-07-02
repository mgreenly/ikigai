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

assert_not_matching() {
	local label="$1" regex="$2" haystack="$3"
	if grep -Eq "$regex" <<<"$haystack"; then
		echo "FAIL - $label: matched [$regex]"
		fails=$((fails + 1))
	else
		echo "ok   - $label"
	fi
}

assert_file_eq() {
	local label="$1" want="$2" got="$3"
	if cmp -s "$want" "$got"; then
		echo "ok   - $label"
	else
		echo "FAIL - $label: files differ"
		fails=$((fails + 1))
	fi
}

write_service() {
	local svc="$1" version="$2" share="$3"
	mkdir -p "$ROOT/${svc}/cmd/${svc}" "$ROOT/${svc}/etc"
	cat >"$ROOT/${svc}/go.mod" <<EOF_SERVICE_MOD
module ${svc}

go 1.26

require appkit v0.0.0

replace appkit => ../appkit
EOF_SERVICE_MOD
	cat >"$ROOT/${svc}/cmd/${svc}/main.go" <<EOF_SERVICE
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
	fmt.Println("${svc}")
}
EOF_SERVICE
	cat >"$ROOT/${svc}/etc/deploy.env" <<'EOF_DEPLOY'
ACCOUNT=int
SSH_USER=ec2-user
SSH_KEY=~/.ssh/id_ed25519_int_ikigenba_com
APEX_SUFFIX=ikigenba.com
EOF_DEPLOY
	cat >"$ROOT/${svc}/etc/nginx.conf" <<EOF_NGINX
server {
    server_name ${svc}.example.test;
    proxy_set_header X-Service ${svc};
}
EOF_NGINX
	cat >"$ROOT/${svc}/etc/manifest.env" <<EOF_MANIFEST
SERVICE=${svc}
PORT=9${#svc}01
EOF_MANIFEST
	printf '%s\n' "$version" >"$ROOT/${svc}/VERSION"
	if [ "$share" = "yes" ]; then
		mkdir -p "$ROOT/${svc}/share/nested"
		printf '%s\n' "${svc} static asset" >"$ROOT/${svc}/share/public.txt"
		printf '%s\n' "${svc} nested asset" >"$ROOT/${svc}/share/nested/info.txt"
	fi
}

run_ship() {
	local svc="$1" out_var="$2" bundle_var="$3"
	local output status bundle
	output="$(DRY_RUN=1 "$ROOT/bin/ship" "$svc" 2>&1)"
	status=$?
	if [ "$status" -ne 0 ]; then
		echo "FAIL - ${svc} ship dry-run build exited $status"
		printf '%s\n' "$output"
		fails=$((fails + 1))
	else
		echo "ok   - ${svc} ship dry-run build succeeds"
	fi
	bundle="$(printf '%s\n' "$output" | sed -n 's/^>> dry-run complete\. Artifact: //p' | tail -n 1)"
	printf -v "$out_var" '%s' "$output"
	printf -v "$bundle_var" '%s' "$bundle"
}

verify_bundle_contract() {
	local svc="$1" bundle="$2" expect_share="$3"
	local extract listing
	extract="$(mktemp -d "$ROOT/extract-${svc}.XXXXXX")"
	listing="$(tar -tzf "$bundle")"

	assert_contains "${svc} bundle contains bare executable" "$svc" "$listing"
	assert_contains "${svc} bundle contains nginx.conf" "nginx.conf" "$listing"
	assert_contains "${svc} bundle contains manifest.env" "manifest.env" "$listing"
	tar -xzf "$bundle" -C "$extract"

	if [ -x "$extract/$svc" ]; then
		echo "ok   - ${svc} bundle executable bit preserved"
	else
		echo "FAIL - ${svc} bundle missing executable [$svc]"
		fails=$((fails + 1))
	fi
	assert_file_eq "${svc} nginx.conf copied byte-for-byte" "$ROOT/${svc}/etc/nginx.conf" "$extract/nginx.conf"
	assert_file_eq "${svc} manifest.env copied byte-for-byte" "$ROOT/${svc}/etc/manifest.env" "$extract/manifest.env"

	if [ "$expect_share" = "yes" ]; then
		assert_contains "${svc} bundle contains share/public.txt" "share/public.txt" "$listing"
		assert_contains "${svc} bundle contains share/nested/info.txt" "share/nested/info.txt" "$listing"
		assert_file_eq "${svc} share/public.txt copied byte-for-byte" "$ROOT/${svc}/share/public.txt" "$extract/share/public.txt"
		assert_file_eq "${svc} share/nested/info.txt copied byte-for-byte" "$ROOT/${svc}/share/nested/info.txt" "$extract/share/nested/info.txt"
	else
		assert_not_matching "${svc} bundle omits share entries" '(^|/)share(/|$)' "$listing"
	fi
}

mkdir -p "$ROOT/bin" "$ROOT/appkit"
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

write_service ledger v0.7.1 yes
write_service notes v0.7.1 no

git -C "$ROOT" init -q
git -C "$ROOT" config user.email t@t.t
git -C "$ROOT" config user.name t
git -C "$ROOT" symbolic-ref HEAD refs/heads/main
git -C "$ROOT" add -A
git -C "$ROOT" commit -qm baseline

SHA="$(git -C "$ROOT" rev-parse --short HEAD)"
FULL="v0.7.1+${SHA}"

# R-45PQ-GAF2
run_ship ledger LEDGER_OUT LEDGER_BUNDLE
if [ -f "$LEDGER_BUNDLE" ]; then
	echo "ok   - bundle exists"
else
	echo "FAIL - missing bundle [$LEDGER_BUNDLE]"
	fails=$((fails + 1))
fi

assert_eq "bundle filename uses full SemVer" "ledger-${FULL}.tar.gz" "$(basename "$LEDGER_BUNDLE")"
if [ -f "$LEDGER_BUNDLE" ]; then
	LEDGER_EXTRACT="$(mktemp -d "$ROOT/r45-ledger.XXXXXX")"
	tar -xzf "$LEDGER_BUNDLE" -C "$LEDGER_EXTRACT" ledger
	if [ -x "$LEDGER_EXTRACT/ledger" ]; then
		echo "ok   - extracted bundle binary is executable"
		assert_eq "bundle binary version output equals full SemVer" "$FULL" "$("$LEDGER_EXTRACT/ledger" version)"
	else
		echo "FAIL - extracted bundle binary is not executable"
		fails=$((fails + 1))
	fi
fi
assert_contains "ship output uses full remote bundle" "/tmp/ledger-${FULL}.tar.gz" "$LEDGER_OUT"
assert_contains "ship output uses full stage release" "sudo opsctl stage  ledger ${FULL} --artifact /tmp/ledger-${FULL}.tar.gz" "$LEDGER_OUT"
assert_contains "ship output uses source commit sha" "+${SHA}" "$LEDGER_OUT"

# R-P4CO-FY2L
verify_bundle_contract ledger "$LEDGER_BUNDLE" yes
run_ship notes NOTES_OUT NOTES_BUNDLE
assert_eq "no-share bundle filename uses full SemVer" "notes-${FULL}.tar.gz" "$(basename "$NOTES_BUNDLE")"
verify_bundle_contract notes "$NOTES_BUNDLE" no
assert_contains "no-share ship output uses full remote bundle" "/tmp/notes-${FULL}.tar.gz" "$NOTES_OUT"

echo
if [ "$fails" -eq 0 ]; then
	echo "PASS"
	exit 0
else
	echo "FAIL ($fails failing assertions)"
	exit 1
fi
