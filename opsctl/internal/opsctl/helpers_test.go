package opsctl

import (
	"context"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"testing"
)

// stubSystem records restart calls and lets a test force is-active to fail, so
// the systemd seam runs with no real systemd (PLAN §C2). It also captures, on
// each Restart, the version `current` resolves to — the assertion hook proving
// the symlink only ever points at a complete release at the moment the unit (re)
// starts (atomic-swap invariant).
type stubSystem struct {
	mu             sync.Mutex
	restarts       int
	failIsActive   bool
	activeState    string   // state IsActiveState reports (default "active" if "")
	currentLink    string   // /opt/<app>/current — read on each Restart
	seenAtRestart  []string // current's target version observed at each restart
	binExistsAtRun []bool   // whether current/<app> existed (complete release) at restart
	app            string

	// Provisioning-op recorder (D1): every privileged op init-box / setup invoke
	// through the seam is recorded here (and never executed), so a test can assert
	// the box ops were REQUESTED, in order, without root.
	ops []string
}

func (s *stubSystem) record(op string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.ops = append(s.ops, op)
}

func (s *stubSystem) InstallPackages(ctx context.Context, pkgs ...string) error {
	s.record("install-packages:" + strings.Join(pkgs, ","))
	return nil
}

func (s *stubSystem) EnsureSystemUser(ctx context.Context, app, homeDir string) error {
	s.record("ensure-user:" + app + ":" + homeDir)
	return nil
}

func (s *stubSystem) ChownTree(ctx context.Context, owner, group, path string) error {
	s.record("chown:" + owner + ":" + group + ":" + path)
	return nil
}

func (s *stubSystem) DaemonReload(ctx context.Context) error {
	s.record("daemon-reload")
	return nil
}

func (s *stubSystem) EnableUnit(ctx context.Context, unit string, now bool) error {
	if now {
		s.record("enable-now:" + unit)
	} else {
		s.record("enable:" + unit)
	}
	return nil
}

func (s *stubSystem) NginxTest(ctx context.Context) error {
	s.record("nginx-test")
	return nil
}

func (s *stubSystem) NginxReload(ctx context.Context) error {
	s.record("nginx-reload")
	return nil
}

func (s *stubSystem) ObtainCert(ctx context.Context, domain, email, webroot string) error {
	s.record("obtain-cert:" + domain)
	return nil
}

// opSeq returns the recorded provisioning ops as a single comma-joined string for
// easy assertion.
func (s *stubSystem) opSeq() []string {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]string(nil), s.ops...)
}

func (s *stubSystem) Restart(ctx context.Context, app string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.restarts++
	// Observe what current points at right now — the launcher would exec
	// current/<app> here, so it must resolve to a complete release.
	if s.currentLink != "" {
		if dst, err := os.Readlink(s.currentLink); err == nil {
			s.seenAtRestart = append(s.seenAtRestart, filepath.Base(dst))
			bin := filepath.Join(s.currentLink, app) // resolves through the symlink
			_, statErr := os.Stat(bin)
			s.binExistsAtRun = append(s.binExistsAtRun, statErr == nil)
		} else {
			s.seenAtRestart = append(s.seenAtRestart, "<none>")
			s.binExistsAtRun = append(s.binExistsAtRun, false)
		}
	}
	return nil
}

func (s *stubSystem) IsActive(ctx context.Context, app string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.failIsActive {
		return &exec.ExitError{}
	}
	return nil
}

func (s *stubSystem) IsActiveState(ctx context.Context, app string) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.activeState != "" {
		return s.activeState, nil
	}
	return "active", nil
}

func (s *stubSystem) Systemctl(ctx context.Context, args ...string) error {
	s.record("systemctl:" + strings.Join(args, " "))
	return nil
}

func (s *stubSystem) Journalctl(ctx context.Context, args ...string) error {
	s.record("journalctl:" + strings.Join(args, " "))
	return nil
}

// fakeRunner execs the compiled fakeapp binary, injecting a fixed base env (the
// FAKE_* knobs that parameterise the scenario) plus opsctl's per-verb env
// overrides. It mirrors RealRunner but with a controllable base env so a test can
// set the binary's self-reported version, embedded schema, and manifest body.
type fakeRunner struct {
	baseEnv []string // FAKE_VERSION=…, FAKE_EMBEDDED=…, FAKE_MANIFEST=…, FAKE_APP=…
	// commitByPath overrides FAKE_COMMIT per binary path, so the stage collision
	// guard can be exercised: a real binary self-reports its OWN ldflag-stamped
	// commit regardless of env, so the already-placed release and the incoming
	// artifact (distinct paths) can report different SHAs. A path absent here falls
	// back to the baseEnv FAKE_COMMIT (if any). Keyed by absolute path basename via
	// exact path match.
	commitByPath map[string]string
}

func (r fakeRunner) Run(ctx context.Context, binary, verb string, args []string, env []string) (string, error) {
	full := append([]string{verb}, args...)
	cmd := exec.CommandContext(ctx, binary, full...)
	cmd.Env = append(append(os.Environ(), r.baseEnv...), env...)
	if c, ok := r.commitByPath[binary]; ok {
		cmd.Env = append(cmd.Env, "FAKE_COMMIT="+c)
	}
	var stdout, stderr strings.Builder
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Run(); err != nil {
		return stdout.String(), &runErr{verb: verb, stderr: stderr.String(), err: err}
	}
	return stdout.String(), nil
}

type runErr struct {
	verb, stderr string
	err          error
}

func (e *runErr) Error() string {
	return e.verb + ": " + e.err.Error() + ": " + strings.TrimSpace(e.stderr)
}

// buildFakeApp compiles testdata/fakeapp into a static linux/amd64 binary placed
// at dst (the contract's required shape, so opsctl's ELF preflight passes). It is
// built once per test process and reused.
var (
	fakeOnce sync.Once
	fakePath string
	fakeErr  error
)

func compileFakeApp(t *testing.T) string {
	t.Helper()
	fakeOnce.Do(func() {
		dir := t.TempDir()
		// t.TempDir is per-test; keep the built binary in the package's own temp so
		// it survives across tests in this process.
		out, err := os.MkdirTemp("", "opsctl-fakeapp-*")
		if err != nil {
			fakeErr = err
			return
		}
		bin := filepath.Join(out, "fakeapp")
		src, err := filepath.Abs(filepath.Join("testdata", "fakeapp"))
		if err != nil {
			fakeErr = err
			return
		}
		cmd := exec.Command("go", "build", "-o", bin, ".")
		cmd.Dir = src
		cmd.Env = append(os.Environ(),
			"CGO_ENABLED=0", "GOOS=linux", "GOARCH=amd64", "GOWORK=off", "GOFLAGS=-buildvcs=false")
		if b, berr := cmd.CombinedOutput(); berr != nil {
			fakeErr = &runErr{verb: "build fakeapp", stderr: string(b), err: berr}
			return
		}
		fakePath = bin
		_ = dir
	})
	if fakeErr != nil {
		t.Fatalf("compile fakeapp: %v", fakeErr)
	}
	return fakePath
}

// stageArtifact copies the compiled fakeapp to a uniquely-named artifact path (as
// a deploy would scp it to /tmp), so each install consumes a distinct source.
func stageArtifact(t *testing.T, name string) string {
	t.Helper()
	src := compileFakeApp(t)
	dst := filepath.Join(t.TempDir(), name)
	b, err := os.ReadFile(src)
	if err != nil {
		t.Fatalf("read fakeapp: %v", err)
	}
	if err := os.WriteFile(dst, b, 0o755); err != nil {
		t.Fatalf("stage artifact: %v", err)
	}
	return dst
}
