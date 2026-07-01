package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"testing"
	"time"
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
	entries, err := os.ReadDir(l.LibexecDir())
	if err != nil {
		t.Fatalf("read libexec: %v", err)
	}
	var out []string
	prefix := l.App + "-"
	for _, e := range entries {
		if !e.IsDir() && strings.HasPrefix(e.Name(), prefix) {
			out = append(out, strings.TrimPrefix(e.Name(), prefix))
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
	cur := readRunVersion(t, l)
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

func TestPrune_RemovesOldLibexecBinaries(t *testing.T) {
	// R-3VYJ-E4HI
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	installSeq(t, root, app, sys, 2, []string{"v1.0.0", "v1.1.0", "v1.2.0"})

	if _, err := os.Stat(l.LibexecBinary("v1.0.0")); !os.IsNotExist(err) {
		t.Fatalf("old libexec binary still exists after prune (err=%v)", err)
	}
	for _, v := range []string{"v1.1.0", "v1.2.0"} {
		if _, err := os.Stat(l.LibexecBinary(v)); err != nil {
			t.Fatalf("kept libexec binary %s missing after prune: %v", v, err)
		}
	}
	if got := readRunVersion(t, l); got != "v1.2.0" {
		t.Fatalf("live version = %q, want v1.2.0", got)
	}
}

func TestPrune_RemovesCompleteOlderVersionSets(t *testing.T) {
	// R-1CN2-AZHH
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	versions := []string{"v1.0.0", "v1.1.0", "v1.2.0", "v1.3.0", "v1.4.0"}
	o := installSeq(t, root, app, sys, len(versions)+1, versions)
	for _, v := range versions {
		assertReleaseSetExists(t, l, v)
	}

	o.Keep = 1
	if err := o.Prune(context.Background(), app); err != nil {
		t.Fatalf("prune: %v", err)
	}

	got := releaseDirs(t, l)
	want := []string{"v1.3.0", "v1.4.0"}
	if len(got) != len(want) {
		t.Fatalf("after prune kept %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("after prune kept %v, want %v", got, want)
		}
	}

	for _, v := range []string{"v1.0.0", "v1.1.0", "v1.2.0"} {
		assertReleaseSetRemoved(t, l, v)
	}
	for _, v := range []string{"v1.3.0", "v1.4.0"} {
		assertReleaseSetExists(t, l, v)
	}
	if got := readRunVersion(t, l); got != "v1.4.0" {
		t.Fatalf("live version = %q, want v1.4.0", got)
	}
}

func TestPrune_NewestSetUsesSemanticNumericOrdering(t *testing.T) {
	// R-3X6F-RW87
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	for _, v := range []string{"v0.7.10", "v0.7.9"} {
		if err := os.MkdirAll(l.LibexecDir(), 0o755); err != nil {
			t.Fatalf("mkdir libexec: %v", err)
		}
		if err := os.WriteFile(l.LibexecBinary(v), []byte(v), 0o755); err != nil {
			t.Fatalf("write libexec binary %s: %v", v, err)
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

func assertReleaseSetExists(t *testing.T, l Layout, version string) {
	t.Helper()
	for name, path := range map[string]string{
		"libexec binary": l.LibexecBinary(version),
		"nginx config":   l.NginxConfFile(version),
		"manifest":       l.ManifestFile(version),
		"share dir":      l.ShareVersionDir(version),
	} {
		if _, err := os.Stat(path); err != nil {
			t.Fatalf("%s for %s missing: %v", name, version, err)
		}
	}
}

func assertReleaseSetRemoved(t *testing.T, l Layout, version string) {
	t.Helper()
	for name, path := range map[string]string{
		"libexec binary": l.LibexecBinary(version),
		"etc dir":        l.EtcVersionDir(version),
		"share dir":      l.ShareVersionDir(version),
	} {
		if _, err := os.Stat(path); !os.IsNotExist(err) {
			t.Fatalf("%s for pruned %s still exists (err=%v)", name, version, err)
		}
	}
}

func TestPrune_NewestSetUsesLibexecMtimeForBuildMetadata(t *testing.T) {
	// R-4221-AZ6Z
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	older := "v1.0.0+old"
	newer := "v1.0.0+new"
	for _, v := range []string{older, newer} {
		libexecFile := l.LibexecBinary(v)
		if err := os.MkdirAll(filepath.Dir(libexecFile), 0o755); err != nil {
			t.Fatalf("mkdir libexec %s: %v", v, err)
		}
		if err := os.WriteFile(libexecFile, []byte(v), 0o644); err != nil {
			t.Fatalf("write libexec file %s: %v", v, err)
		}
	}
	base := time.Unix(1700000000, 0)
	if err := os.Chtimes(l.LibexecBinary(older), base, base); err != nil {
		t.Fatalf("chtime older libexec: %v", err)
	}
	if err := os.Chtimes(l.LibexecBinary(newer), base.Add(time.Hour), base.Add(time.Hour)); err != nil {
		t.Fatalf("chtime newer libexec: %v", err)
	}

	o := newOpsctl(t, root, app, &stubSystem{}, fakeEnv(app, newer, 1, ""))
	o.Keep = 1
	if err := o.Prune(context.Background(), app); err != nil {
		t.Fatalf("prune: %v", err)
	}

	got := releaseDirs(t, l)
	want := []string{newer}
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
	if cur := readRunVersion(t, l); cur != "v2.0.0" {
		t.Fatalf("live version after rollback = %q, want v2.0.0", cur)
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
