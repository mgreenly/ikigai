package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
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
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
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
	mismatch := stageArtifact(t, "mismatch")
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

	// Not a static ELF: a text file as the artifact.
	bad := filepath.Join(t.TempDir(), "not-elf")
	if err := os.WriteFile(bad, []byte("#!/bin/sh\necho hi\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	o2 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	if err := o2.Stage(context.Background(), app, "v1.0.0", bad, false); err == nil || !strings.Contains(err.Error(), "ELF") {
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
// target the data dir, on every install. The stub records (never executes) the
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
