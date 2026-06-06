package opsctl

import (
	"context"
	"strings"
	"testing"
)

// TestReleases_CurrentAndPredecessor deploys three ascending versions and asserts
// `releases <app>` lists them in order, marking the live one (current) and its
// immediate predecessor (the default rollback target). Older releases carry no
// mark. A high --keep on the test Opsctl avoids prune dropping them.
func TestReleases_CurrentAndPredecessor(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	sys := &stubSystem{}

	versions := []string{"v0.1.0", "v0.1.1", "v0.1.2"}
	for _, v := range versions {
		o := newOpsctl(t, root, app, sys, fakeEnv(app, v, 1, ""))
		o.Keep = 10 // keep every release so the history is intact for the listing
		if err := stageAndDeploy(t, o, app, v, stageArtifact(t, app+"-"+v)); err != nil {
			t.Fatalf("deploy %s: %v", v, err)
		}
	}

	o := newOpsctl(t, root, app, sys, fakeEnv(app, "v0.1.2", 1, ""))
	var buf strings.Builder
	o.Out = &buf
	if err := o.Releases(context.Background(), app); err != nil {
		t.Fatalf("releases: %v", err)
	}

	lines := strings.Split(strings.TrimSpace(buf.String()), "\n")
	if len(lines) != 3 {
		t.Fatalf("expected 3 release lines, got %d:\n%s", len(lines), buf.String())
	}
	// Ascending order: oldest first, current last.
	if !strings.HasPrefix(lines[0], "v0.1.0") || strings.Contains(lines[0], "(") {
		t.Errorf("line 0 = %q, want unmarked v0.1.0", lines[0])
	}
	if !strings.HasPrefix(lines[1], "v0.1.1") || !strings.Contains(lines[1], "predecessor") {
		t.Errorf("line 1 = %q, want v0.1.1 marked predecessor", lines[1])
	}
	if !strings.HasPrefix(lines[2], "v0.1.2") || !strings.Contains(lines[2], "current") {
		t.Errorf("line 2 = %q, want v0.1.2 marked current", lines[2])
	}
}

// TestReleases_None asserts an app with no releases reports cleanly rather than
// erroring.
func TestReleases_None(t *testing.T) {
	root := t.TempDir()
	o := newOpsctl(t, root, "ledger", &stubSystem{}, fakeEnv("ledger", "v0.1.0", 1, ""))
	var buf strings.Builder
	o.Out = &buf
	if err := o.Releases(context.Background(), "ledger"); err != nil {
		t.Fatalf("releases (none): %v", err)
	}
	if !strings.Contains(buf.String(), "no releases") {
		t.Errorf("empty releases output = %q, want a 'no releases' notice", buf.String())
	}
}
