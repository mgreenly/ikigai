package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestCompareVersion_NumericPatchOrdering(t *testing.T) {
	// R-3X6F-RW87
	l := NewLayout(t.TempDir(), "ledger")
	if got := l.compareVersion("v0.7.10", "v0.7.9"); got <= 0 {
		t.Fatalf("compareVersion(v0.7.10, v0.7.9) = %d, want v0.7.10 ordered after v0.7.9", got)
	}
	if got := l.compareVersion("v0.7.9", "v0.7.10"); got >= 0 {
		t.Fatalf("compareVersion(v0.7.9, v0.7.10) = %d, want v0.7.9 ordered before v0.7.10", got)
	}
}

func TestCompareVersion_PrereleaseOrdering(t *testing.T) {
	// R-3YEC-5NYW
	l := NewLayout(t.TempDir(), "ledger")
	cases := []struct {
		older string
		newer string
	}{
		{"v1.0.0-rc.1", "v1.0.0"},
		{"v1.0.0-rc.2", "v1.0.0-rc.10"},
	}
	for _, tc := range cases {
		if got := l.compareVersion(tc.older, tc.newer); got >= 0 {
			t.Fatalf("compareVersion(%s, %s) = %d, want older prerelease ordered first", tc.older, tc.newer, got)
		}
	}
}

func TestCompareVersion_BuildMetadataPrecedenceEqual(t *testing.T) {
	// R-40U4-X7GA
	l := NewLayout(t.TempDir(), "ledger")
	a := "v0.7.1+aaaa"
	b := "v0.7.1+bbbb"
	if l.LibexecBinary(a) == l.LibexecBinary(b) {
		t.Fatalf("LibexecBinary(%q) and LibexecBinary(%q) collapsed build metadata identity", a, b)
	}
	for _, v := range []string{a, b} {
		libexecFile := l.LibexecBinary(v)
		if err := os.MkdirAll(filepath.Dir(libexecFile), 0o755); err != nil {
			t.Fatalf("mkdir libexec %s: %v", v, err)
		}
		if err := os.WriteFile(libexecFile, []byte(v), 0o644); err != nil {
			t.Fatalf("write libexec file %s: %v", v, err)
		}
	}
	sameTime := time.Unix(1700000100, 0)
	if err := os.Chtimes(l.LibexecBinary(a), sameTime, sameTime); err != nil {
		t.Fatalf("chtime libexec %s: %v", a, err)
	}
	if err := os.Chtimes(l.LibexecBinary(b), sameTime, sameTime); err != nil {
		t.Fatalf("chtime libexec %s: %v", b, err)
	}
	if got := l.compareVersion(a, b); got != 0 {
		t.Fatalf("compareVersion(%s, %s) = %d, want precedence-equal", a, b, got)
	}
}

func TestCompareVersion_PrecedenceEqualBuildsUseLibexecMtime(t *testing.T) {
	// R-4221-AZ6Z
	l := NewLayout(t.TempDir(), "ledger")
	older := "v0.7.1+aaaa"
	newer := "v0.7.1+bbbb"
	for _, v := range []string{older, newer} {
		libexecFile := l.LibexecBinary(v)
		if err := os.MkdirAll(filepath.Dir(libexecFile), 0o755); err != nil {
			t.Fatalf("mkdir libexec %s: %v", v, err)
		}
		if err := os.WriteFile(libexecFile, []byte("stamp"), 0o644); err != nil {
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

	if got := l.compareVersion(older, newer); got >= 0 {
		t.Fatalf("compareVersion(%s, %s) = %d, want older mtime sorted first", older, newer, got)
	}
	if got := l.compareVersion(newer, older); got <= 0 {
		t.Fatalf("compareVersion(%s, %s) = %d, want newer mtime sorted last", newer, older, got)
	}
	o := &Opsctl{}
	rels, err := o.listReleases(l)
	if err != nil {
		t.Fatalf("listReleases: %v", err)
	}
	want := []string{older, newer}
	if strings.Join(rels, ",") != strings.Join(want, ",") {
		t.Fatalf("listReleases = %v, want %v", rels, want)
	}
}

func TestVersionTakingBoundariesRejectInvalidSemVer(t *testing.T) {
	// R-439X-OQXO
	ctx := context.Background()
	root := t.TempDir()
	app := "ledger"
	for _, good := range []string{"v0.7.1", "v1.2.3-rc.1", "v1.2.3+build.5", "v1.2.3-rc.1+build.5"} {
		if !validVersion(good) {
			t.Fatalf("validVersion(%q) = false, want true", good)
		}
	}
	invalid := []string{
		"0.7.1",
		"v1",
		"v1.2",
		"v1.2-rc.1",
		"v1.2+build.1",
		"not-semver",
		"v1.2.3.4",
		"v01.2.3",
		"v1.02.3",
		"v1.2.03",
		"v1.2.3-01",
		"v1.2.3-rc.01",
		"v1.2.3-",
		"v1.2.3-rc..1",
		"v1.2.3+",
		"v1.2.3+build..1",
	}
	for _, bad := range invalid {
		if validVersion(bad) {
			t.Fatalf("validVersion(%q) = true, want false", bad)
		}

		o := newOpsctl(t, root, app, &stubSystem{}, fakeEnv(app, "v1.2.3", 1, ""))
		artifact := filepath.Join(t.TempDir(), "missing-artifact")
		if err := o.Stage(ctx, app, bad, artifact, false); err == nil || !strings.Contains(err.Error(), "invalid version") {
			t.Fatalf("Stage(%q) err = %v, want invalid version refusal", bad, err)
		}
		if err := o.Deploy(ctx, app, bad); err == nil || !strings.Contains(err.Error(), "invalid version") {
			t.Fatalf("Deploy(%q) err = %v, want invalid version refusal", bad, err)
		}
		if err := o.Rollback(ctx, app, bad); err == nil || !strings.Contains(err.Error(), "invalid target version") {
			t.Fatalf("Rollback(%q) err = %v, want invalid target version refusal", bad, err)
		}
		if err := o.preflight(ctx, artifact, app, bad); err == nil || !strings.Contains(err.Error(), "invalid version") {
			t.Fatalf("preflight version arg %q err = %v, want invalid version refusal", bad, err)
		}

		artifact = stageArtifact(t, "ledger-"+strings.NewReplacer("/", "_").Replace(bad))
		o = newOpsctl(t, root, app, &stubSystem{}, fakeEnv(app, bad, 1, ""))
		if err := o.preflight(ctx, artifact, app, "v1.2.3"); err == nil || !strings.Contains(err.Error(), "self-reports invalid version") {
			t.Fatalf("preflight self-report %q err = %v, want invalid self-report refusal", bad, err)
		}
	}
}

func TestReleaseIdentityBoundariesRejectInvalidSemVer(t *testing.T) {
	// R-439X-OQXO
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	o := &Opsctl{Root: root}

	if err := os.MkdirAll(l.LibexecDir(), 0o755); err != nil {
		t.Fatalf("mkdir libexec: %v", err)
	}
	if err := os.WriteFile(l.LibexecBinary("v1.0.0"), []byte("ok"), 0o755); err != nil {
		t.Fatalf("write valid release: %v", err)
	}
	if err := os.WriteFile(filepath.Join(l.LibexecDir(), app+"-v1"), []byte("bad"), 0o755); err != nil {
		t.Fatalf("write invalid release: %v", err)
	}
	if _, err := o.listReleases(l); err == nil || !strings.Contains(err.Error(), "invalid release version \"v1\"") {
		t.Fatalf("listReleases invalid dir err = %v, want invalid release version refusal", err)
	}

	if err := os.MkdirAll(filepath.Dir(l.RunLink()), 0o755); err != nil {
		t.Fatalf("mkdir run dir: %v", err)
	}
	if err := os.Symlink(filepath.Join("..", "libexec", app+"-v1.2"), l.RunLink()); err != nil {
		t.Fatalf("symlink invalid run target: %v", err)
	}
	if _, err := o.currentVersion(l); err == nil || !strings.Contains(err.Error(), "invalid current version \"v1.2\"") {
		t.Fatalf("currentVersion invalid target err = %v, want invalid version refusal", err)
	}
}

func TestInvalidVersionErrorsDescribeAcceptedSemVerShape(t *testing.T) {
	// R-439X-OQXO
	o := &Opsctl{}
	err := o.Deploy(context.Background(), "ledger", "v1")
	if err == nil {
		t.Fatal("Deploy(v1) err = nil, want invalid-version refusal")
	}
	for _, want := range []string{"invalid version \"v1\"", versionShape, "SemVer 2.0"} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("Deploy(v1) err = %q, want it to include %q", err, want)
		}
	}
}
