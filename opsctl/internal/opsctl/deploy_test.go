package opsctl

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

// newOpsctl builds an Opsctl over a temp root with the stub system and a fake
// runner carrying the given FAKE_* scenario env.
func newOpsctl(t *testing.T, root, app string, sys *stubSystem, base []string) *Opsctl {
	t.Helper()
	sys.app = app
	return &Opsctl{
		Root:   root,
		Keep:   3,
		System: sys,
		Runner: fakeRunner{baseEnv: base},
		Out:    &strings.Builder{},
		Err:    &strings.Builder{},
	}
}

func fakeEnv(app, version string, embedded int, manifest string) []string {
	env := []string{
		"FAKE_APP=" + app,
		"FAKE_VERSION=" + version,
		"FAKE_EMBEDDED=" + itoa(embedded),
	}
	if manifest != "" {
		env = append(env, "FAKE_MANIFEST="+manifest)
	}
	return env
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	var b []byte
	for n > 0 {
		b = append([]byte{byte('0' + n%10)}, b...)
		n /= 10
	}
	if neg {
		b = append([]byte{'-'}, b...)
	}
	return string(b)
}

// stageAndDeploy runs the two-verb cutover the old monolithic Install did in one
// shot: stage the artifact into libexec/<app>-<version> then deploy it. The
// acceptance tests below drive the full lifecycle through this helper so they
// exercise both verbs exactly as an operator would.
func stageAndDeploy(t *testing.T, o *Opsctl, app, version, artifact string) error {
	t.Helper()
	bundle := bundleArtifactFromBinary(t, app, version, filepath.Base(artifact), artifact)
	if err := o.Stage(context.Background(), app, version, bundle, false); err != nil {
		return err
	}
	return o.Deploy(context.Background(), app, version)
}

// readRunVersion resolves bin/run → its deployed version.
func readRunVersion(t *testing.T, l Layout) string {
	t.Helper()
	v, err := (&Opsctl{}).currentVersion(l)
	if err != nil {
		t.Fatalf("read live version: %v", err)
	}
	return v
}

// dbApplied reads the fake "DB" file (a single integer = applied schema version).
func dbApplied(t *testing.T, l Layout) (int, bool) {
	t.Helper()
	b, err := os.ReadFile(l.DBPath())
	if err != nil {
		return 0, false
	}
	n := 0
	for _, c := range strings.TrimSpace(string(b)) {
		n = n*10 + int(c-'0')
	}
	return n, true
}

// resolveThroughStablePaths asserts the launcher-facing stable paths are valid:
// bin/run resolves to an existing binary, and etc/manifest.env
// exists and names the app. This must hold after every install/rollback (PLAN
// §2.6 — load-bearing at all times, including mid-swap).
func resolveThroughStablePaths(t *testing.T, l Layout) {
	t.Helper()
	// bin/run -> ../libexec/<app>-<version>; resolving it must reach a real file.
	runResolved, err := filepath.EvalSymlinks(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run does not resolve: %v", err)
	}
	if fi, err := os.Stat(runResolved); err != nil || fi.IsDir() {
		t.Fatalf("bin/run target %s is not a runnable file: %v", runResolved, err)
	}
	man, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("manifest.env missing: %v", err)
	}
	if !strings.Contains(string(man), "APP="+l.App) {
		t.Fatalf("manifest.env does not name app: %q", string(man))
	}
}

// TestInstallInstallRollback is the C2 acceptance core: a full install, then a
// second install, then a rollback, against a temp OPSCTL_ROOT — asserting the
// atomic repoint, the stable paths staying valid throughout, that a no-schema-
// change deploy never modifies the DB, and (in the schema-advance variant below)
// that the backup/restore wires together.
func TestInstallInstallRollback_NoSchemaChange(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	// First install: v1.0.0, embedded schema 2 (creates the DB at applied=2).
	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 2, ""))
	art1 := stageArtifact(t, "ledger-v1.0.0")
	if err := stageAndDeploy(t, o, app, "v1.0.0", art1); err != nil {
		t.Fatalf("deploy v1.0.0: %v", err)
	}
	if got := readRunVersion(t, l); got != "v1.0.0" {
		t.Fatalf("live version = %q, want v1.0.0", got)
	}
	resolveThroughStablePaths(t, l)
	applied1, ok := dbApplied(t, l)
	if !ok || applied1 != 2 {
		t.Fatalf("after v1.0.0 install, applied = %d (ok=%v), want 2", applied1, ok)
	}
	dbInfo1, _ := os.Stat(l.DBPath())

	// Second install: v1.1.0, SAME embedded schema 2 → no schema advance → the DB
	// must NOT be modified and NO backup taken.
	o2 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.1.0", 2, ""))
	art2 := stageArtifact(t, "ledger-v1.1.0")
	if err := stageAndDeploy(t, o2, app, "v1.1.0", art2); err != nil {
		t.Fatalf("deploy v1.1.0: %v", err)
	}
	if got := readRunVersion(t, l); got != "v1.1.0" {
		t.Fatalf("live version = %q, want v1.1.0", got)
	}
	resolveThroughStablePaths(t, l)

	// DB untouched by a no-schema-change deploy (mtime + content unchanged).
	dbInfo2, _ := os.Stat(l.DBPath())
	if !dbInfo1.ModTime().Equal(dbInfo2.ModTime()) {
		t.Errorf("no-schema-change install modified the DB mtime: %v -> %v", dbInfo1.ModTime(), dbInfo2.ModTime())
	}
	if _, err := os.Stat(l.PreMigrationBackup("v1.1.0")); err == nil {
		t.Errorf("no-schema-change install took a pre-migration backup, want none")
	}

	// Rollback to the prior release (v1.0.0). No schema advance ⇒ no DB restore.
	if err := o2.Rollback(context.Background(), app, ""); err != nil {
		t.Fatalf("rollback: %v", err)
	}
	if got := readRunVersion(t, l); got != "v1.0.0" {
		t.Fatalf("after rollback live version = %q, want v1.0.0", got)
	}
	resolveThroughStablePaths(t, l)

	if sys.restarts != 3 {
		t.Fatalf("restart count = %d, want 3", sys.restarts)
	}
}

// TestSchemaAdvance_BackupAndRollbackRestores proves the schema-aware path: a
// schema-advancing install takes a pre-migration backup and migrates the DB
// forward; a subsequent rollback restores the backup so the older binary's
// downgrade guard would accept the DB. It also proves the rollback re-mints the
// event-plane epoch: driving restore through the target binary removes the
// <db>.generation sidecar end to end.
func TestSchemaAdvance_BackupAndRollbackRestores(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	// v1.0.0 — embedded schema 2, fresh DB → applied=2.
	o1 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 2, ""))
	if err := stageAndDeploy(t, o1, app, "v1.0.0", stageArtifact(t, "v1")); err != nil {
		t.Fatalf("deploy v1.0.0: %v", err)
	}
	if applied, _ := dbApplied(t, l); applied != 2 {
		t.Fatalf("applied after v1.0.0 = %d, want 2", applied)
	}

	// v2.0.0 — embedded schema 5 → schema ADVANCES (2 → 5). Install must back up the
	// DB (named pre-v2.0.0.db) BEFORE migrating it forward to 5.
	o2 := newOpsctl(t, root, app, sys, fakeEnv(app, "v2.0.0", 5, ""))
	if err := stageAndDeploy(t, o2, app, "v2.0.0", stageArtifact(t, "v2")); err != nil {
		t.Fatalf("deploy v2.0.0: %v", err)
	}
	backup := l.PreMigrationBackup("v2.0.0")
	if _, err := os.Stat(backup); err != nil {
		t.Fatalf("schema-advancing install took NO pre-migration backup: %v", err)
	}
	// The backup must capture the PRE-migration state (applied=2), not the migrated 5.
	bb, _ := os.ReadFile(backup)
	if strings.TrimSpace(string(bb)) != "2" {
		t.Fatalf("pre-migration backup holds applied=%q, want 2 (the pre-migrate state)", strings.TrimSpace(string(bb)))
	}
	// The live DB advanced to 5.
	if applied, _ := dbApplied(t, l); applied != 5 {
		t.Fatalf("applied after v2.0.0 = %d, want 5 (migrated forward)", applied)
	}

	// A live producer carries an event-plane generation sidecar next to the DB.
	// Rolling back rewinds the DB, so the sidecar must be re-minted (removed) by
	// driving restore through the target binary — prove the opsctl→binary→restore
	// wiring does this end to end.
	gen := l.GenerationPath()
	if err := os.WriteFile(gen, []byte("epoch-before-rollback\n"), 0o644); err != nil {
		t.Fatalf("seed generation sidecar: %v", err)
	}

	// Rollback to v1.0.0 — because v2.0.0 advanced the schema (its pre-migration
	// backup exists), rollback must RESTORE the DB to applied=2 first, then swap.
	if err := o2.Rollback(context.Background(), app, ""); err != nil {
		t.Fatalf("rollback: %v", err)
	}
	if got := readRunVersion(t, l); got != "v1.0.0" {
		t.Fatalf("after rollback live version = %q, want v1.0.0", got)
	}
	if applied, _ := dbApplied(t, l); applied != 2 {
		t.Fatalf("after rollback applied = %d, want 2 (DB restored from pre-migration backup)", applied)
	}
	// The sidecar must be gone: the restore re-minted the epoch.
	if _, err := os.Stat(gen); !os.IsNotExist(err) {
		t.Fatalf("generation sidecar still present after rollback (stat err = %v); epoch not re-minted", err)
	}
	resolveThroughStablePaths(t, l)
}

// TestPreflight_Rejections asserts install refuses a bad artifact and leaves the
// live release untouched.
func TestPreflight_Rejections(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	sys := &stubSystem{}

	// Version mismatch: artifact self-reports v9.9.9 but we stage it as v1.0.0.
	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v9.9.9", 1, ""))
	mismatch := stageBundleArtifact(t, app, "v1.0.0", "mismatch")
	err := o.Stage(context.Background(), app, "v1.0.0", mismatch, false)
	if err == nil || !strings.Contains(err.Error(), "self-reports version") {
		t.Fatalf("version-mismatch stage err = %v, want a version-mismatch refusal", err)
	}
	if sys.restarts != 0 {
		t.Errorf("preflight failure still restarted the unit (%d times)", sys.restarts)
	}
	// Preflight refusal keeps the /tmp artifact for retry (decision 2).
	if _, err := os.Stat(mismatch); err != nil {
		t.Errorf("preflight refusal removed the /tmp artifact, want it kept: %v", err)
	}

	// Not a static ELF: a text file as the bundled libexec binary.
	bad := filepath.Join(t.TempDir(), "not-elf")
	if err := os.WriteFile(bad, []byte("#!/bin/sh\necho hi\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	badBundle := bundleArtifactFromBinary(t, app, "v1.0.0", "not-elf", bad)
	o2 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	if err := o2.Stage(context.Background(), app, "v1.0.0", badBundle, false); err == nil || !strings.Contains(err.Error(), "ELF") {
		t.Fatalf("non-ELF stage err = %v, want an ELF refusal", err)
	}
}

// TestInstall_ConvertsLegacyBinRunFile asserts the conversion case the D2 box
// prototype hit: when /opt/<app>/bin/run already exists as a REGULAR FILE (the
// old pre-redesign layout's wrapper script), install must replace it with the
// stable versioned libexec symlink rather than failing with EEXIST.
func TestInstall_ConvertsLegacyBinRunFile(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	// Seed the OLD layout: bin/run is a regular file (the legacy wrapper script).
	if err := os.MkdirAll(l.BinDir(), 0o755); err != nil {
		t.Fatalf("mkdir bin: %v", err)
	}
	if err := os.WriteFile(l.RunLink(), []byte("#!/bin/sh\nexec ./ledger.bin\n"), 0o755); err != nil {
		t.Fatalf("seed legacy bin/run: %v", err)
	}

	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v0.1.0", 3, ""))
	if err := stageAndDeploy(t, o, app, "v0.1.0", stageArtifact(t, "ledger-v0.1.0")); err != nil {
		t.Fatalf("deploy over legacy bin/run: %v", err)
	}
	// bin/run is now a symlink at ../current/<app> and resolves to a real binary.
	got, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink after conversion: %v", err)
	}
	if want := l.runTarget("v0.1.0"); got != want {
		t.Fatalf("bin/run -> %q, want %q", got, want)
	}
	resolveThroughStablePaths(t, l)
}

func TestDeploy_SwapsBinRunToVersionedLibexecBinary(t *testing.T) {
	// R-3TIQ-ML04
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	o1 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	if err := stageAndDeploy(t, o1, app, "v1.0.0", stageArtifact(t, "ledger-v1.0.0")); err != nil {
		t.Fatalf("deploy v1.0.0: %v", err)
	}
	o2 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.1.0", 1, ""))
	if err := stageAndDeploy(t, o2, app, "v1.1.0", stageArtifact(t, "ledger-v1.1.0")); err != nil {
		t.Fatalf("deploy v1.1.0: %v", err)
	}

	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("read bin/run: %v", err)
	}
	if want := l.runTarget("v1.1.0"); target != want {
		t.Fatalf("bin/run target = %q, want %q", target, want)
	}
	resolved, err := filepath.EvalSymlinks(l.RunLink())
	if err != nil {
		t.Fatalf("resolve bin/run: %v", err)
	}
	if resolved != l.LibexecBinary("v1.1.0") {
		t.Fatalf("bin/run resolves to %q, want %q", resolved, l.LibexecBinary("v1.1.0"))
	}
	if _, err := os.Stat(l.LibexecBinary("v1.0.0")); err != nil {
		t.Fatalf("previous libexec binary missing after deploy: %v", err)
	}
}

// TestStampDataPaths asserts the regenerated manifest gains the absolute on-box
// state paths the serving process needs (the D2 box prototype hit the serving
// binary falling back to appkit's relative ./tmp/<app>.db default because nothing
// injected <APP>_DB_PATH into ikigenba-launch's exported env).
func TestStampDataPaths(t *testing.T) {
	l := NewLayout("/opt", "ledger")
	portable := "APP=ledger\nMOUNT=/srv/ledger/\nPORT=3002\nMCP=true\n"
	got := stampDataPaths(portable, l)

	for _, want := range []string{
		"LEDGER_DB_PATH=/opt/ledger/state/ledger.db",
		"LEDGER_GENERATION_PATH=/opt/ledger/cache/ledger.db.generation",
		"APP=ledger", "PORT=3002",
	} {
		if !strings.Contains(got, want) {
			t.Errorf("stamped manifest missing %q\n--- got ---\n%s", want, got)
		}
	}
	if !strings.HasSuffix(got, "\n") || strings.HasSuffix(got, "\n\n") {
		t.Errorf("stamped manifest must end with exactly one newline, got %q", got)
	}

	// Idempotent + binary-wins: a manifest that already assigns the key keeps its
	// value and gains no duplicate.
	withKey := "APP=ledger\nLEDGER_DB_PATH=/custom/path.db\n"
	got2 := stampDataPaths(withKey, l)
	if strings.Count(got2, "LEDGER_DB_PATH=") != 1 {
		t.Errorf("expected exactly one LEDGER_DB_PATH assignment, got:\n%s", got2)
	}
	if !strings.Contains(got2, "LEDGER_DB_PATH=/custom/path.db") {
		t.Errorf("binary's own LEDGER_DB_PATH should win, got:\n%s", got2)
	}
}

// TestInstall_ChownsStateDirToAppUser asserts install hands the state tree back to
// the `<app>` service user after the root-run migrate (the cutover-reset bug:
// migrate, run as root, creates a fresh DB owned root:root, which the unit's
// dedicated <app> user cannot write — crash-loop). The chown must request the
// bare app name as BOTH owner and group (matching setup's EnsureSystemUser) and
// target the state dir, on every install. The stub records (never executes) the
// op, so no real system path is chowned under the temp OPSCTL_ROOT.
func TestInstall_ChownsStateDirToAppUser(t *testing.T) {
	root := t.TempDir()
	app := "crm"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 2, ""))
	if err := stageAndDeploy(t, o, app, "v1.0.0", stageArtifact(t, "crm-v1.0.0")); err != nil {
		t.Fatalf("deploy: %v", err)
	}

	want := "chown:" + app + ":" + app + ":" + l.StateDir()
	var found bool
	for _, op := range sys.opSeq() {
		if op == want {
			found = true
			break
		}
	}
	if !found {
		t.Fatalf("install did not request the data-dir chown; want %q in ops %v", want, sys.opSeq())
	}
}

// TestInstall_IsActiveFailure asserts a failed is-active surfaces an error that
// points the operator at rollback (the release dir + backup are left intact).
func TestInstall_IsActiveFailure(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	sys := &stubSystem{failIsActive: true}
	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	err := stageAndDeploy(t, o, app, "v1.0.0", stageArtifact(t, "v1"))
	if err == nil || !strings.Contains(err.Error(), "did not come up") {
		t.Fatalf("is-active failure err = %v, want a 'did not come up' error", err)
	}
}

// R-4LKF-FB23
func TestNotifySetupDeployBootsHealthWithStateAndCachePaths(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "notify"
	const version = "v1.2.3"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	sys := &notifyBootSystem{
		stubSystem: &stubSystem{},
		t:          t,
		layout:     l,
		port:       freePort(t),
	}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  RealRunner{},
		Out:     io.Discard,
		Err:     io.Discard,
	}

	fragment := "location /srv/notify/ {\n    proxy_pass http://127.0.0.1:__PORT__;\n}\n"
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3003, Fragment: fragment}); err != nil {
		t.Fatalf("setup notify: %v", err)
	}

	artifact := buildNotifyArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage notify: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy notify: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("notify DB was not created under state/: %v", err)
	}
	if got := filepath.Dir(l.GenerationPath()); got != l.CacheDir() {
		t.Fatalf("generation sidecar parent = %q, want cache dir %q", got, l.CacheDir())
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("notify binary missing under libexec/: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink: %v", err)
	}
	if want := l.runTarget(version); target != want {
		t.Fatalf("bin/run -> %q, want %q", target, want)
	}

	manifest, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	for _, want := range []string{
		"APP=notify",
		"NOTIFY_DB_PATH=" + l.DBPath(),
		"NOTIFY_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"notify"`) {
		t.Fatalf("health response = %s, want notify ok envelope", sys.healthBody)
	}
}

// R-4LKF-FB23
func TestDropboxSetupDeployBootsHealthWithStateCacheAndMirrorPaths(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "dropbox"
	const version = "v0.8.0"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	sys := &dropboxBootSystem{
		stubSystem: &stubSystem{},
		t:          t,
		layout:     l,
		port:       freePort(t),
	}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  RealRunner{},
		Out:     io.Discard,
		Err:     io.Discard,
	}

	fragment := "location /srv/dropbox/ {\n    proxy_pass http://127.0.0.1:__PORT__;\n}\n"
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3005, Fragment: fragment}); err != nil {
		t.Fatalf("setup dropbox: %v", err)
	}

	artifact := buildDropboxArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage dropbox: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy dropbox: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("dropbox DB was not created under state/: %v", err)
	}
	if got := filepath.Dir(l.GenerationPath()); got != l.CacheDir() {
		t.Fatalf("generation sidecar parent = %q, want cache dir %q", got, l.CacheDir())
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("dropbox binary missing under libexec/: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink: %v", err)
	}
	if want := l.runTarget(version); target != want {
		t.Fatalf("bin/run -> %q, want %q", target, want)
	}

	mirrorDir := filepath.Join(l.StateDir(), "mirror")
	if fi, err := os.Stat(mirrorDir); err != nil || !fi.IsDir() {
		t.Fatalf("dropbox mirror was not created under state/: %v", err)
	}
	for _, forbidden := range []string{
		filepath.Join(l.AppDir(), "data", "mirror"),
		filepath.Join(l.CacheDir(), "mirror"),
		filepath.Join(l.AppDir(), "tmp", "mirror"),
	} {
		if _, err := os.Stat(forbidden); err == nil {
			t.Fatalf("dropbox mirror unexpectedly created at rebuildable/legacy path %s", forbidden)
		} else if !os.IsNotExist(err) {
			t.Fatalf("stat forbidden mirror path %s: %v", forbidden, err)
		}
	}

	manifest, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	for _, want := range []string{
		"APP=dropbox",
		"DROPBOX_DB_PATH=" + l.DBPath(),
		"DROPBOX_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"dropbox"`) {
		t.Fatalf("health response = %s, want dropbox ok envelope", sys.healthBody)
	}
}

// R-4LKF-FB23
func TestPromptsSetupDeployBootsHealthWithStateSandboxesAndCacheRuns(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "prompts"
	const version = "v0.12.0"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	staleRunsDir := filepath.Join(l.CacheDir(), "runs")
	if err := os.MkdirAll(filepath.Join(staleRunsDir, "stale-run"), 0o755); err != nil {
		t.Fatalf("create stale runs dir: %v", err)
	}

	sys := &promptsBootSystem{
		stubSystem: &stubSystem{},
		t:          t,
		layout:     l,
		port:       freePort(t),
	}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  RealRunner{},
		Out:     io.Discard,
		Err:     io.Discard,
	}

	fragment := "location /srv/prompts/ {\n    proxy_pass http://127.0.0.1:__PORT__;\n}\n"
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3004, Fragment: fragment}); err != nil {
		t.Fatalf("setup prompts: %v", err)
	}

	artifact := buildPromptsArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage prompts: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy prompts: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("prompts DB was not created under state/: %v", err)
	}
	sandboxesDir := filepath.Join(l.StateDir(), "sandboxes")
	if fi, err := os.Stat(sandboxesDir); err != nil || !fi.IsDir() {
		t.Fatalf("prompts sandboxes dir was not created under state/: %v", err)
	}
	if got := filepath.Dir(l.GenerationPath()); got != l.CacheDir() {
		t.Fatalf("generation sidecar parent = %q, want cache dir %q", got, l.CacheDir())
	}
	if _, err := os.Stat(l.GenerationPath()); err != nil {
		t.Fatalf("prompts generation sidecar missing under cache/: %v", err)
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("prompts binary missing under libexec/: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink: %v", err)
	}
	if want := l.runTarget(version); target != want {
		t.Fatalf("bin/run -> %q, want %q", target, want)
	}
	for _, retired := range []string{
		filepath.Join(l.BinDir(), "backup"),
		filepath.Join(l.BinDir(), "restore"),
	} {
		if _, err := os.Stat(retired); err == nil {
			t.Fatalf("retired per-service script still exists: %s", retired)
		} else if !os.IsNotExist(err) {
			t.Fatalf("stat retired per-service script %s: %v", retired, err)
		}
	}

	if fi, err := os.Stat(staleRunsDir); err != nil || !fi.IsDir() {
		t.Fatalf("prompts runs dir was not recreated under cache/: %v", err)
	}
	if _, err := os.Stat(filepath.Join(staleRunsDir, "stale-run")); !os.IsNotExist(err) {
		t.Fatalf("stale non-state run survived boot recreation: %v", err)
	}
	forbidden := filepath.Join(l.StateDir(), "runs")
	if _, err := os.Stat(forbidden); !os.IsNotExist(err) {
		t.Fatalf("prompts runs unexpectedly created under durable state: %v", err)
	}

	manifest, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	for _, want := range []string{
		"APP=prompts",
		"PROMPTS_DB_PATH=" + l.DBPath(),
		"PROMPTS_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"prompts"`) {
		t.Fatalf("health response = %s, want prompts ok envelope", sys.healthBody)
	}
}

// R-4LKF-FB23
func TestWikiSetupDeployBootsHealthWithStateAndCachePaths(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "wiki"
	const version = "v0.4.0"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	sys := &wikiBootSystem{
		stubSystem: &stubSystem{},
		t:          t,
		layout:     l,
		port:       freePort(t),
	}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  RealRunner{},
		Out:     io.Discard,
		Err:     io.Discard,
	}

	fragment := "location /srv/wiki/ {\n    proxy_pass http://127.0.0.1:__PORT__;\n}\n"
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3006, Fragment: fragment}); err != nil {
		t.Fatalf("setup wiki: %v", err)
	}

	artifact := buildWikiArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage wiki: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy wiki: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("wiki DB was not created under state/: %v", err)
	}
	if got := filepath.Dir(l.GenerationPath()); got != l.CacheDir() {
		t.Fatalf("generation sidecar parent = %q, want cache dir %q", got, l.CacheDir())
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("wiki binary missing under libexec/: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink: %v", err)
	}
	if want := l.runTarget(version); target != want {
		t.Fatalf("bin/run -> %q, want %q", target, want)
	}
	for _, forbidden := range []string{
		filepath.Join(l.StateDir(), "index"),
		filepath.Join(l.StateDir(), "rag"),
		filepath.Join(l.StateDir(), "vectors"),
		filepath.Join(l.StateDir(), "wiki.db.generation"),
	} {
		if _, err := os.Stat(forbidden); err == nil {
			t.Fatalf("wiki rebuildable artifact unexpectedly created under durable state: %s", forbidden)
		} else if !os.IsNotExist(err) {
			t.Fatalf("stat forbidden wiki state path %s: %v", forbidden, err)
		}
	}

	manifest, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	for _, want := range []string{
		"APP=wiki",
		"WIKI_DB_PATH=" + l.DBPath(),
		"WIKI_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"wiki"`) {
		t.Fatalf("health response = %s, want wiki ok envelope", sys.healthBody)
	}
}

// R-4LKF-FB23
func TestCronSetupDeployBootsHealthWithStateAndCachePaths(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "cron"
	const version = "v0.5.1"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	sys := &cronBootSystem{
		stubSystem: &stubSystem{},
		t:          t,
		layout:     l,
		port:       freePort(t),
	}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  RealRunner{},
		Out:     io.Discard,
		Err:     io.Discard,
	}

	fragment := "location /srv/cron/ {\n    proxy_pass http://127.0.0.1:__PORT__;\n}\n"
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3007, Fragment: fragment}); err != nil {
		t.Fatalf("setup cron: %v", err)
	}

	artifact := buildCronArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage cron: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy cron: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("cron DB was not created under state/: %v", err)
	}
	if got := filepath.Dir(l.DBPath()); got != l.StateDir() {
		t.Fatalf("DB parent = %q, want state dir %q", got, l.StateDir())
	}
	if got := filepath.Dir(l.GenerationPath()); got != l.CacheDir() {
		t.Fatalf("generation sidecar parent = %q, want cache dir %q", got, l.CacheDir())
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("cron binary missing under libexec/: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink: %v", err)
	}
	if want := l.runTarget(version); target != want {
		t.Fatalf("bin/run -> %q, want %q", target, want)
	}

	manifest, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	for _, want := range []string{
		"APP=cron",
		"CRON_DB_PATH=" + l.DBPath(),
		"CRON_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"cron"`) {
		t.Fatalf("health response = %s, want cron ok envelope", sys.healthBody)
	}
}

// R-4LKF-FB23
func TestGmailSetupDeployBootsHealthWithStateAndCachePaths(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "gmail"
	const version = "v0.5.0"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	sys := &gmailBootSystem{
		stubSystem: &stubSystem{},
		t:          t,
		layout:     l,
		port:       freePort(t),
	}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  RealRunner{},
		Out:     io.Discard,
		Err:     io.Discard,
	}

	fragment := "location /srv/gmail/ {\n    proxy_pass http://127.0.0.1:__PORT__;\n}\n"
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3008, Fragment: fragment}); err != nil {
		t.Fatalf("setup gmail: %v", err)
	}

	artifact := buildGmailArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage gmail: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy gmail: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("gmail DB was not created under state/: %v", err)
	}
	if got := filepath.Dir(l.DBPath()); got != l.StateDir() {
		t.Fatalf("DB parent = %q, want state dir %q", got, l.StateDir())
	}
	if got := filepath.Dir(l.GenerationPath()); got != l.CacheDir() {
		t.Fatalf("generation sidecar parent = %q, want cache dir %q", got, l.CacheDir())
	}
	generation, err := os.ReadFile(l.GenerationPath())
	if err != nil {
		t.Fatalf("gmail generation sidecar missing under cache/: %v", err)
	}
	if strings.TrimSpace(string(generation)) == "" {
		t.Fatalf("gmail generation sidecar is empty")
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("gmail binary missing under libexec/: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink: %v", err)
	}
	if want := l.runTarget(version); target != want {
		t.Fatalf("bin/run -> %q, want %q", target, want)
	}

	manifest, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	for _, want := range []string{
		"APP=gmail",
		"GMAIL_DB_PATH=" + l.DBPath(),
		"GMAIL_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"gmail"`) {
		t.Fatalf("health response = %s, want gmail ok envelope", sys.healthBody)
	}
}

// R-4LKF-FB23
func TestSitesSetupDeployBootsHealthWithStateWWWPaths(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "sites"
	const version = "v0.6.0"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	sys := &sitesBootSystem{
		stubSystem: &stubSystem{},
		t:          t,
		layout:     l,
		port:       freePort(t),
	}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  RealRunner{},
		Out:     io.Discard,
		Err:     io.Discard,
	}

	if err := o.Setup(context.Background(), SetupOptions{
		App:  app,
		Port: 3010,
		Fragment: readRepoFile(t,
			"../../../sites/etc/nginx.conf"),
		WWWDirs: WWWDirsFor(root, app),
	}); err != nil {
		t.Fatalf("setup sites: %v", err)
	}
	for _, dir := range []string{
		l.WWWRoot(),
		l.WWWWorkingDir(),
		l.WWWPublicDir(),
		l.WWWPrivateDir(),
	} {
		fi, err := os.Stat(dir)
		if err != nil || !fi.IsDir() {
			t.Fatalf("sites www dir %s was not created: %v", dir, err)
		}
		if fi.Mode().Perm() != 0o755 {
			t.Fatalf("sites www dir %s mode = %o, want 0755", dir, fi.Mode().Perm())
		}
	}

	artifact := buildSitesArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage sites: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy sites: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("sites DB was not created under state/: %v", err)
	}
	if got := l.DBPath(); got != filepath.Join(l.StateDir(), "sites.db") {
		t.Fatalf("sites DB path = %q, want state/sites.db", got)
	}
	if got := l.GenerationPath(); got != filepath.Join(l.CacheDir(), "sites.db.generation") {
		t.Fatalf("sites generation sidecar path = %q, want cache/sites.db.generation", got)
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("sites binary missing under libexec/: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("bin/run is not a symlink: %v", err)
	}
	if want := l.runTarget(version); target != want {
		t.Fatalf("bin/run -> %q, want %q", target, want)
	}
	resolved, err := filepath.EvalSymlinks(l.RunLink())
	if err != nil {
		t.Fatalf("resolve bin/run: %v", err)
	}
	if resolved != l.LibexecBinary(version) {
		t.Fatalf("bin/run resolves to %q, want %q", resolved, l.LibexecBinary(version))
	}

	manifest, err := os.ReadFile(l.ManifestPath())
	if err != nil {
		t.Fatalf("read manifest: %v", err)
	}
	for _, want := range []string{
		"APP=sites",
		"SITES_DB_PATH=" + l.DBPath(),
		"SITES_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"sites"`) {
		t.Fatalf("health response = %s, want sites ok envelope", sys.healthBody)
	}
}

type notifyBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *notifyBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("NOTIFY_PORT=%d", s.port),
		"NOTIFY_IP=127.0.0.1",
		"NTFY_TOPIC=test-topic",
		"NTFY_API_KEY=test-token",
		"NOTIFY_NTFY_BASE_URL=http://127.0.0.1:1",
		"CRM_FEED_URL=http://127.0.0.1:1/feed",
		"PROMPTS_FEED_URL=http://127.0.0.1:1/feed",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start notify serve: %w", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	s.started = true
	s.t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			if cmd.Process != nil {
				_ = cmd.Process.Kill()
			}
			<-done
		}
	})

	waitURL := fmt.Sprintf("http://127.0.0.1:%d/health", s.port)
	deadline := time.After(10 * time.Second)
	tick := time.NewTicker(10 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-ctx.Done():
			cancel()
			return ctx.Err()
		case err := <-done:
			cancel()
			return fmt.Errorf("notify serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("notify did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
		case <-tick.C:
			resp, err := http.Get(waitURL)
			if err != nil {
				continue
			}
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil && resp.StatusCode == http.StatusOK {
				s.healthBody = string(body)
				return nil
			}
		}
	}
}

type dropboxBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *dropboxBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("DROPBOX_PORT=%d", s.port),
		"DROPBOX_IP=127.0.0.1",
		"DROPBOX_APP_KEY=test-key",
		"DROPBOX_APP_SECRET=test-secret",
		"DROPBOX_REFRESH_TOKEN=test-refresh-token",
		"DROPBOX_LONGPOLL_TIMEOUT=1",
		"HTTPS_PROXY=http://127.0.0.1:1",
		"HTTP_PROXY=http://127.0.0.1:1",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start dropbox serve: %w", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	s.started = true
	s.t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			if cmd.Process != nil {
				_ = cmd.Process.Kill()
			}
			<-done
		}
	})

	waitURL := fmt.Sprintf("http://127.0.0.1:%d/health", s.port)
	deadline := time.After(10 * time.Second)
	tick := time.NewTicker(10 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-ctx.Done():
			cancel()
			return ctx.Err()
		case err := <-done:
			cancel()
			return fmt.Errorf("dropbox serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("dropbox did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
		case <-tick.C:
			resp, err := http.Get(waitURL)
			if err != nil {
				continue
			}
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil && resp.StatusCode == http.StatusOK {
				s.healthBody = string(body)
				return nil
			}
		}
	}
}

type promptsBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *promptsBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("PROMPTS_PORT=%d", s.port),
		"PROMPTS_IP=127.0.0.1",
		"PROMPTS_CRON_FEED_URL=http://127.0.0.1:1/feed",
		"PROMPTS_CRM_FEED_URL=http://127.0.0.1:1/feed",
		"PROMPTS_LEDGER_FEED_URL=http://127.0.0.1:1/feed",
		"PROMPTS_DROPBOX_FEED_URL=http://127.0.0.1:1/feed",
		"PROMPTS_SCRIPTS_FEED_URL=http://127.0.0.1:1/feed",
		"PROMPTS_PROMPTS_FEED_URL=http://127.0.0.1:1/feed",
		"ANTHROPIC_API_KEY=sk-test",
		"OPENAI_API_KEY=sk-test",
		"GEMINI_API_KEY=sk-test",
		"ZAI_API_KEY=sk-test",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start prompts serve: %w", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	s.started = true
	s.t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			if cmd.Process != nil {
				_ = cmd.Process.Kill()
			}
			<-done
		}
	})

	waitURL := fmt.Sprintf("http://127.0.0.1:%d/health", s.port)
	deadline := time.After(10 * time.Second)
	tick := time.NewTicker(10 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-ctx.Done():
			cancel()
			return ctx.Err()
		case err := <-done:
			cancel()
			return fmt.Errorf("prompts serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("prompts did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
		case <-tick.C:
			resp, err := http.Get(waitURL)
			if err != nil {
				continue
			}
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil && resp.StatusCode == http.StatusOK {
				s.healthBody = string(body)
				return nil
			}
		}
	}
}

type wikiBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *wikiBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("WIKI_PORT=%d", s.port),
		"WIKI_IP=127.0.0.1",
		"ANTHROPIC_API_KEY=sk-test",
		"OPENAI_API_KEY=sk-test",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start wiki serve: %w", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	s.started = true
	s.t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			if cmd.Process != nil {
				_ = cmd.Process.Kill()
			}
			<-done
		}
	})

	waitURL := fmt.Sprintf("http://127.0.0.1:%d/health", s.port)
	deadline := time.After(10 * time.Second)
	tick := time.NewTicker(10 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-ctx.Done():
			cancel()
			return ctx.Err()
		case err := <-done:
			cancel()
			return fmt.Errorf("wiki serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("wiki did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
		case <-tick.C:
			resp, err := http.Get(waitURL)
			if err != nil {
				continue
			}
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil && resp.StatusCode == http.StatusOK {
				s.healthBody = string(body)
				return nil
			}
		}
	}
}

type cronBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *cronBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("CRON_PORT=%d", s.port),
		"CRON_IP=127.0.0.1",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start cron serve: %w", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	s.started = true
	s.t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			if cmd.Process != nil {
				_ = cmd.Process.Kill()
			}
			<-done
		}
	})

	waitURL := fmt.Sprintf("http://127.0.0.1:%d/health", s.port)
	deadline := time.After(10 * time.Second)
	tick := time.NewTicker(10 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-ctx.Done():
			cancel()
			return ctx.Err()
		case err := <-done:
			cancel()
			return fmt.Errorf("cron serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("cron did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
		case <-tick.C:
			resp, err := http.Get(waitURL)
			if err != nil {
				continue
			}
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil && resp.StatusCode == http.StatusOK {
				s.healthBody = string(body)
				return nil
			}
		}
	}
}

type gmailBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *gmailBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("GMAIL_PORT=%d", s.port),
		"GMAIL_IP=127.0.0.1",
		"GMAIL_CLIENT_ID=test-client-id",
		"GMAIL_CLIENT_SECRET=test-client-secret",
		"GMAIL_REFRESH_TOKEN=test-refresh-token",
		"GMAIL_POLL_INTERVAL=24h",
		"HTTPS_PROXY=http://127.0.0.1:1",
		"HTTP_PROXY=http://127.0.0.1:1",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start gmail serve: %w", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	s.started = true
	s.t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			if cmd.Process != nil {
				_ = cmd.Process.Kill()
			}
			<-done
		}
	})

	waitURL := fmt.Sprintf("http://127.0.0.1:%d/health", s.port)
	deadline := time.After(10 * time.Second)
	tick := time.NewTicker(10 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-ctx.Done():
			cancel()
			return ctx.Err()
		case err := <-done:
			cancel()
			return fmt.Errorf("gmail serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("gmail did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
		case <-tick.C:
			resp, err := http.Get(waitURL)
			if err != nil {
				continue
			}
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil && resp.StatusCode == http.StatusOK {
				s.healthBody = string(body)
				return nil
			}
		}
	}
}

type sitesBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *sitesBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("SITES_PORT=%d", s.port),
		"SITES_IP=127.0.0.1",
		"SITES_ROOT="+s.layout.WWWRoot(),
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start sites serve: %w", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	s.started = true
	s.t.Cleanup(func() {
		cancel()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			if cmd.Process != nil {
				_ = cmd.Process.Kill()
			}
			<-done
		}
	})

	waitURL := fmt.Sprintf("http://127.0.0.1:%d/health", s.port)
	deadline := time.After(10 * time.Second)
	tick := time.NewTicker(10 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-ctx.Done():
			cancel()
			return ctx.Err()
		case err := <-done:
			cancel()
			return fmt.Errorf("sites serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("sites did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
		case <-tick.C:
			resp, err := http.Get(waitURL)
			if err != nil {
				continue
			}
			body, readErr := io.ReadAll(resp.Body)
			resp.Body.Close()
			if readErr == nil && resp.StatusCode == http.StatusOK {
				s.healthBody = string(body)
				return nil
			}
		}
	}
}

func buildNotifyArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "notify")
	notifyDir, err := filepath.Abs(filepath.Join("..", "..", "..", "notify"))
	if err != nil {
		t.Fatalf("resolve notify dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/notify")
	cmd.Dir = notifyDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build notify artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "notify", version, "notify-"+version, out)
}

func buildWikiArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "wiki")
	wikiDir, err := filepath.Abs(filepath.Join("..", "..", "..", "wiki"))
	if err != nil {
		t.Fatalf("resolve wiki dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/wiki")
	cmd.Dir = wikiDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build wiki artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "wiki", version, "wiki-"+version, out)
}

func buildPromptsArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "prompts")
	promptsDir, err := filepath.Abs(filepath.Join("..", "..", "..", "prompts"))
	if err != nil {
		t.Fatalf("resolve prompts dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/prompts")
	cmd.Dir = promptsDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build prompts artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "prompts", version, "prompts-"+version, out)
}

func buildDropboxArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "dropbox")
	dropboxDir, err := filepath.Abs(filepath.Join("..", "..", "..", "dropbox"))
	if err != nil {
		t.Fatalf("resolve dropbox dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/dropbox")
	cmd.Dir = dropboxDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build dropbox artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "dropbox", version, "dropbox-"+version, out)
}

func buildGmailArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "gmail")
	gmailDir, err := filepath.Abs(filepath.Join("..", "..", "..", "gmail"))
	if err != nil {
		t.Fatalf("resolve gmail dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/gmail")
	cmd.Dir = gmailDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build gmail artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "gmail", version, "gmail-"+version, out)
}

func buildCronArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "cron")
	cronDir, err := filepath.Abs(filepath.Join("..", "..", "..", "cron"))
	if err != nil {
		t.Fatalf("resolve cron dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/cron")
	cmd.Dir = cronDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build cron artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "cron", version, "cron-"+version, out)
}

func buildSitesArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "sites")
	sitesDir, err := filepath.Abs(filepath.Join("..", "..", "..", "sites"))
	if err != nil {
		t.Fatalf("resolve sites dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/sites")
	cmd.Dir = sitesDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build sites artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "sites", version, "sites-"+version, out)
}

func manifestEnv(path string) []string {
	body, err := os.ReadFile(path)
	if err != nil {
		return nil
	}
	var env []string
	for _, line := range strings.Split(string(body), "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") || !strings.Contains(line, "=") {
			continue
		}
		env = append(env, line)
	}
	return env
}

func freePort(t *testing.T) int {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("reserve port: %v", err)
	}
	defer ln.Close()
	return ln.Addr().(*net.TCPAddr).Port
}
