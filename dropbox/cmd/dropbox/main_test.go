package main

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
	"time"

	appkitdatabase "appkit/db"
	"appkit/manifest"
	appkitserver "appkit/server"
	appweb "appkit/web"

	"dropbox/internal/db"
	dropboxservice "dropbox/internal/dropbox"

	"registry"

	_ "modernc.org/sqlite"
)

func TestRegistrySourcePortsIncludesEveryRegisteredServiceAndNoExtras(t *testing.T) {
	// R-Q9XX-2TKH
	allowed := registrySourcePorts()
	for _, service := range registry.Services {
		if !allowed[service.Port] {
			t.Errorf("registered service %q port %d is not allowed", service.Name, service.Port)
		}
	}
	for _, port := range []int{8080, 39999} {
		if allowed[port] {
			t.Errorf("unregistered port %d is allowed", port)
		}
	}
}

// R-8DF1-W89F
func TestCommittedManifestIsPortable(t *testing.T) {
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}
	if bytes.Contains(committed, []byte("/opt/")) {
		t.Fatalf("committed manifest.env contains on-box /opt/ path:\n%s", committed)
	}
	for _, line := range bytes.Split(committed, []byte("\n")) {
		if bytes.HasPrefix(line, []byte("DROPBOX_DB_PATH=")) || bytes.HasPrefix(line, []byte("DROPBOX_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
// R-QMW4-G94S
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App:     "dropbox",
		Mount:   "/srv/dropbox/",
		Default: false,
		Port:    registry.MustPort("dropbox"),
		MCP:     true,
		Feed:    "/feed",
		Extras: []manifest.KV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
	})
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}

	if got != string(committed) {
		t.Fatalf("manifest.Emit output != committed etc/manifest.env\n--- emit ---\n%s\n--- committed ---\n%s", got, committed)
	}
}

// R-4LKF-FB23
func TestDefaultMirrorPathTracksDurableStateDB(t *testing.T) {
	env := map[string]string{
		"DROPBOX_DB_PATH": "/opt/dropbox/state/dropbox.db",
	}
	got := defaultMirrorPath(func(key string) string { return env[key] })
	if want := "/opt/dropbox/state/mirror"; got != want {
		t.Fatalf("default mirror path = %q, want %q", got, want)
	}
}

func TestDefaultMirrorPathHonorsExplicitOverride(t *testing.T) {
	env := map[string]string{
		"DROPBOX_MIRROR_PATH": "/srv/private/dropbox-mirror",
		"DROPBOX_DB_PATH":     "/opt/dropbox/state/dropbox.db",
	}
	got := defaultMirrorPath(func(key string) string { return env[key] })
	if want := "/srv/private/dropbox-mirror"; got != want {
		t.Fatalf("default mirror path = %q, want explicit override %q", got, want)
	}
}

func TestMountedFilesystemRoutesUseSharedLoopbackGuard(t *testing.T) {
	// R-7UGD-T8J6
	conn, err := sql.Open("sqlite", "file:"+filepath.Join(t.TempDir(), "dropbox.db")+"?_pragma=foreign_keys(ON)")
	if err != nil {
		t.Fatal(err)
	}
	conn.SetMaxOpenConns(1)
	t.Cleanup(func() { conn.Close() })
	migrations, err := appkitdatabase.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatal(err)
	}
	if err := appkitdatabase.Migrate(context.Background(), conn, migrations); err != nil {
		t.Fatal(err)
	}
	mirror, err := dropboxservice.NewMirror(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	svc := dropboxservice.NewService(conn)
	svc.Mirror = mirror
	for _, path := range []string{"/seed.txt", "/delete.txt", "/move-source.txt"} {
		if _, err := svc.Write(context.Background(), path, strings.NewReader(path), "seed"); err != nil {
			t.Fatalf("seed %s: %v", path, err)
		}
	}

	srv, err := appkitserver.New(appkitserver.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/dropbox/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test",
		Service:    "dropbox",
		DB:         conn,
		Register: func(rt *appkitserver.Router) error {
			mountLoopbackRoutes(rt, svc)
			return nil
		},
	})
	if err != nil {
		t.Fatal(err)
	}

	type route struct {
		method string
		url    string
		body   string
		status int
	}
	routes := []route{
		{method: http.MethodGet, url: "/content?path=%2Fseed.txt", status: http.StatusOK},
		{method: http.MethodPut, url: "/content?path=%2Fput.txt", body: "put", status: http.StatusOK},
		{method: http.MethodDelete, url: "/content?path=%2Fdelete.txt", status: http.StatusNoContent},
		{method: http.MethodPost, url: "/mkdir?path=%2Fdir", status: http.StatusNoContent},
		{method: http.MethodPost, url: "/move?from=%2Fmove-source.txt&to=%2Fmove-target.txt", status: http.StatusNoContent},
		{method: http.MethodGet, url: "/list", status: http.StatusOK},
		{method: http.MethodGet, url: "/stat?path=%2Fseed.txt", status: http.StatusOK},
	}

	for _, tc := range routes {
		req := httptest.NewRequest(tc.method, tc.url, strings.NewReader(tc.body))
		req.Header.Set("X-Forwarded-Proto", "https")
		rec := httptest.NewRecorder()
		srv.Handler.ServeHTTP(rec, req)
		if rec.Code != http.StatusNotFound || rec.Body.String() != "404 page not found\n" {
			t.Fatalf("forwarded %s %s = %d %q, want bare 404", tc.method, tc.url, rec.Code, rec.Body.String())
		}
	}
	for _, path := range []string{"/put.txt", "/dir", "/move-target.txt"} {
		if _, err := svc.Stat(path); !errors.Is(err, dropboxservice.ErrNotFound) {
			t.Fatalf("blocked route mutated %s: %v", path, err)
		}
	}
	for _, path := range []string{"/delete.txt", "/move-source.txt"} {
		if _, err := svc.Stat(path); err != nil {
			t.Fatalf("blocked route removed %s: %v", path, err)
		}
	}

	for _, tc := range routes {
		req := httptest.NewRequest(tc.method, tc.url, strings.NewReader(tc.body))
		req.Header.Set("X-Owner-Email", "machine@example.com")
		req.Header.Set("X-Client-Id", "loopback-machine")
		rec := httptest.NewRecorder()
		srv.Handler.ServeHTTP(rec, req)
		if rec.Code != tc.status {
			t.Fatalf("owner-bearing loopback %s %s = %d %q, want %d", tc.method, tc.url, rec.Code, rec.Body.String(), tc.status)
		}
	}
}

// R-4LKF-FB23
func TestDropboxBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "dropbox")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	libexecDir := filepath.Join(appRoot, "libexec")
	binDir := filepath.Join(appRoot, "bin")
	etcDir := filepath.Join(appRoot, "etc")
	shareDir := filepath.Join(appRoot, "share")
	for _, dir := range []string{stateDir, cacheDir, libexecDir, binDir, etcDir, shareDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}

	versionBytes, err := os.ReadFile(filepath.Join("..", "..", "VERSION"))
	if err != nil {
		t.Fatalf("read VERSION: %v", err)
	}
	version := strings.TrimSpace(string(versionBytes))
	if !regexp.MustCompile(`^v[0-9]+\.[0-9]+\.[0-9]+$`).MatchString(version) {
		t.Fatalf("VERSION = %q, want v-prefixed SemVer", version)
	}

	committedManifest, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}
	etcVersionDir := filepath.Join(etcDir, version)
	shareVersionDir := filepath.Join(shareDir, version)
	for _, dir := range []string{etcVersionDir, shareVersionDir} {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", dir, err)
		}
	}
	shippedManifest := filepath.Join(etcVersionDir, "manifest.env")
	if err := os.WriteFile(shippedManifest, committedManifest, 0o644); err != nil {
		t.Fatalf("write shipped manifest.env: %v", err)
	}
	if err := os.Symlink(version, filepath.Join(etcDir, "current")); err != nil {
		t.Fatalf("symlink etc/current: %v", err)
	}
	if err := os.Symlink(version, filepath.Join(shareDir, "current")); err != nil {
		t.Fatalf("symlink share/current: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(filepath.Join(etcDir, "current")); err != nil || resolved != etcVersionDir {
		t.Fatalf("etc/current resolves to %q err=%v, want %q", resolved, err, etcVersionDir)
	}
	if resolved, err := filepath.EvalSymlinks(filepath.Join(shareDir, "current")); err != nil || resolved != shareVersionDir {
		t.Fatalf("share/current resolves to %q err=%v, want %q", resolved, err, shareVersionDir)
	}
	selectedManifest, err := os.ReadFile(filepath.Join(etcDir, "current", "manifest.env"))
	if err != nil {
		t.Fatalf("read selected manifest.env: %v", err)
	}
	if !bytes.Equal(selectedManifest, committedManifest) {
		t.Fatalf("selected manifest.env differs from committed authored file\n--- selected ---\n%s\n--- committed ---\n%s", selectedManifest, committedManifest)
	}

	binary := filepath.Join(libexecDir, "dropbox-"+version)
	build := exec.Command("go", "build", "-buildvcs=false", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build dropbox: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/dropbox-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "dropbox.db")
	generationPath := filepath.Join(cacheDir, "dropbox.db.generation")
	mirrorPath := filepath.Join(stateDir, "mirror")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = testEnv(map[string]string{
		"IKIGENBA_DOMAIN":         "",
		"IKIGENBA_ROOT":           "",
		"DROPBOX_IP":              "127.0.0.1",
		"DROPBOX_PORT":            fmt.Sprintf("%d", port),
		"DROPBOX_DB_PATH":         dbPath,
		"DROPBOX_GENERATION_PATH": generationPath,
		"DROPBOX_MIRROR_PATH":     mirrorPath,
		"DROPBOX_APP_KEY":         "",
		"DROPBOX_APP_SECRET":      "",
		"DROPBOX_REFRESH_TOKEN":   "",
		"DROPBOX_APP_FOLDER_ROOT": "",
		"DROPBOX_WWW_PATH":        wwwRoot(t),
	})
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start dropbox: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "dropbox" {
		t.Fatalf("health service = %v, want dropbox; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	details, ok := doc["details"].(map[string]any)
	if !ok {
		t.Fatalf("health details = %#v, want JSON object", doc["details"])
	}
	for _, key := range []string{"mirror_bytes", "disk_free_bytes", "disk_total_bytes", "failed_files"} {
		if _, ok := details[key]; !ok {
			t.Fatalf("health details missing %s: %#v", key, details)
		}
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("dropbox did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("dropbox did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
	if info, err := os.Stat(mirrorPath); err != nil {
		t.Fatalf("dropbox did not create mirror under state/: %v", err)
	} else if !info.IsDir() {
		t.Fatalf("mirror path %s is not a directory", mirrorPath)
	}
}

func TestWWWSiteLoadsRealShareTree(t *testing.T) {
	root := wwwRoot(t)
	if strings.Contains(root, "internal/web") {
		t.Fatalf("WWW root %q points at deleted internal web package", root)
	}

	site := loadWWW(t)
	rec := httptest.NewRecorder()
	// R-QO40-U0VH
	if err := site.Render(rec, "landing.html", landingData("dropbox-real", "v1.2.3")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "<title>dropbox-real") {
		t.Fatalf("share/www landing render = status %d body:\n%s", rec.Code, rec.Body.String())
	}

	for _, rel := range []string{
		"landing.html",
		filepath.Join("static", "tokens.css"),
		filepath.Join("static", "fonts", "space-grotesk.woff2"),
		filepath.Join("static", "fonts", "ibm-plex-sans.woff2"),
		filepath.Join("static", "fonts", "ibm-plex-mono-400.woff2"),
		filepath.Join("static", "fonts", "ibm-plex-mono-500.woff2"),
	} {
		info, err := os.Stat(filepath.Join(root, rel))
		if err != nil {
			t.Fatalf("share/www missing %s: %v", rel, err)
		}
		if info.IsDir() || info.Size() == 0 {
			t.Fatalf("share/www/%s is not a non-empty file: dir=%v size=%d", rel, info.IsDir(), info.Size())
		}
	}
}

func TestWWWStaticServesThroughChassisSiteOnly(t *testing.T) {
	site := loadWWW(t)
	cases := []struct {
		path        string
		contentType string
	}{
		{path: "/static/tokens.css", contentType: "text/css; charset=utf-8"},
		{path: "/static/fonts/space-grotesk.woff2", contentType: "font/woff2"},
	}

	for _, tc := range cases {
		t.Run(tc.path, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodGet, tc.path, nil)
			rec := httptest.NewRecorder()

			site.Static().ServeHTTP(rec, req)

			// R-QPBX-7SM6
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
			}
			if got := rec.Header().Get("Content-Type"); got != tc.contentType {
				t.Fatalf("Content-Type = %q, want %q", got, tc.contentType)
			}
			if rec.Body.Len() == 0 {
				t.Fatal("body is empty")
			}
		})
	}

	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)
	for _, forbidden := range []string{`web.StaticHandler`, `rt.Handle("GET /static/"`} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/dropbox/main.go still contains dropbox-side static handler %q", forbidden)
		}
	}
}

func TestWWWSiteRendersLandingWithServiceAndVersion(t *testing.T) {
	rec := renderLanding(t, "dropbox-test", "v9.8.7")

	// R-LAND-3C9X
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	// R-LAND-9J6A
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	// R-LAND-5E2Y
	if count := strings.Count(body, "dropbox-test"); count != 3 {
		t.Fatalf("service name count = %d, want 3 in title, heading, and details\n%s", count, body)
	}
	// R-LAND-7G4Z
	if count := strings.Count(body, "v9.8.7"); count != 1 {
		t.Fatalf("version count = %d, want 1\n%s", count, body)
	}
}

func TestWWWSiteUsesCanonicalServiceLayout(t *testing.T) {
	rec := renderLanding(t, "dropbox", "dev")
	body := rec.Body.String()

	for _, want := range []string{
		`<main>`,
		`<a class="home" href="/">Home</a>`,
		`<section aria-labelledby="page-title">`,
		`<div class="eyebrow">Dropbox mirror</div>`,
		`<h1 id="page-title">dropbox</h1>`,
		`Dropbox keeps a private local mirror in sync with one Dropbox app folder and publishes change events to the event plane.`,
		`<dl aria-label="Service details">`,
		`<dd><code>POST /mcp</code></dd>`,
		`class="version"`,
	} {
		// R-HOME-6P8T
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing %q:\n%s", want, body)
		}
	}
}

func TestWWWSiteLinksOnlyAppLocalStaticAssets(t *testing.T) {
	rec := renderLanding(t, "dropbox", "dev")
	body := rec.Body.String()

	// R-LQXL-095Q
	if !strings.Contains(body, `href="static/tokens.css"`) {
		t.Fatalf("landing HTML did not link local tokens.css:\n%s", body)
	}
	for _, forbidden := range []string{`href="/static/tokens.css"`, "dashboard", "/srv/dashboard", "https://", "http://"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing HTML contains forbidden cross-service asset reference %q:\n%s", forbidden, body)
		}
	}
}

func TestWWWSitePreloadsSelfServedFontFiles(t *testing.T) {
	rec := renderLanding(t, "dropbox", "dev")
	head := htmlHead(t, rec.Body.String())
	tokens := readWWWStatic(t, "/static/tokens.css")

	// R-LULA-5KDT
	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		preload := `<link rel="preload" as="font" type="font/woff2" crossorigin href="static/fonts/` + font + `">`
		if !strings.Contains(head, preload) {
			t.Fatalf("landing head missing font preload %q:\n%s", preload, head)
		}
		if !strings.Contains(tokens, `url('fonts/`+font+`')`) {
			t.Fatalf("tokens.css does not use matching self-served URL for %s:\n%s", font, tokens)
		}
	}
}

func TestWWWStaticServesTokensAndFonts(t *testing.T) {
	site := loadWWW(t)
	cases := []struct {
		path        string
		contentType string
	}{
		{path: "/static/tokens.css", contentType: "text/css; charset=utf-8"},
		{path: "/static/fonts/space-grotesk.woff2", contentType: "font/woff2"},
		{path: "/static/fonts/ibm-plex-sans.woff2", contentType: "font/woff2"},
		{path: "/static/fonts/ibm-plex-mono-400.woff2", contentType: "font/woff2"},
		{path: "/static/fonts/ibm-plex-mono-500.woff2", contentType: "font/woff2"},
	}

	for _, tc := range cases {
		t.Run(tc.path, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodGet, tc.path, nil)
			rec := httptest.NewRecorder()

			site.Static().ServeHTTP(rec, req)

			// R-ASST-3H6J
			// R-ASST-5K8L
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
			}
			if got := rec.Header().Get("Content-Type"); got != tc.contentType {
				t.Fatalf("Content-Type = %q, want %q", got, tc.contentType)
			}
			if rec.Body.Len() == 0 {
				t.Fatal("body is empty")
			}
		})
	}
}

func TestWWWTokensCSSDeclaresFontFaces(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-ASST-7M1N
	for _, want := range []string{
		`@font-face`,
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-sans.woff2')`,
		`url('fonts/ibm-plex-mono-400.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
		`font-family: 'Space Grotesk'`,
		`font-family: 'IBM Plex Sans'`,
		`font-family: 'IBM Plex Mono'`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing %q:\n%s", want, body)
		}
	}
}

func TestWWWTokensCSSUsesOptionalFontDisplayForEveryFontFace(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-LS5H-E0WF
	if strings.Contains(body, "font-display: swap") {
		t.Fatalf("tokens.css still contains font-display swap:\n%s", body)
	}
	if faces, optional := strings.Count(body, "@font-face"), strings.Count(body, "font-display: optional;"); optional != faces || faces != 4 {
		t.Fatalf("font-display optional count = %d, want one for each of %d @font-face blocks and 4 total:\n%s", optional, faces, body)
	}
}

func TestWWWTokensCSSUsesSelfServedFontURLs(t *testing.T) {
	body := readWWWStatic(t, "/static/tokens.css")

	// R-LTDD-RSN4
	if strings.Contains(body, `url('/static/fonts/`) {
		t.Fatalf("tokens.css still contains origin-absolute font URL:\n%s", body)
	}
	for _, want := range []string{
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-sans.woff2')`,
		`url('fonts/ibm-plex-mono-400.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing self-served font URL %q:\n%s", want, body)
		}
	}
}

func TestExactRootRouteDoesNotShadowMCPOrUnknownPaths(t *testing.T) {
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(loadWWW(t), "dropbox", "dev"))
	mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "health")
	})
	mux.HandleFunc("GET /feed", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "feed")
	})
	mux.HandleFunc("GET /.well-known/oauth-protected-resource", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "prm")
	})
	mux.HandleFunc("GET /content", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "content")
	})
	mux.HandleFunc("GET /list", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "list")
	})
	mux.HandleFunc("POST /mcp", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "mcp")
	})

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	// R-ROUT-2B5C
	if root.Code != http.StatusOK || !strings.Contains(root.Body.String(), `<h1 id="page-title">dropbox</h1>`) {
		t.Fatalf("root did not dispatch landing handler: status=%d body=%q", root.Code, root.Body.String())
	}

	for _, tc := range []struct {
		method string
		path   string
		body   string
	}{
		{method: http.MethodPost, path: "/mcp", body: "mcp"},
		{method: http.MethodGet, path: "/health", body: "health"},
		{method: http.MethodGet, path: "/feed", body: "feed"},
		{method: http.MethodGet, path: "/.well-known/oauth-protected-resource", body: "prm"},
		{method: http.MethodGet, path: "/content", body: "content"},
		{method: http.MethodGet, path: "/list", body: "list"},
	} {
		t.Run(tc.path, func(t *testing.T) {
			rec := httptest.NewRecorder()
			mux.ServeHTTP(rec, httptest.NewRequest(tc.method, tc.path, nil))

			// R-ROUT-4D7E
			if rec.Code != http.StatusOK || rec.Body.String() != tc.body {
				t.Fatalf("%s %s = status %d body %q, want stub handler body %q", tc.method, tc.path, rec.Code, rec.Body.String(), tc.body)
			}
			if strings.Contains(rec.Body.String(), `<h1 id="page-title">dropbox</h1>`) {
				t.Fatalf("%s %s returned landing page: status=%d body=%q", tc.method, tc.path, rec.Code, rec.Body.String())
			}
		})
	}

	nope := httptest.NewRecorder()
	mux.ServeHTTP(nope, httptest.NewRequest(http.MethodGet, "/nope", nil))
	// R-ROUT-6F9G
	if nope.Code != http.StatusNotFound {
		t.Fatalf("GET /nope status = %d, want %d", nope.Code, http.StatusNotFound)
	}
	if strings.Contains(nope.Body.String(), `<h1 id="page-title">dropbox</h1>`) {
		t.Fatalf("GET /nope returned landing page: status=%d body=%q", nope.Code, nope.Body.String())
	}
}

func TestCompositionRootEnablesChassisWWWAndKeepsDomainWiring(t *testing.T) {
	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)

	for _, want := range []string{
		`WWW:        true,`,
		`rt.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {`,
		`rt.WWW().Render(w, "landing.html", struct {`,
		`Service string`,
		`Version string`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(`,
		`rt.HandleLoopback("GET /content", svc.ContentHandler())`,
		`rt.HandleLoopback("GET /list", svc.ListHandler())`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/dropbox/main.go missing %q", want)
		}
	}
	for _, forbidden := range []string{
		`"dropbox/internal/web"`,
		`rt.Handle("GET /static/"`,
		`web.LandingHandler`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/dropbox/main.go still contains %q", forbidden)
		}
	}

	landingLine := lineContaining(t, main, `rt.Handle("GET /{$}"`)
	if strings.Contains(landingLine, "RequireIdentity") {
		t.Fatalf("landing route is identity-gated: %s", landingLine)
	}
}

func TestNoDropboxWebEmbedsRemainOutsideExistingEmbeddedPackages(t *testing.T) {
	needle := "go:" + "embed"
	for _, root := range []string{"../../cmd", "../../internal"} {
		err := filepath.WalkDir(root, func(path string, entry os.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if entry.IsDir() {
				if path == "../../internal/db" {
					return filepath.SkipDir
				}
				return nil
			}
			if filepath.Ext(path) != ".go" {
				return nil
			}
			src, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			if strings.Contains(string(src), needle) {
				t.Fatalf("%s still contains %s", path, needle)
			}
			return nil
		})
		if err != nil {
			t.Fatalf("scan %s: %v", root, err)
		}
	}
}

func TestNginxLandingLocationIsExactMatchAndSessionGated(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location = /srv/dropbox/ {")
	prefix := nginxLocationBlock(t, conf, "location /srv/dropbox/ {")

	// R-NGNX-2P4Q
	if block == prefix || !strings.HasPrefix(block, "location = /srv/dropbox/ {") || !strings.HasPrefix(prefix, "location /srv/dropbox/ {") {
		t.Fatalf("landing exact-match block is not distinct from prefix block:\nlanding:\n%s\nprefix:\n%s", block, prefix)
	}
	// R-NGNX-4R6S
	if !strings.Contains(block, "auth_request /_session-authn;") || strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("landing block auth_request is not session-gated only:\n%s", block)
	}
	if !strings.Contains(block, "auth_request_set $dropbox_session_owner $upstream_http_x_owner_email;") ||
		!strings.Contains(block, "proxy_set_header X-Owner-Email $dropbox_session_owner;") {
		t.Fatalf("landing block does not propagate session owner identity:\n%s", block)
	}
	// R-NGNX-6T8U
	if !strings.Contains(block, "proxy_pass "+registry.BaseURL("dropbox")+"/;") {
		t.Fatalf("landing block does not proxy to loopback upstream root with trailing slash:\n%s", block)
	}
}

func TestNginxExistingServiceLocationsSurvive(t *testing.T) {
	conf := readNginxConfig(t)
	prefix := nginxLocationBlock(t, conf, "location /srv/dropbox/ {")

	// R-NGNX-8V1W
	for _, want := range []string{
		"location = /srv/dropbox/.well-known/oauth-protected-resource {",
		"location = /srv/dropbox/content {",
		"location @dropbox_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx config missing pre-existing location %q", want)
		}
	}
	if !strings.Contains(conf, "location = /srv/dropbox/content {\n    return 404;\n}") {
		t.Fatalf("nginx config content defence-in-depth location does not return 404")
	}
	if !strings.Contains(prefix, "auth_request /_authn;") {
		t.Fatalf("bearer-gated prefix block is missing auth_request /_authn:\n%s", prefix)
	}
}

func TestNginxStaticLocationIsSessionGatedAndProxiesStaticHandler(t *testing.T) {
	conf := readNginxConfig(t)
	static := nginxLocationBlock(t, conf, "location /srv/dropbox/static/ {")

	for _, want := range []string{
		"auth_request /_session-authn;",
		"proxy_pass " + registry.BaseURL("dropbox") + "/static/;",
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		// R-LVT6-JC4I
		// R-QMW4-G94S
		if !strings.Contains(static, want) {
			t.Fatalf("static location missing %q:\n%s", want, static)
		}
	}
	for _, want := range []string{
		"location = /srv/dropbox/ {",
		"location /srv/dropbox/ {",
		"location = /srv/dropbox/content {\n    return 404;\n}",
		"location = /srv/dropbox/.well-known/oauth-protected-resource {",
		"location @dropbox_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx config missing pre-existing location %q", want)
		}
	}
}

func TestNginxSessionLocationsBounceUnauthorizedNavigationsToLogin(t *testing.T) {
	conf := readNginxConfig(t)
	for _, opener := range []string{
		"location = /srv/dropbox/ {",
		"location /srv/dropbox/static/ {",
	} {
		t.Run(opener, func(t *testing.T) {
			block := nginxLocationBlock(t, conf, opener)

			// R-3MN6-J0UR
			if !strings.Contains(block, "auth_request /_session-authn;") || !strings.Contains(block, "error_page 401 = @login_bounce;") {
				t.Fatalf("session-gated location is missing login bounce opt-in:\\n%s", block)
			}
		})
	}
}

func TestNginxBearerLocationKeepsUnauthorizedResponseForMCPClients(t *testing.T) {
	conf := readNginxConfig(t)
	bearer := nginxLocationBlock(t, conf, "location /srv/dropbox/ {")

	// R-3NV2-WSLG
	if !strings.Contains(bearer, "auth_request /_authn;") || strings.Contains(bearer, "error_page 401 = @login_bounce;") {
		t.Fatalf("bearer location must keep its unredirected 401 behavior:\\n%s", bearer)
	}
}

func TestNginxLoginBounceOptInPreservesExistingLocationDirectives(t *testing.T) {
	conf := readNginxConfig(t)

	// R-3P2Z-AKC5
	for _, want := range []string{
		"location = /srv/dropbox/.well-known/oauth-protected-resource {",
		"location = /srv/dropbox/content {",
		"location = /srv/dropbox/ {",
		"location /srv/dropbox/static/ {",
		"location /srv/dropbox/ {",
		"location @dropbox_authn_500 {",
	} {
		if !strings.Contains(conf, want) {
			t.Fatalf("nginx config removed existing location %q", want)
		}
	}
	for opener, proxyPass := range map[string]string{
		"location = /srv/dropbox/ {":      "proxy_pass " + registry.BaseURL("dropbox") + "/;",
		"location /srv/dropbox/static/ {": "proxy_pass " + registry.BaseURL("dropbox") + "/static/;",
	} {
		block := nginxLocationBlock(t, conf, opener)
		if !strings.Contains(block, "auth_request /_session-authn;") || !strings.Contains(block, proxyPass) {
			t.Fatalf("session location did not retain its auth request and proxy pass:\\n%s", block)
		}
	}
}

func testEnv(overrides map[string]string) []string {
	env := os.Environ()
	out := make([]string, 0, len(env)+len(overrides))
	for _, kv := range env {
		key, _, _ := strings.Cut(kv, "=")
		if _, ok := overrides[key]; ok {
			continue
		}
		out = append(out, kv)
	}
	for key, value := range overrides {
		out = append(out, key+"="+value)
	}
	return out
}

func freeTCPPort(t *testing.T) int {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen for free port: %v", err)
	}
	defer ln.Close()
	return ln.Addr().(*net.TCPAddr).Port
}

func waitForHealth(t *testing.T, port int, done <-chan error, stdout, stderr *bytes.Buffer) map[string]any {
	t.Helper()
	url := fmt.Sprintf("http://127.0.0.1:%d/health", port)
	client := http.Client{Timeout: 250 * time.Millisecond}
	deadline := time.Now().Add(5 * time.Second)
	var last string
	for time.Now().Before(deadline) {
		select {
		case err := <-done:
			t.Fatalf("dropbox exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
		default:
		}

		resp, err := client.Get(url)
		if err == nil {
			body, readErr := io.ReadAll(resp.Body)
			closeErr := resp.Body.Close()
			if resp.StatusCode == http.StatusOK && readErr == nil && closeErr == nil {
				var doc map[string]any
				if err := json.Unmarshal(body, &doc); err != nil {
					t.Fatalf("decode health JSON: %v\nbody:\n%s", err, body)
				}
				return doc
			}
			last = fmt.Sprintf("status=%d read=%v close=%v body=%s", resp.StatusCode, readErr, closeErr, body)
		} else {
			last = err.Error()
		}
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatalf("dropbox never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
	}
}

func loadWWW(t *testing.T) *appweb.Site {
	t.Helper()
	site, err := appweb.Load(wwwRoot(t))
	if err != nil {
		t.Fatalf("load share/www: %v", err)
	}
	return site
}

func wwwRoot(t *testing.T) string {
	t.Helper()
	root, err := filepath.Abs(filepath.Join("..", "..", "share", "www"))
	if err != nil {
		t.Fatalf("resolve share/www: %v", err)
	}
	return root
}

func renderLanding(t *testing.T, service, version string) *httptest.ResponseRecorder {
	t.Helper()
	rec := httptest.NewRecorder()
	if err := loadWWW(t).Render(rec, "landing.html", landingData(service, version)); err != nil {
		t.Fatalf("render landing.html: %v", err)
	}
	return rec
}

func landingHandler(site *appweb.Site, service, version string) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		if err := site.Render(w, "landing.html", landingData(service, version)); err != nil {
			http.Error(w, "template error", http.StatusInternalServerError)
		}
	})
}

func landingData(service, version string) struct {
	Service string
	Version string
} {
	return struct {
		Service string
		Version string
	}{
		Service: service,
		Version: version,
	}
}

func readWWWStatic(t *testing.T, path string) string {
	t.Helper()
	req := httptest.NewRequest(http.MethodGet, path, nil)
	rec := httptest.NewRecorder()
	loadWWW(t).Static().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("GET %s status = %d, want %d\n%s", path, rec.Code, http.StatusOK, rec.Body.String())
	}
	return rec.Body.String()
}

func lineContaining(t *testing.T, text, needle string) string {
	t.Helper()
	for _, line := range strings.Split(text, "\n") {
		if strings.Contains(line, needle) {
			return line
		}
	}
	t.Fatalf("no line contains %q", needle)
	return ""
}

func htmlHead(t *testing.T, body string) string {
	t.Helper()
	start := strings.Index(body, "<head>")
	if start == -1 {
		t.Fatalf("HTML missing head opener:\n%s", body)
	}
	end := strings.Index(body[start:], "</head>")
	if end == -1 {
		t.Fatalf("HTML missing head closer:\n%s", body)
	}
	return body[start : start+end+len("</head>")]
}

func readNginxConfig(t *testing.T) string {
	t.Helper()
	src, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatal(err)
	}
	return string(src)
}

func nginxLocationBlock(t *testing.T, conf, opener string) string {
	t.Helper()
	start := strings.Index(conf, opener)
	if start == -1 {
		t.Fatalf("nginx config missing %q", opener)
	}
	bodyStart := start + len(opener)
	endRel := strings.Index(conf[bodyStart:], "\n}")
	if endRel == -1 {
		t.Fatalf("nginx config location %q has no closing brace", opener)
	}
	return conf[start : bodyStart+endRel+len("\n}")]
}
