package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// newCommitOpsctl builds an Opsctl whose fake runner self-reports a per-binary
// commit SHA: the already-placed release binary reports existCommit, the incoming
// artifact reports incomingCommit. This mirrors a real binary self-reporting its
// own ldflag-stamped commit regardless of the runner's env — which is what the
// stage collision guard compares.
func newCommitOpsctl(t *testing.T, root, app string, sys *stubSystem, existCommit, incomingCommit, artifact string) *Opsctl {
	t.Helper()
	o := newOpsctl(t, root, app, sys, append(fakeEnv(app, "v1.0.0", 1, ""), "FAKE_COMMIT="+incomingCommit))
	l := NewLayout(root, app)
	o.Runner = fakeRunner{
		baseEnv: append(fakeEnv(app, "v1.0.0", 1, ""), "FAKE_COMMIT="+incomingCommit),
		commitByPath: map[string]string{
			l.LibexecBinary("v1.0.0"): existCommit,
		},
	}
	return o
}

// TestStage_SameSHANoOp asserts re-staging the same version at the SAME commit is
// an idempotent no-op: the already-placed release binary is NOT re-copied, and the
// /tmp artifact is still deleted (decision 2 — the release is confirmed in place).
func TestStage_SameSHANoOp(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	art1 := stageBundleArtifact(t, app, "v1.0.0", "ledger-v1.0.0-a")
	o := newCommitOpsctl(t, root, app, sys, "deadbeef", "deadbeef", art1)
	if err := o.Stage(context.Background(), app, "v1.0.0", art1, false); err != nil {
		t.Fatalf("first stage: %v", err)
	}
	if _, err := os.Stat(art1); !os.IsNotExist(err) {
		t.Fatalf("first stage did not delete the /tmp artifact (err=%v)", err)
	}
	relBin := l.LibexecBinary("v1.0.0")
	info1, err := os.Stat(relBin)
	if err != nil {
		t.Fatalf("release binary missing after stage: %v", err)
	}

	// Re-stage the same version at the same commit → idempotent no-op (no re-copy),
	// /tmp still deleted.
	art2 := stageBundleArtifact(t, app, "v1.0.0", "ledger-v1.0.0-b")
	o2 := newCommitOpsctl(t, root, app, sys, "deadbeef", "deadbeef", art2)
	if err := o2.Stage(context.Background(), app, "v1.0.0", art2, false); err != nil {
		t.Fatalf("idempotent re-stage: %v", err)
	}
	if _, err := os.Stat(art2); !os.IsNotExist(err) {
		t.Fatalf("idempotent re-stage did not delete the /tmp artifact (err=%v)", err)
	}
	info2, err := os.Stat(relBin)
	if err != nil {
		t.Fatalf("release binary missing after re-stage: %v", err)
	}
	if !info1.ModTime().Equal(info2.ModTime()) {
		t.Errorf("same-SHA re-stage re-copied the release binary (mtime %v -> %v)", info1.ModTime(), info2.ModTime())
	}
}

// TestStage_DifferentSHARefuses asserts a different-commit collision is refused
// without --force, and the /tmp artifact is KEPT so the operator can retry.
func TestStage_DifferentSHARefuses(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	sys := &stubSystem{}

	o1 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	if err := o1.Stage(context.Background(), app, "v1.0.0", stageBundleArtifact(t, app, "v1.0.0", "ledger-a"), false); err != nil {
		t.Fatalf("first stage: %v", err)
	}

	// A second build of the SAME version at a DIFFERENT commit must be refused.
	art := stageBundleArtifact(t, app, "v1.0.0", "ledger-b")
	o2 := newCommitOpsctl(t, root, app, sys, "deadbeef", "cafef00d", art)
	err := o2.Stage(context.Background(), app, "v1.0.0", art, false)
	if err == nil || !strings.Contains(err.Error(), "already staged at commit") {
		t.Fatalf("different-SHA stage err = %v, want a collision refusal", err)
	}
	// Refusal keeps the /tmp artifact (decision 2).
	if _, err := os.Stat(art); err != nil {
		t.Errorf("collision refusal removed the /tmp artifact, want it kept: %v", err)
	}
}

// TestStage_ForceOverride asserts --force replaces an already-staged release at a
// different commit (the documented escape hatch, incl. two -dirty stamps).
func TestStage_ForceOverride(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	sys := &stubSystem{}

	o1 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	if err := o1.Stage(context.Background(), app, "v1.0.0", stageBundleArtifact(t, app, "v1.0.0", "ledger-a"), false); err != nil {
		t.Fatalf("first stage: %v", err)
	}

	art := stageBundleArtifact(t, app, "v1.0.0", "ledger-b")
	o2 := newCommitOpsctl(t, root, app, sys, "deadbeef", "cafef00d", art)
	if err := o2.Stage(context.Background(), app, "v1.0.0", art, true); err != nil {
		t.Fatalf("--force stage: %v", err)
	}
	// On success --force deletes the /tmp artifact like any other placement.
	if _, err := os.Stat(art); !os.IsNotExist(err) {
		t.Fatalf("--force stage did not delete the /tmp artifact (err=%v)", err)
	}
}

// TestDeploy_NotStagedGuard asserts deploy refuses early when the release was never
// staged, pointing the operator at stage — before any manifest/schema/migrate exec.
func TestDeploy_NotStagedGuard(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	sys := &stubSystem{}

	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	err := o.Deploy(context.Background(), app, "v1.0.0")
	if err == nil || !strings.Contains(err.Error(), "not staged") {
		t.Fatalf("unstaged deploy err = %v, want a 'not staged' refusal", err)
	}
	if sys.restarts != 0 {
		t.Errorf("unstaged deploy still restarted the unit (%d times)", sys.restarts)
	}
}

func TestStage_RefusesInvalidVersionAndDifferentRestageThenUnpacksBundleTiers(t *testing.T) {
	// R-84VR-7U2K
	root := t.TempDir()
	app := "ledger"
	version := "v1.0.0"
	l := NewLayout(root, app)
	sys := &stubSystem{}
	ctx := context.Background()

	badVersionArtifact := stageBundleArtifact(t, app, version, "ledger-invalid-version")
	o := newOpsctl(t, root, app, sys, fakeEnv(app, version, 1, ""))
	if err := o.Stage(ctx, app, "v1.2", badVersionArtifact, false); err == nil || !strings.Contains(err.Error(), "invalid version") {
		t.Fatalf("invalid-version stage err = %v, want invalid version refusal", err)
	}
	if _, err := os.Stat(l.AppDir()); !os.IsNotExist(err) {
		t.Fatalf("invalid version unpacked app tree (stat err=%v)", err)
	}
	if _, err := os.Stat(badVersionArtifact); err != nil {
		t.Fatalf("invalid-version refusal removed artifact: %v", err)
	}

	first := stageBundleArtifact(t, app, version, "ledger-first")
	firstOps := newCommitOpsctl(t, root, app, sys, "deadbeef", "deadbeef", first)
	if err := firstOps.Stage(ctx, app, version, first, false); err != nil {
		t.Fatalf("first stage: %v", err)
	}
	assertBundleTiers(t, l, version)
	if _, err := os.Stat(first); !os.IsNotExist(err) {
		t.Fatalf("successful stage did not delete artifact (err=%v)", err)
	}

	refused := stageBundleArtifact(t, app, version, "ledger-different")
	refusedOps := newCommitOpsctl(t, root, app, sys, "deadbeef", "cafef00d", refused)
	err := refusedOps.Stage(ctx, app, version, refused, false)
	if err == nil || !strings.Contains(err.Error(), "already staged at commit") {
		t.Fatalf("different re-stage err = %v, want commit collision refusal", err)
	}
	if _, err := os.Stat(refused); err != nil {
		t.Fatalf("collision refusal removed artifact: %v", err)
	}

	forced := stageBundleArtifact(t, app, version, "ledger-forced")
	forcedOps := newCommitOpsctl(t, root, app, sys, "deadbeef", "cafef00d", forced)
	if err := forcedOps.Stage(ctx, app, version, forced, true); err != nil {
		t.Fatalf("forced re-stage: %v", err)
	}
	assertBundleTiers(t, l, version)
	if _, err := os.Stat(forced); !os.IsNotExist(err) {
		t.Fatalf("forced stage did not delete artifact (err=%v)", err)
	}
}

func TestStage_UnpacksVersionedBundlePaths(t *testing.T) {
	// R-1BF5-X7QS
	root := t.TempDir()
	app := "ledger"
	version := "v2.3.4"
	l := NewLayout(root, app)
	artifact := stageBundleArtifact(t, app, version, "ledger-v2.3.4")
	o := newOpsctl(t, root, app, &stubSystem{}, fakeEnv(app, version, 1, ""))

	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage bundle: %v", err)
	}
	assertBundleTiers(t, l, version)
}

func assertBundleTiers(t *testing.T, l Layout, version string) {
	t.Helper()
	for _, path := range []string{
		l.LibexecBinary(version),
		l.NginxConfFile(version),
		l.ManifestFile(version),
		filepath.Join(l.ShareVersionDir(version), "assets", "resource.txt"),
	} {
		if _, err := os.Stat(path); err != nil {
			t.Fatalf("staged tier path %s missing: %v", path, err)
		}
	}
	body, err := os.ReadFile(l.ManifestFile(version))
	if err != nil {
		t.Fatalf("read staged manifest: %v", err)
	}
	if !strings.Contains(string(body), "APP="+l.App) {
		t.Fatalf("staged manifest does not name app: %q", string(body))
	}
}
