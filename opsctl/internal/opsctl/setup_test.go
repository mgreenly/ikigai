package opsctl

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
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
		l.AppDir(),
		l.StateDir(),
		l.WWWDir(),
		l.CacheDir(),
		l.LibexecDir(),
		l.BinDir(),
		l.EtcDir(),
		l.shareDir(),
	} {
		if fi, err := os.Stat(dir); err != nil || !fi.IsDir() {
			t.Fatalf("directory %s not materialized: %v", dir, err)
		}
	}
	if _, err := os.Stat(l.BackupsDir()); !os.IsNotExist(err) {
		t.Fatalf("setup created backups dir %s, want absent (err=%v)", l.BackupsDir(), err)
	}

	assertMode(t, l.StateDir(), 0o711)
	assertMode(t, l.DBPath(), 0o640)
	assertMode(t, l.WWWDir(), 0o750)
	assertMode(t, l.WWWPublicDir(), 0o750)
	assertMode(t, l.WWWPrivateDir(), 0o750)

	if err := os.WriteFile(filepath.Join(l.CacheDir(), "probe"), []byte("ok"), 0o644); err != nil {
		t.Fatalf("cache dir is not writable: %v", err)
	}

	wantOps := []string{
		"chown:" + app + ":" + app + ":" + l.StateDir(),
		"chown:" + app + ":" + app + ":" + l.DBPath(),
		"chown:" + app + ":web:" + l.WWWDir(),
		"ensure-group:web",
	}
	for _, want := range wantOps {
		if !hasOp(sys.opSeq(), want) {
			t.Fatalf("setup ownership ops = %v, missing %q", sys.opSeq(), want)
		}
	}

	owners := ownershipPlan(sys.opSeq())
	for _, rootOwned := range []string{l.EtcDir(), l.shareDir()} {
		if got := ownerForPath(owners, rootOwned); got != (ownerGroup{}) {
			t.Fatalf("%s was handed to %s:%s through Owner seam, want root-owned", rootOwned, got.owner, got.group)
		}
	}
}

// R-LHY1-6IS8
func TestSetupCreatesStableNginxSymlinkToActiveConfig(t *testing.T) {
	const app = "svc"
	o, _, l := newSetupTestOpsctl(t, app)

	if err := o.Setup(context.Background(), SetupOptions{App: app}); err != nil {
		t.Fatalf("setup: %v", err)
	}

	fi, err := os.Lstat(l.FragmentPath())
	if err != nil {
		t.Fatalf("lstat fragment symlink: %v", err)
	}
	if fi.Mode()&os.ModeSymlink == 0 {
		t.Fatalf("%s mode = %v, want symlink", l.FragmentPath(), fi.Mode())
	}
	target, err := os.Readlink(l.FragmentPath())
	if err != nil {
		t.Fatalf("readlink fragment symlink: %v", err)
	}
	if target != l.ActiveNginxConf() {
		t.Fatalf("fragment symlink target = %q, want %q", target, l.ActiveNginxConf())
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

	if !web.canRead(statMode(t, l.WWWPublicDir()), ownerForPath(owners, l.WWWPublicDir())) ||
		!web.canList(statMode(t, l.WWWPublicDir()), ownerForPath(owners, l.WWWPublicDir())) {
		t.Fatalf("web group cannot read/list public www dir under modeled Unix mode/owner semantics")
	}
	if !web.canRead(statMode(t, l.WWWPrivateDir()), ownerForPath(owners, l.WWWPrivateDir())) ||
		!web.canList(statMode(t, l.WWWPrivateDir()), ownerForPath(owners, l.WWWPrivateDir())) {
		t.Fatalf("web group cannot read/list private www dir under modeled Unix mode/owner semantics")
	}
	if web.canRead(statMode(t, l.DBPath()), ownerForPath(owners, l.DBPath())) {
		t.Fatalf("web group can read %s; want DB denied by mode/owner model", l.DBPath())
	}
	if web.canList(statMode(t, l.StateDir()), ownerForPath(owners, l.StateDir())) {
		t.Fatalf("web group can list %s; want state dir listing denied by mode/owner model", l.StateDir())
	}
}

// R-4LKF-FB23
func TestWebhooksSetupDeployBootsHealthWithStateCacheAndLibexecPaths(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	const app = "webhooks"
	const version = "v0.1.0"
	l := NewLayoutSys(root, sysRoot, app)
	if err := os.MkdirAll(l.LocationsDir(), 0o755); err != nil {
		t.Fatalf("create locations dir: %v", err)
	}

	sys := &webhooksBootSystem{
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

	fragment := "location /srv/webhooks/ {\n    proxy_pass http://127.0.0.1:__PORT__;\n}\n"
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3011, Fragment: fragment}); err != nil {
		t.Fatalf("setup webhooks: %v", err)
	}

	artifact := buildWebhooksArtifact(t, version)
	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage webhooks: %v", err)
	}
	if err := o.Deploy(context.Background(), app, version); err != nil {
		t.Fatalf("deploy webhooks: %v", err)
	}

	if _, err := os.Stat(l.DBPath()); err != nil {
		t.Fatalf("webhooks DB was not created under state/: %v", err)
	}
	if got := l.DBPath(); got != filepath.Join(l.StateDir(), "webhooks.db") {
		t.Fatalf("webhooks DB path = %q, want state/webhooks.db", got)
	}
	if got := l.GenerationPath(); got != filepath.Join(l.CacheDir(), "webhooks.db.generation") {
		t.Fatalf("webhooks generation sidecar path = %q, want cache/webhooks.db.generation", got)
	}
	if _, err := os.Stat(l.LibexecBinary(version)); err != nil {
		t.Fatalf("webhooks binary missing under libexec/: %v", err)
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
		"APP=webhooks",
		"WEBHOOKS_DB_PATH=" + l.DBPath(),
		"WEBHOOKS_GENERATION_PATH=" + l.GenerationPath(),
	} {
		if !strings.Contains(string(manifest), want) {
			t.Fatalf("manifest missing %q\n--- manifest ---\n%s", want, manifest)
		}
	}
	if !strings.Contains(sys.healthBody, `"status":"ok"`) || !strings.Contains(sys.healthBody, `"service":"webhooks"`) {
		t.Fatalf("health response = %s, want webhooks ok envelope", sys.healthBody)
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

type webhooksBootSystem struct {
	*stubSystem
	t          *testing.T
	layout     Layout
	port       int
	healthBody string
	started    bool
}

func (s *webhooksBootSystem) IsActive(ctx context.Context, app string) error {
	if s.started {
		return nil
	}
	runCtx, cancel := context.WithCancel(context.Background())
	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(runCtx, s.layout.RunLink(), "serve")
	cmd.Env = append(os.Environ(), manifestEnv(s.layout.ManifestPath())...)
	cmd.Env = append(cmd.Env,
		fmt.Sprintf("WEBHOOKS_PORT=%d", s.port),
		"WEBHOOKS_IP=127.0.0.1",
		"WEBHOOKS_RESOURCE_ID=http://127.0.0.1/srv/webhooks/mcp",
		"WEBHOOKS_AUTH_SERVER=http://127.0.0.1",
	)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("start webhooks serve: %w", err)
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
			return fmt.Errorf("webhooks serve exited before health: %w\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		case <-deadline:
			cancel()
			return fmt.Errorf("webhooks did not reach /health\nstdout:\n%s\nstderr:\n%s", stdout.String(), stderr.String())
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

func buildWebhooksArtifact(t *testing.T, version string) string {
	t.Helper()
	out := filepath.Join(t.TempDir(), "webhooks")
	webhooksDir, err := filepath.Abs(filepath.Join("..", "..", "..", "webhooks"))
	if err != nil {
		t.Fatalf("resolve webhooks dir: %v", err)
	}
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X appkit.version="+version+" -X appkit.commit=abcdef0", "-o", out, "./cmd/webhooks")
	cmd.Dir = webhooksDir
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOFLAGS=-buildvcs=false")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build webhooks artifact: %v\n%s", err, b)
	}
	return bundleArtifactFromBinary(t, "webhooks", version, "webhooks-"+version, out)
}
