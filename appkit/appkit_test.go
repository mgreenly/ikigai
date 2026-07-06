package appkit

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"testing"
	"time"

	"appkit/internal/testmigrations"
	"appkit/manifest"
	"appkit/server"

	"eventplane/consumer"
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

func TestDispatch_ReducedVerbSetAndManifestLibrary(t *testing.T) {
	// R-8EMY-A004
	dbPath := filepath.Join(t.TempDir(), "widget.db")
	recognized := []struct {
		name       string
		args       []string
		env        map[string]string
		wantCode   int
		wantStderr string
	}{
		{name: "serve", args: []string{"serve"}, env: map[string]string{"WIDGET_DB_PATH": dbPath, "WIDGET_LOG_LEVEL": "screaming"}, wantCode: 1, wantStderr: "invalid log level"},
		{name: "version", args: []string{"version"}, wantCode: 0},
		{name: "manifest", args: []string{"manifest"}, wantCode: 0},
		{name: "migrate", args: []string{"migrate"}, env: map[string]string{"WIDGET_DB_PATH": dbPath}, wantCode: 0},
		{name: "schema", args: []string{"schema"}, env: map[string]string{"WIDGET_DB_PATH": dbPath}, wantCode: 0},
	}
	for _, tc := range recognized {
		t.Run(tc.name, func(t *testing.T) {
			code, _, errs := run(t, testSpec(), tc.env, tc.args...)
			if code != tc.wantCode {
				t.Fatalf("%s exit = %d, want %d; stderr=%q", tc.name, code, tc.wantCode, errs)
			}
			if tc.wantStderr != "" && !strings.Contains(errs, tc.wantStderr) {
				t.Fatalf("%s stderr = %q, want %q", tc.name, errs, tc.wantStderr)
			}
			if strings.Contains(errs, "unknown command") {
				t.Fatalf("%s was not accepted by dispatch: %q", tc.name, errs)
			}
		})
	}

	want := manifest.Emit(manifest.Fields{
		App:     "widget",
		Mount:   "/srv/widget/",
		Port:    3099,
		MCP:     true,
		Feed:    "/feed",
		Extras:  []manifest.KV{{Key: "OUTBOX_RETENTION_DAYS", Value: "7"}},
		Default: false,
	})
	code, out, errs := run(t, testSpec(), nil, "manifest")
	if code != 0 {
		t.Fatalf("manifest exit = %d, want 0; stderr=%q", code, errs)
	}
	if out != want {
		t.Fatalf("manifest dispatch\n got: %q\nwant: %q", out, want)
	}
}

func TestDispatch_ConsumersDeriveManifestConsumes(t *testing.T) {
	// R-4199-A0U9
	spec := testSpec()
	spec.Consumes = nil
	spec.Consumers = []Consumer{{Source: "a"}, {Source: "b"}}

	code, out, errs := run(t, spec, nil, "manifest")
	if code != 0 {
		t.Fatalf("manifest exit = %d, want 0; stderr=%q", code, errs)
	}
	fields, _, err := manifest.Parse(strings.NewReader(out))
	if err != nil {
		t.Fatalf("parse manifest: %v", err)
	}
	if fields["CONSUMES"] != "a,b" {
		t.Fatalf("CONSUMES = %q, want %q", fields["CONSUMES"], "a,b")
	}

	spec.Consumers = nil
	code, out, errs = run(t, spec, nil, "manifest")
	if code != 0 {
		t.Fatalf("manifest without consumers exit = %d, want 0; stderr=%q", code, errs)
	}
	fields, _, err = manifest.Parse(strings.NewReader(out))
	if err != nil {
		t.Fatalf("parse manifest without consumers: %v", err)
	}
	if _, ok := fields["CONSUMES"]; ok {
		t.Fatalf("CONSUMES was emitted for a Spec with no consumer fields: %q", out)
	}
}

func TestConsumersDeriveReflectionSubscriptions(t *testing.T) {
	// R-42H5-NSKY
	spec := Spec{
		Consumers: []Consumer{
			{Source: "crm", Subscriptions: []consumer.Subscription{
				{Source: "crm", Filter: "contact.created"},
				{Source: "crm", Filter: "contact.updated"},
			}},
			{Source: "ledger", Subscriptions: []consumer.Subscription{
				{Source: "ledger", Filter: "invoice.paid"},
			}},
		},
	}

	var got []consumer.Subscription
	_, err := server.New(server.Options{
		Addr:          "127.0.0.1:0",
		Logger:        discardLogger(),
		ResourceID:    "http://localhost:8080/srv/widget/mcp",
		AuthServer:    "http://localhost:8080",
		Version:       versionString(),
		Service:       "widget",
		Subscriptions: specSubscriptions(spec),
		Register: func(rt *server.Router) error {
			provider := rt.Subscriptions()
			if provider == nil {
				t.Fatal("Router Subscriptions provider is nil for Consumers")
			}
			got = provider()
			return nil
		},
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}
	want := []consumer.Subscription{
		{Source: "crm", Filter: "contact.created"},
		{Source: "crm", Filter: "contact.updated"},
		{Source: "ledger", Filter: "invoice.paid"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("subscriptions = %#v, want %#v", got, want)
	}
}

func TestServe_ConsumersConflictWithLegacyFields(t *testing.T) {
	cases := []struct {
		name string
		spec Spec
		want string
	}{
		{
			name: "legacy consumes",
			spec: Spec{
				App:       "widget",
				Consumes:  []string{"crm"},
				Consumers: []Consumer{{Source: "crm", Handler: func(*Router) consumer.Handler { return nil }}},
			},
			want: "Spec.Consumers conflicts with legacy Spec.Consumes",
		},
		{
			name: "legacy subscriptions",
			spec: Spec{
				App:           "widget",
				Subscriptions: func() []consumer.Subscription { return nil },
				Consumers:     []Consumer{{Source: "crm", Handler: func(*Router) consumer.Handler { return nil }}},
			},
			want: "Spec.Consumers conflicts with legacy Spec.Subscriptions",
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			// R-44WY-FC2C
			code, _, errs := run(t, tc.spec, map[string]string{"WIDGET_DB_PATH": filepath.Join(t.TempDir(), "widget.db")}, "serve")
			if code != 1 {
				t.Fatalf("serve exit = %d, want 1; stderr=%q", code, errs)
			}
			if !strings.Contains(errs, tc.want) {
				t.Fatalf("stderr = %q, want conflict %q", errs, tc.want)
			}
		})
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

func TestDispatch_BackupRestoreRemovedAndSpecHasNoHooks(t *testing.T) {
	// R-QQNU-T5M7
	for _, verb := range []string{"backup", "restore"} {
		code, _, errs := run(t, testSpec(), nil, verb)
		if code != 2 {
			t.Fatalf("%s exit = %d, want unknown-command exit 2", verb, code)
		}
		if !strings.Contains(errs, `unknown command "`+verb+`"`) || !strings.Contains(errs, "want serve|version|manifest|migrate|schema") {
			t.Fatalf("%s stderr = %q, want unknown-command message naming reduced verb set", verb, errs)
		}
	}

	specType := reflect.TypeOf(Spec{})
	for _, field := range []string{"Backup", "Restore"} {
		if _, ok := specType.FieldByName(field); ok {
			t.Fatalf("Spec still exposes removed field %s", field)
		}
	}
}

func TestDispatch_UnknownCommand(t *testing.T) {
	code, _, errs := run(t, testSpec(), nil, "bogus")
	if code != 2 {
		t.Fatalf("unknown command exit = %d, want 2", code)
	}
	if !strings.Contains(errs, "unknown command") || !strings.Contains(errs, "want serve|version|manifest|migrate|schema") {
		t.Errorf("stderr = %q, want an unknown-command message with reduced verb set", errs)
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

func TestDispatch_ServeWithWWWFailsOnMissingRoot(t *testing.T) {
	// R-M8VU-IMBO
	root := t.TempDir()
	missingWWW := filepath.Join(root, "share", "www")
	dbPath := filepath.Join(root, "state", "widget.db")
	genPath := filepath.Join(root, "cache", "widget.db.generation")
	spec := testSpec()
	spec.WWW = true

	code, _, errs := run(t, spec, map[string]string{
		"WIDGET_WWW_PATH":        missingWWW,
		"WIDGET_DB_PATH":         dbPath,
		"WIDGET_GENERATION_PATH": genPath,
	}, "serve")
	if code != 1 {
		t.Fatalf("serve missing www exit = %d, want 1; stderr=%q", code, errs)
	}
	if !strings.Contains(errs, missingWWW) {
		t.Fatalf("stderr = %q, want it to name missing www root %q", errs, missingWWW)
	}
	if _, err := os.Stat(dbPath); !os.IsNotExist(err) {
		t.Fatalf("db file exists after missing www failure (stat err=%v), want serve to fail before boot", err)
	}
	if _, err := os.Stat(genPath); !os.IsNotExist(err) {
		t.Fatalf("generation file exists after missing www failure (stat err=%v), want serve to fail before boot", err)
	}
}

func TestServiceBinaryBoot_ReconstructsCacheAndGenerationOnBoot(t *testing.T) {
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
	bin := buildBootSmokeService(t)
	stdout, stderr := startServiceBinaryAndReachHealth(t, bin, addr, map[string]string{
		"WIDGET_DB_PATH":         dbPath,
		"WIDGET_GENERATION_PATH": genPath,
		"WIDGET_PORT":            strconv.Itoa(port),
	})
	if info, err := os.Stat(cacheDir); err != nil || !info.IsDir() {
		t.Fatalf("cache dir after binary boot = (%v, %v), want directory\nstdout:\n%s\nstderr:\n%s", info, err, stdout, stderr)
	}
	generation, err := os.ReadFile(genPath)
	if err != nil {
		t.Fatalf("read generation sidecar after binary boot: %v\nstdout:\n%s\nstderr:\n%s", err, stdout, stderr)
	}
	if strings.TrimSpace(string(generation)) == "" {
		t.Fatalf("generation sidecar is empty\nstdout:\n%s\nstderr:\n%s", stdout, stderr)
	}
	if info, err := os.Stat(dbPath); err != nil || info.IsDir() {
		t.Fatalf("db file after binary boot = (%v, %v), want file\nstdout:\n%s\nstderr:\n%s", info, err, stdout, stderr)
	}
}

func buildBootSmokeService(t *testing.T) string {
	t.Helper()

	appkitDir, err := filepath.Abs(".")
	if err != nil {
		t.Fatalf("resolve appkit dir: %v", err)
	}
	eventplaneDir, err := filepath.Abs(filepath.Join(appkitDir, "..", "eventplane"))
	if err != nil {
		t.Fatalf("resolve eventplane dir: %v", err)
	}
	registryDir, err := filepath.Abs(filepath.Join(appkitDir, "..", "registry"))
	if err != nil {
		t.Fatalf("resolve registry dir: %v", err)
	}

	dir := t.TempDir()
	if err := os.MkdirAll(filepath.Join(dir, "migrations"), 0o755); err != nil {
		t.Fatalf("create service migrations dir: %v", err)
	}
	files := map[string]string{
		"go.mod": fmt.Sprintf(`module widgetbootsmoke

go 1.26

require (
	appkit v0.0.0
	eventplane v0.0.0
	registry v0.0.0
)

replace appkit => %s
replace eventplane => %s
replace registry => %s
`, filepath.ToSlash(appkitDir), filepath.ToSlash(eventplaneDir), filepath.ToSlash(registryDir)),
		"main.go": `package main

import (
	"embed"

	"appkit"
)

//go:embed migrations/*.sql
var migrations embed.FS

func main() {
	appkit.Main(appkit.Spec{
		App:        "widget",
		Mount:      "/srv/widget/",
		Port:       3099,
		MCP:        true,
		Feed:       "/feed",
		Migrations: migrations,
	})
}
`,
		filepath.Join("migrations", "001_schema_migrations.sql"): `CREATE TABLE schema_migrations (
    version    INTEGER PRIMARY KEY,
    applied_at TEXT    NOT NULL
);
`,
		filepath.Join("migrations", "002_widgets.sql"): `CREATE TABLE widgets (
    id   INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);
`,
	}
	for name, contents := range files {
		if err := os.WriteFile(filepath.Join(dir, name), []byte(contents), 0o644); err != nil {
			t.Fatalf("write service %s: %v", name, err)
		}
	}

	bin := filepath.Join(dir, "widget")
	cmd := exec.Command("go", "build", "-mod=mod", "-o", bin, ".")
	cmd.Dir = dir
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("build boot smoke service binary: %v\n%s", err, out)
	}
	return bin
}

func startServiceBinaryAndReachHealth(t *testing.T, bin, addr string, env map[string]string) (string, string) {
	t.Helper()

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, bin, "serve")
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	cmd.Env = append(os.Environ(), "OUTBOX_RETENTION_DAYS=7")
	for k, v := range env {
		cmd.Env = append(cmd.Env, k+"="+v)
	}

	if err := cmd.Start(); err != nil {
		t.Fatalf("start service binary: %v", err)
	}

	waitErr := waitForHealth(ctx, addr, "widget")
	if waitErr == nil {
		if err := cmd.Process.Signal(os.Interrupt); err != nil {
			t.Fatalf("interrupt service binary: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		}
	} else if cmd.Process != nil {
		_ = cmd.Process.Kill()
	}

	err := cmd.Wait()
	if waitErr != nil {
		t.Fatalf("service binary did not reach /health: %v\nwait: %v\nstdout:\n%s\nstderr:\n%s", waitErr, err, stdout.String(), stderr.String())
	}
	if err != nil {
		t.Fatalf("service binary exited after health with %v, want graceful SIGINT shutdown\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
	}
	return stdout.String(), stderr.String()
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
