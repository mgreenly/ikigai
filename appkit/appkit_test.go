package appkit

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
	"time"

	"appkit/internal/testmigrations"
)

// testSpec is a producer-shaped spec over the shared test migrations, used to
// drive the dispatcher.
func testSpec() Spec {
	return Spec{
		App:        "widget",
		Mount:      "/srv/widget/",
		Port:       3099,
		MCP:        true,
		Feed:       "/feed",
		Migrations: testmigrations.FS,
		ManifestExtras: []ManifestKV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
		},
	}
}

// run drives the testable dispatch core with empty env unless overridden.
func run(t *testing.T, spec Spec, env map[string]string, args ...string) (code int, stdout, stderr string) {
	t.Helper()
	var out, errb bytes.Buffer
	getenv := func(k string) string {
		if env == nil {
			return ""
		}
		return env[k]
	}
	code = dispatch(spec, args, getenv, strings.NewReader(""), &out, &errb)
	return code, out.String(), errb.String()
}

func TestDispatch_Version(t *testing.T) {
	code, out, _ := run(t, testSpec(), nil, "version")
	if code != 0 {
		t.Fatalf("version exit = %d, want 0", code)
	}
	// Default un-stamped build self-reports "dev (none)" -> "dev".
	if !strings.Contains(out, version) {
		t.Errorf("version output %q does not contain %q", out, version)
	}
}

func TestDispatch_VersionFlagAlias(t *testing.T) {
	code, out, _ := run(t, testSpec(), nil, "--version")
	if code != 0 {
		t.Fatalf("--version exit = %d, want 0", code)
	}
	if strings.TrimSpace(out) == "" {
		t.Error("--version produced no output")
	}
}

func TestDispatch_Manifest_ByteForm(t *testing.T) {
	code, out, _ := run(t, testSpec(), nil, "manifest")
	if code != 0 {
		t.Fatalf("manifest exit = %d, want 0", code)
	}
	want := "APP=widget\nMOUNT=/srv/widget/\nDEFAULT=false\nPORT=3099\nMCP=true\nFEED=/feed\n" +
		"OUTBOX_RETENTION_DAYS=7\n"
	if out != want {
		t.Fatalf("manifest emit\n got: %q\nwant: %q", out, want)
	}
}

func TestDispatch_Manifest_Consumer(t *testing.T) {
	spec := Spec{App: "notify", Mount: "/srv/notify/", Port: 3003, MCP: true, Consumes: []string{"crm"}, Migrations: testmigrations.FS}
	_, out, _ := run(t, spec, nil, "manifest")
	want := "APP=notify\nMOUNT=/srv/notify/\nDEFAULT=false\nPORT=3003\nMCP=true\nCONSUMES=crm\n"
	if out != want {
		t.Fatalf("consumer manifest\n got: %q\nwant: %q", out, want)
	}
}

func TestDispatch_Migrate(t *testing.T) {
	dbPath := filepath.Join(t.TempDir(), "widget.db")
	env := map[string]string{"WIDGET_DB_PATH": dbPath}
	code, out, errs := run(t, testSpec(), env, "migrate")
	if code != 0 {
		t.Fatalf("migrate exit = %d, stderr=%q", code, errs)
	}
	if !strings.Contains(out, "version 2") {
		t.Errorf("migrate output = %q, want it to report version 2", out)
	}
}

func TestDispatch_Schema(t *testing.T) {
	dir := t.TempDir()
	dbPath := filepath.Join(dir, "widget.db")
	env := map[string]string{"WIDGET_DB_PATH": dbPath}

	// Before any DB exists: applied=0 (a brand-new app's first install), embedded
	// = the binary's max migration. This is the schema-advance signal opsctl reads.
	code, out, errs := run(t, testSpec(), env, "schema")
	if code != 0 {
		t.Fatalf("schema (no db) exit = %d, stderr=%q", code, errs)
	}
	if strings.TrimSpace(out) != "applied=0 embedded=2" {
		t.Fatalf("schema (no db) = %q, want %q", strings.TrimSpace(out), "applied=0 embedded=2")
	}

	// After migrate: applied catches up to embedded (no further advance).
	if code, _, errs := run(t, testSpec(), env, "migrate"); code != 0 {
		t.Fatalf("setup migrate: exit %d, %q", code, errs)
	}
	code, out, errs = run(t, testSpec(), env, "schema")
	if code != 0 {
		t.Fatalf("schema (migrated) exit = %d, stderr=%q", code, errs)
	}
	if strings.TrimSpace(out) != "applied=2 embedded=2" {
		t.Fatalf("schema (migrated) = %q, want %q", strings.TrimSpace(out), "applied=2 embedded=2")
	}
}

func TestDispatch_BackupThenRestore(t *testing.T) {
	dir := t.TempDir()
	dbPath := filepath.Join(dir, "widget.db")
	env := map[string]string{"WIDGET_DB_PATH": dbPath}

	// Establish a DB first.
	if code, _, errs := run(t, testSpec(), env, "migrate"); code != 0 {
		t.Fatalf("setup migrate: exit %d, %q", code, errs)
	}
	backupPath := filepath.Join(dir, "snap.db")
	if code, _, errs := run(t, testSpec(), env, "backup", "--out", backupPath); code != 0 {
		t.Fatalf("backup: exit %d, %q", code, errs)
	}
	if code, out, errs := run(t, testSpec(), env, "restore", "--from", backupPath); code != 0 {
		t.Fatalf("restore: exit %d, %q", code, errs)
	} else if !strings.Contains(out, "restored widget") {
		t.Errorf("restore output = %q", out)
	}
	// The restored DB must still be a valid migrated DB.
	if code, out, errs := run(t, testSpec(), env, "migrate"); code != 0 {
		t.Fatalf("post-restore migrate: exit %d, %q", code, errs)
	} else if !strings.Contains(out, "version 2") {
		t.Errorf("post-restore migrate output = %q", out)
	}
}

// TestDispatch_RestoreReMintsEpoch is the regression test for
// docs/bug-rollback-epoch-remint.md: a restore through the dispatch path must
// re-mint the event-plane epoch by removing the <db>.generation sidecar, so a
// post-restore boot mints a fresh generation and pre-restore consumer cursors
// are rejected with stale-epoch instead of resuming onto reused seqs.
func TestDispatch_RestoreReMintsEpoch(t *testing.T) {
	dir := t.TempDir()
	dbPath := filepath.Join(dir, "widget.db")
	genPath := dbPath + ".generation"
	env := map[string]string{"WIDGET_DB_PATH": dbPath}

	// Establish a DB and a snapshot to restore from.
	if code, _, errs := run(t, testSpec(), env, "migrate"); code != 0 {
		t.Fatalf("setup migrate: exit %d, %q", code, errs)
	}
	backupPath := filepath.Join(dir, "snap.db")
	if code, _, errs := run(t, testSpec(), env, "backup", "--out", backupPath); code != 0 {
		t.Fatalf("backup: exit %d, %q", code, errs)
	}

	// Pre-seed the generation sidecar, as a live producer would carry.
	if err := os.WriteFile(genPath, []byte("GEN_A\n"), 0o644); err != nil {
		t.Fatalf("seed sidecar: %v", err)
	}

	code, out, errs := run(t, testSpec(), env, "restore", "--from", backupPath)
	if code != 0 {
		t.Fatalf("restore: exit %d, %q", code, errs)
	}
	if !strings.Contains(out, "re-minted event-plane epoch") {
		t.Errorf("restore output = %q, want a re-mint line", out)
	}
	if _, err := os.Stat(genPath); !os.IsNotExist(err) {
		t.Fatalf("generation sidecar still present after restore (stat err = %v); epoch not re-minted", err)
	}
}

// TestDispatch_RestoreNoSidecar covers a non-producer (or never-booted producer)
// where no generation sidecar exists: restore must still succeed (the absent
// sidecar's ErrNotExist is ignored).
func TestDispatch_RestoreNoSidecar(t *testing.T) {
	dir := t.TempDir()
	dbPath := filepath.Join(dir, "widget.db")
	env := map[string]string{"WIDGET_DB_PATH": dbPath}

	if code, _, errs := run(t, testSpec(), env, "migrate"); code != 0 {
		t.Fatalf("setup migrate: exit %d, %q", code, errs)
	}
	backupPath := filepath.Join(dir, "snap.db")
	if code, _, errs := run(t, testSpec(), env, "backup", "--out", backupPath); code != 0 {
		t.Fatalf("backup: exit %d, %q", code, errs)
	}

	// No sidecar pre-seeded.
	if code, _, errs := run(t, testSpec(), env, "restore", "--from", backupPath); code != 0 {
		t.Fatalf("restore (no sidecar): exit %d, %q", code, errs)
	}
}

// TestDispatch_RestoreOverrideStillReMints proves the chokepoint guarantee: even
// when a Spec.Restore override fully replaces defaultRestore, the dispatcher
// (runRestore) still removes the generation sidecar. The hook here only touches
// the DB and returns nil; the re-mint is the verb's job, not the hook's.
func TestDispatch_RestoreOverrideStillReMints(t *testing.T) {
	dir := t.TempDir()
	dbPath := filepath.Join(dir, "widget.db")
	genPath := dbPath + ".generation"
	env := map[string]string{"WIDGET_DB_PATH": dbPath}

	if code, _, errs := run(t, testSpec(), env, "migrate"); code != 0 {
		t.Fatalf("setup migrate: exit %d, %q", code, errs)
	}
	if err := os.WriteFile(genPath, []byte("GEN_A\n"), 0o644); err != nil {
		t.Fatalf("seed sidecar: %v", err)
	}

	called := false
	spec := testSpec()
	spec.Restore = func(ctx context.Context, req RestoreReq) error {
		called = true
		// A trivial hook: touch the DB so the restore "did something", return nil.
		return os.WriteFile(req.DBPath, []byte("restored"), 0o644)
	}

	if code, _, errs := run(t, spec, env, "restore"); code != 0 {
		t.Fatalf("override restore: exit %d, %q", code, errs)
	}
	if !called {
		t.Fatal("Spec.Restore override was not invoked")
	}
	if _, err := os.Stat(genPath); !os.IsNotExist(err) {
		t.Fatalf("sidecar present after override restore (stat err = %v); dispatcher did not re-mint", err)
	}
}

func TestDispatch_DefaultBackupOverride(t *testing.T) {
	called := false
	spec := testSpec()
	spec.Backup = func(ctx context.Context, req BackupReq) error {
		called = true
		if req.App != "widget" {
			t.Errorf("backup req App = %q", req.App)
		}
		return nil
	}
	if code, _, errs := run(t, spec, nil, "backup"); code != 0 {
		t.Fatalf("override backup exit %d, %q", code, errs)
	}
	if !called {
		t.Error("Spec.Backup override was not invoked")
	}
}

func TestDispatch_UnknownCommand(t *testing.T) {
	code, _, errs := run(t, testSpec(), nil, "bogus")
	if code != 2 {
		t.Fatalf("unknown command exit = %d, want 2", code)
	}
	if !strings.Contains(errs, "unknown command") {
		t.Errorf("stderr = %q, want an unknown-command message", errs)
	}
}

func TestDispatch_MissingApp(t *testing.T) {
	code, _, errs := run(t, Spec{}, nil, "version")
	if code != 1 {
		t.Fatalf("missing App exit = %d, want 1", code)
	}
	if !strings.Contains(errs, "Spec.App is required") {
		t.Errorf("stderr = %q", errs)
	}
}

func TestDispatch_ServeRejectsBadLogLevel(t *testing.T) {
	// The serve path resolves config, then validates the log level before binding
	// a socket; a bad level fails fast with exit 1 and never opens a listener.
	dbPath := filepath.Join(t.TempDir(), "widget.db")
	env := map[string]string{"WIDGET_DB_PATH": dbPath, "WIDGET_LOG_LEVEL": "screaming"}
	code, _, errs := run(t, testSpec(), env, "serve")
	if code != 1 {
		t.Fatalf("serve bad log level exit = %d, want 1", code)
	}
	if !strings.Contains(errs, "invalid log level") {
		t.Errorf("stderr = %q, want an invalid-log-level error", errs)
	}
}

func TestDispatch_ServeReconstructsCacheAndGenerationOnBoot(t *testing.T) {
	// R-4E91-4OLX
	root := t.TempDir()
	appRoot := filepath.Join(root, "opt", "widget")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	if err := os.MkdirAll(stateDir, 0o750); err != nil {
		t.Fatalf("create state dir: %v", err)
	}

	dbPath := filepath.Join(stateDir, "widget.db")
	genPath := filepath.Join(cacheDir, "widget.db.generation")
	if _, err := os.Stat(cacheDir); !os.IsNotExist(err) {
		t.Fatalf("cache dir exists before boot (stat err = %v)", err)
	}
	if _, err := os.Stat(genPath); !os.IsNotExist(err) {
		t.Fatalf("generation sidecar exists before boot (stat err = %v)", err)
	}

	port := freeLoopbackPort(t)
	addr := net.JoinHostPort("127.0.0.1", strconv.Itoa(port))
	reachedHealth := false
	spec := testSpec()
	spec.Workers = []func(context.Context) error{
		func(ctx context.Context) error {
			ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
			defer cancel()
			if err := waitForHealth(ctx, addr, "widget"); err != nil {
				return err
			}
			reachedHealth = true
			return nil
		},
	}

	env := map[string]string{
		"WIDGET_DB_PATH":         dbPath,
		"WIDGET_GENERATION_PATH": genPath,
		"WIDGET_PORT":            strconv.Itoa(port),
	}
	code, _, errs := run(t, spec, env, "serve")
	if code != 0 {
		t.Fatalf("serve exit = %d, stderr=%q", code, errs)
	}
	if !reachedHealth {
		t.Fatal("serve returned without reaching /health")
	}
	if info, err := os.Stat(cacheDir); err != nil || !info.IsDir() {
		t.Fatalf("cache dir after boot = (%v, %v), want directory", info, err)
	}
	generation, err := os.ReadFile(genPath)
	if err != nil {
		t.Fatalf("read generation sidecar: %v", err)
	}
	if strings.TrimSpace(string(generation)) == "" {
		t.Fatal("generation sidecar is empty")
	}
	if info, err := os.Stat(dbPath); err != nil || info.IsDir() {
		t.Fatalf("db file after boot = (%v, %v), want file", info, err)
	}
}

func freeLoopbackPort(t *testing.T) int {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("allocate loopback port: %v", err)
	}
	defer ln.Close()
	return ln.Addr().(*net.TCPAddr).Port
}

func waitForHealth(ctx context.Context, addr, service string) error {
	client := &http.Client{Timeout: 200 * time.Millisecond}
	tick := time.NewTicker(25 * time.Millisecond)
	defer tick.Stop()
	var lastErr error
	for {
		req, err := http.NewRequestWithContext(ctx, http.MethodGet, "http://"+addr+"/health", nil)
		if err != nil {
			return err
		}
		resp, err := client.Do(req)
		if err == nil {
			var body map[string]any
			decodeErr := json.NewDecoder(resp.Body).Decode(&body)
			closeErr := resp.Body.Close()
			if resp.StatusCode == http.StatusOK && decodeErr == nil && closeErr == nil &&
				body["status"] == "ok" && body["service"] == service {
				return nil
			}
			lastErr = fmt.Errorf("health response status=%d body=%v decode=%v close=%v", resp.StatusCode, body, decodeErr, closeErr)
		} else {
			lastErr = err
		}

		select {
		case <-ctx.Done():
			if lastErr != nil {
				return fmt.Errorf("health never became ready: %w", lastErr)
			}
			return ctx.Err()
		case <-tick.C:
		}
	}
}
