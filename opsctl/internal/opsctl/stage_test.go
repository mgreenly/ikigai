package opsctl

import (
	"context"
	"os"
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
	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	l := NewLayout(root, app)
	o.Runner = fakeRunner{
		baseEnv: fakeEnv(app, "v1.0.0", 1, ""),
		commitByPath: map[string]string{
			l.LibexecBinary("v1.0.0"): existCommit,
			artifact:                  incomingCommit,
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

	art1 := stageArtifact(t, "ledger-v1.0.0-a")
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
	art2 := stageArtifact(t, "ledger-v1.0.0-b")
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
	if err := o1.Stage(context.Background(), app, "v1.0.0", stageArtifact(t, "ledger-a"), false); err != nil {
		t.Fatalf("first stage: %v", err)
	}

	// A second build of the SAME version at a DIFFERENT commit must be refused.
	art := stageArtifact(t, "ledger-b")
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
	if err := o1.Stage(context.Background(), app, "v1.0.0", stageArtifact(t, "ledger-a"), false); err != nil {
		t.Fatalf("first stage: %v", err)
	}

	art := stageArtifact(t, "ledger-b")
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
