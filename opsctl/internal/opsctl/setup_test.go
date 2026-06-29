package opsctl

import (
	"context"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func newSetupTestOpsctl(t *testing.T, app string) (*Opsctl, *stubSystem, Layout) {
	t.Helper()

	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		System:  sys,
		Out:     io.Discard,
		Err:     io.Discard,
	}
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}
	return o, sys, l
}

// R-3SAU-8T9F
func TestSetupMaterializesInstallTreeWithPermissionsAndOwnership(t *testing.T) {
	const app = "svc"
	o, sys, l := newSetupTestOpsctl(t, app)

	if err := o.Setup(context.Background(), SetupOptions{App: app}); err != nil {
		t.Fatalf("setup: %v", err)
	}

	for _, dir := range []string{
		l.StateDir(),
		l.CacheDir(),
		l.LibexecDir(),
		l.BinDir(),
		l.EtcDir(),
		l.BackupsDir(),
	} {
		if fi, err := os.Stat(dir); err != nil || !fi.IsDir() {
			t.Fatalf("directory %s not materialized: %v", dir, err)
		}
	}

	assertMode(t, l.StateDir(), 0o711)
	assertMode(t, l.DBPath(), 0o640)
	assertMode(t, l.WWWPublicDir(), 0o750)
	assertMode(t, l.WWWPrivateDir(), 0o750)

	if err := os.WriteFile(filepath.Join(l.CacheDir(), "probe"), []byte("ok"), 0o644); err != nil {
		t.Fatalf("cache dir is not writable: %v", err)
	}

	wantOps := []string{
		"chown:" + app + ":" + app + ":" + l.StateDir(),
		"chown:" + app + ":" + app + ":" + l.DBPath(),
		"chown:" + app + ":web:" + l.WWWDir(),
	}
	for _, want := range wantOps {
		if !hasOp(sys.opSeq(), want) {
			t.Fatalf("setup ownership ops = %v, missing %q", sys.opSeq(), want)
		}
	}
}

// R-VB77-BU5O
func TestSetupPermissionModelAllowsWebGroupOnlyForWWW(t *testing.T) {
	const app = "svc"
	o, sys, l := newSetupTestOpsctl(t, app)

	if err := o.Setup(context.Background(), SetupOptions{App: app}); err != nil {
		t.Fatalf("setup: %v", err)
	}

	owners := ownershipPlan(sys.opSeq())
	web := unixSubject{user: "nginx", groups: map[string]bool{"web": true}}

	if !web.canList(statMode(t, l.WWWPublicDir()), ownerForPath(owners, l.WWWPublicDir())) {
		t.Fatalf("web group cannot read/list public www dir under modeled Unix mode/owner semantics")
	}
	if !web.canList(statMode(t, l.WWWPrivateDir()), ownerForPath(owners, l.WWWPrivateDir())) {
		t.Fatalf("web group cannot read/list private www dir under modeled Unix mode/owner semantics")
	}
	if web.canRead(statMode(t, l.DBPath()), ownerForPath(owners, l.DBPath())) {
		t.Fatalf("web group can read %s; want DB denied by mode/owner model", l.DBPath())
	}
	if web.canList(statMode(t, l.StateDir()), ownerForPath(owners, l.StateDir())) {
		t.Fatalf("web group can list %s; want state dir listing denied by mode/owner model", l.StateDir())
	}
}

func assertMode(t *testing.T, path string, want os.FileMode) {
	t.Helper()
	if got := statMode(t, path).Perm(); got != want {
		t.Fatalf("%s mode = %o, want %o", path, got, want)
	}
}

func statMode(t *testing.T, path string) os.FileMode {
	t.Helper()
	fi, err := os.Stat(path)
	if err != nil {
		t.Fatalf("stat %s: %v", path, err)
	}
	return fi.Mode()
}

func hasOp(ops []string, want string) bool {
	for _, op := range ops {
		if op == want {
			return true
		}
	}
	return false
}

type ownerGroup struct {
	owner string
	group string
}

func ownershipPlan(ops []string) map[string]ownerGroup {
	owners := make(map[string]ownerGroup)
	for _, op := range ops {
		parts := strings.SplitN(op, ":", 4)
		if len(parts) != 4 || parts[0] != "chown" {
			continue
		}
		owners[parts[3]] = ownerGroup{owner: parts[1], group: parts[2]}
	}
	return owners
}

func ownerForPath(owners map[string]ownerGroup, path string) ownerGroup {
	var (
		match string
		owner ownerGroup
	)
	for prefix, candidate := range owners {
		if path == prefix || strings.HasPrefix(path, prefix+string(os.PathSeparator)) {
			if len(prefix) > len(match) {
				match = prefix
				owner = candidate
			}
		}
	}
	return owner
}

type unixSubject struct {
	user   string
	groups map[string]bool
}

func (s unixSubject) canRead(mode os.FileMode, owner ownerGroup) bool {
	return s.permissionBits(mode, owner)&0o4 == 0o4
}

func (s unixSubject) canList(mode os.FileMode, owner ownerGroup) bool {
	return s.permissionBits(mode, owner)&0o5 == 0o5
}

func (s unixSubject) permissionBits(mode os.FileMode, owner ownerGroup) os.FileMode {
	perm := mode.Perm()
	switch {
	case s.user == owner.owner:
		return (perm >> 6) & 0o7
	case s.groups[owner.group]:
		return (perm >> 3) & 0o7
	default:
		return perm & 0o7
	}
}
