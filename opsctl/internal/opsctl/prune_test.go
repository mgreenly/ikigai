package opsctl

import (
	"context"
	"os"
	"sort"
	"testing"
)

// installSeq installs a sequence of versions (each with the same embedded schema,
// so no backups complicate the release set) and returns the Opsctl.
func installSeq(t *testing.T, root, app string, sys *stubSystem, keep int, versions []string) *Opsctl {
	t.Helper()
	var o *Opsctl
	for _, v := range versions {
		o = newOpsctl(t, root, app, sys, fakeEnv(app, v, 1, ""))
		o.Keep = keep
		if err := stageAndDeploy(t, o, app, v, stageArtifact(t, app+"-"+v)); err != nil {
			t.Fatalf("deploy %s: %v", v, err)
		}
	}
	return o
}

func releaseDirs(t *testing.T, l Layout) []string {
	t.Helper()
	entries, err := os.ReadDir(l.ReleasesDir())
	if err != nil {
		t.Fatalf("read releases: %v", err)
	}
	var out []string
	for _, e := range entries {
		if e.IsDir() {
			out = append(out, e.Name())
		}
	}
	sort.SliceStable(out, func(i, j int) bool { return l.compareVersion(out[i], out[j]) < 0 })
	return out
}

// TestPrune_KeepsExactlyN drives five installs with Keep=3 and asserts exactly the
// newest three releases survive, that current's target is among them, and that
// current is never deleted. Prune runs at the tail of each install.
func TestPrune_KeepsExactlyN(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	versions := []string{"v1.0.0", "v1.1.0", "v1.2.0", "v1.3.0", "v1.4.0"}
	installSeq(t, root, app, sys, 3, versions)

	got := releaseDirs(t, l)
	want := []string{"v1.2.0", "v1.3.0", "v1.4.0"} // newest 3
	if len(got) != len(want) {
		t.Fatalf("after prune kept %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("after prune kept %v, want %v", got, want)
		}
	}

	// current must still resolve to its target, and that target must survive.
	cur := readlinkBase(t, l.CurrentLink())
	if cur != "v1.4.0" {
		t.Fatalf("current = %q, want v1.4.0", cur)
	}
	found := false
	for _, v := range got {
		if v == cur {
			found = true
		}
	}
	if !found {
		t.Fatalf("prune deleted current's target %q (kept %v)", cur, got)
	}
}

func TestPrune_NewestSetUsesSemanticNumericOrdering(t *testing.T) {
	// R-3X6F-RW87
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	for _, v := range []string{"v0.7.10", "v0.7.9"} {
		if err := os.MkdirAll(l.ReleaseDir(v), 0o755); err != nil {
			t.Fatalf("mkdir release %s: %v", v, err)
		}
	}

	o := newOpsctl(t, root, app, &stubSystem{}, fakeEnv(app, "v0.7.10", 1, ""))
	o.Keep = 1
	if err := o.Prune(context.Background(), app); err != nil {
		t.Fatalf("prune: %v", err)
	}

	got := releaseDirs(t, l)
	want := []string{"v0.7.10"}
	if len(got) != len(want) || got[0] != want[0] {
		t.Fatalf("after prune kept %v, want %v", got, want)
	}
}

// TestPrune_NeverDeletesCurrentEvenIfOldest sets current to an OLD release (via a
// rollback) and then installs a chain; prune with a small N must still preserve
// current's target and its predecessor, never deleting the live rollback target.
func TestPrune_NeverDeletesCurrentTarget(t *testing.T) {
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	// Install three; current = v3. Roll back to v2 so current is NOT the newest.
	o := installSeq(t, root, app, sys, 3, []string{"v1.0.0", "v2.0.0", "v3.0.0"})
	if err := o.Rollback(context.Background(), app, ""); err != nil {
		t.Fatalf("rollback: %v", err)
	}
	if cur := readlinkBase(t, l.CurrentLink()); cur != "v2.0.0" {
		t.Fatalf("current after rollback = %q, want v2.0.0", cur)
	}

	// Standalone prune with Keep=1 — would normally keep only v3.0.0, but current
	// (v2.0.0) and its predecessor (v1.0.0) must be retained too.
	o.Keep = 1
	if err := o.Prune(context.Background(), app); err != nil {
		t.Fatalf("prune: %v", err)
	}
	got := releaseDirs(t, l)
	// Keep=1 → {v3.0.0}; plus current v2.0.0; plus current's predecessor v1.0.0.
	want := map[string]bool{"v1.0.0": true, "v2.0.0": true, "v3.0.0": true}
	if len(got) != len(want) {
		t.Fatalf("prune kept %v, want all of %v (current never deleted)", got, want)
	}
	for _, v := range got {
		if !want[v] {
			t.Fatalf("prune kept unexpected %q (want %v)", v, want)
		}
	}
}
