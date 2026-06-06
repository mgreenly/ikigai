package opsctl

import (
	"context"
	"os"
	"strings"
	"testing"
)

// TestStatus_SingleApp deploys one app and asserts `status <app>` reports its
// version, the binary's self-reported commit SHA, and the unit's raw state. The
// stub's settable activeState drives the ACTIVE column (here "failed") so the raw
// state is shown rather than collapsed to an error.
func TestStatus_SingleApp(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	sys := &stubSystem{activeState: "failed"}

	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	// The live binary must self-report a commit SHA for the SHA column; give the
	// runner a per-path commit for the deployed release binary.
	l := NewLayout(root, app)
	o.Runner = fakeRunner{
		baseEnv: fakeEnv(app, "v1.0.0", 1, ""),
		commitByPath: map[string]string{
			l.ReleaseBinary("v1.0.0"): "deadbeef",
			l.CurrentBinary():         "deadbeef", // status reads via the current symlink path
		},
	}
	if err := stageAndDeploy(t, o, app, "v1.0.0", stageArtifact(t, "ledger-a")); err != nil {
		t.Fatalf("deploy: %v", err)
	}

	var buf strings.Builder
	o.Out = &buf
	if err := o.Status(context.Background(), app); err != nil {
		t.Fatalf("status: %v", err)
	}
	out := buf.String()
	for _, want := range []string{app, "v1.0.0", "deadbeef", "failed"} {
		if !strings.Contains(out, want) {
			t.Errorf("status output missing %q:\n%s", want, out)
		}
	}
}

// TestStatus_AllAppsDiscovery deploys two apps and asserts `status` (no arg)
// discovers both via the OPSCTL_ROOT scan and reports a row for each, while a
// setup-but-never-deployed tree (no `current` symlink) is skipped.
func TestStatus_AllAppsDiscovery(t *testing.T) {
	root := t.TempDir()
	sys := &stubSystem{}

	for _, app := range []string{"crm", "ledger"} {
		o := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
		if err := stageAndDeploy(t, o, app, "v1.0.0", stageArtifact(t, app+"-a")); err != nil {
			t.Fatalf("deploy %s: %v", app, err)
		}
	}

	// A bare app dir with no `current` symlink must NOT appear (only deployed apps).
	if err := os.MkdirAll(NewLayout(root, "notify").AppDir(), 0o755); err != nil {
		t.Fatalf("mkdir notify tree: %v", err)
	}

	o := newOpsctl(t, root, "crm", sys, fakeEnv("crm", "v1.0.0", 1, ""))
	var buf strings.Builder
	o.Out = &buf
	if err := o.Status(context.Background(), ""); err != nil {
		t.Fatalf("status all: %v", err)
	}
	out := buf.String()
	if !strings.Contains(out, "crm") || !strings.Contains(out, "ledger") {
		t.Errorf("status all missing a discovered app:\n%s", out)
	}
	if strings.Contains(out, "notify") {
		t.Errorf("status all listed the un-deployed app notify:\n%s", out)
	}
}
