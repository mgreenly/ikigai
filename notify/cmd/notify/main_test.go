package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"regexp"
	"strings"
	"sync"
	"testing"
	"time"

	"appkit"
	"appkit/manifest"
	"appkit/server"
	appweb "appkit/web"
	"eventplane/consumer"
	"notify/internal/push"
	"registry"
)

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
		if bytes.HasPrefix(line, []byte("NOTIFY_DB_PATH=")) || bytes.HasPrefix(line, []byte("NOTIFY_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App:      "notify",
		Mount:    "/srv/notify/",
		Default:  false,
		Port:     registry.MustPort("notify"),
		MCP:      true,
		Consumes: []string{"crm", "prompts"},
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
func TestNotifyBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "notify")
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
	copyTree(t, wwwRoot(t), filepath.Join(shareVersionDir, "www"))
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

	binary := filepath.Join(libexecDir, "notify-"+version)
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build notify: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/notify-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	crmFeed := newIdleFeedServer(t)
	promptsFeed := newIdleFeedServer(t)
	ntfy := httptest.NewServer(http.NotFoundHandler())
	t.Cleanup(ntfy.Close)

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "notify.db")
	generationPath := filepath.Join(cacheDir, "notify.db.generation")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	crmFeedKey := "NOTIFY_CRM_" + "FEED_URL"
	promptsFeedKey := "NOTIFY_PROMPTS_" + "FEED_URL"
	cmd.Env = testEnv(map[string]string{
		"IKIGENBA_DOMAIN":        "",
		"IKIGENBA_ROOT":          "",
		"NOTIFY_IP":              "127.0.0.1",
		"NOTIFY_PORT":            fmt.Sprintf("%d", port),
		"NOTIFY_DB_PATH":         dbPath,
		"NOTIFY_GENERATION_PATH": generationPath,
		"NOTIFY_WWW_PATH":        filepath.Join(shareDir, "current", "www"),
		crmFeedKey:               crmFeed.URL + "/feed",
		promptsFeedKey:           promptsFeed.URL + "/feed",
		"NOTIFY_NTFY_BASE_URL":   ntfy.URL,
		"NTFY_TOPIC":             "notify-test",
		"NTFY_API_KEY":           "notify-test-token",
	})
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start notify: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "notify" {
		t.Fatalf("health service = %v, want notify; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, ok := doc["details"].(map[string]any); !ok {
		t.Fatalf("health details = %#v, want JSON object", doc["details"])
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("notify did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("notify did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
}

func TestNotifySpecDeclaresConsumersInOrder(t *testing.T) {
	// R-4DG9-3Q97
	spec := notifySpec()
	if len(spec.Consumers) != 2 {
		t.Fatalf("len(spec.Consumers) = %d, want 2", len(spec.Consumers))
	}

	crm := spec.Consumers[0]
	if crm.Source != "crm" {
		t.Fatalf("spec.Consumers[0].Source = %q, want crm", crm.Source)
	}
	if want := []consumer.Subscription{push.Subscription()}; !reflect.DeepEqual(crm.Subscriptions, want) {
		t.Fatalf("crm subscriptions = %#v, want %#v", crm.Subscriptions, want)
	}

	prompts := spec.Consumers[1]
	if prompts.Source != "prompts" {
		t.Fatalf("spec.Consumers[1].Source = %q, want prompts", prompts.Source)
	}
	if want := push.PromptsSubscriptions(); !reflect.DeepEqual(prompts.Subscriptions, want) {
		t.Fatalf("prompts subscriptions = %#v, want %#v", prompts.Subscriptions, want)
	}
}

func TestNotifyConsumerHandlersPushSubscribedEventsOnly(t *testing.T) {
	// R-4EO5-HHZW
	spec := notifySpec()
	rt := newTestRouter(t)

	for _, tc := range []struct {
		source       string
		event        consumer.Event
		unsubscribed consumer.Event
		wantTitle    string
		wantBody     string
	}{
		{
			source: "crm",
			event: consumer.Event{
				Type:    "contact.created",
				ID:      "01JCONTACT",
				Source:  "crm",
				Payload: json.RawMessage(`{"display_name":"Ada Lovelace"}`),
			},
			unsubscribed: consumer.Event{
				Type:    "contact.updated",
				ID:      "01JCONTACTUP",
				Source:  "crm",
				Payload: json.RawMessage(`{"display_name":"Ada Lovelace"}`),
			},
			wantTitle: "New contact",
			wantBody:  "Ada Lovelace",
		},
		{
			source: "prompts",
			event: consumer.Event{
				Type:    "run.succeeded",
				ID:      "01JRUNOK",
				Source:  "prompts",
				Payload: json.RawMessage(`{"session_id":"s1","session_name":"nightly scan","trigger_event":"cron.nightly","scheduled_for":"2026-06-06T08:00:00Z"}`),
			},
			unsubscribed: consumer.Event{
				Type:    "run.cancelled",
				ID:      "01JRUNCANCEL",
				Source:  "prompts",
				Payload: json.RawMessage(`{"session_name":"nightly scan"}`),
			},
			wantTitle: "Run succeeded",
			wantBody:  "nightly scan",
		},
	} {
		t.Run(tc.source, func(t *testing.T) {
			ntfy := newNtfyRecorder(t)
			t.Setenv("NOTIFY_NTFY_BASE_URL", ntfy.srv.URL)
			t.Setenv("NTFY_TOPIC", "topic")
			t.Setenv("NTFY_API_KEY", "tok")

			entry, ok := findConsumer(spec.Consumers, tc.source)
			if !ok {
				t.Fatalf("consumer %q not found", tc.source)
			}
			h := entry.Handler(rt)
			if err := h(context.Background(), tc.event); err != nil {
				t.Fatalf("%s subscribed event returned %v, want nil", tc.source, err)
			}
			post := ntfy.waitForPost(t)
			if post.method != http.MethodPost {
				t.Fatalf("method = %q, want POST", post.method)
			}
			if post.path != "/topic" {
				t.Fatalf("path = %q, want /topic", post.path)
			}
			if post.auth != "Bearer tok" {
				t.Fatalf("Authorization = %q, want bearer token", post.auth)
			}
			if post.title != tc.wantTitle {
				t.Fatalf("Title = %q, want %q", post.title, tc.wantTitle)
			}
			if post.body != tc.wantBody {
				t.Fatalf("body = %q, want %q", post.body, tc.wantBody)
			}

			if err := h(context.Background(), tc.unsubscribed); err != nil {
				t.Fatalf("%s unsubscribed event returned %v, want nil", tc.source, err)
			}
			ntfy.assertNoPost(t)
		})
	}
}

func TestNotifySpecEnablesChassisWWWAndKeepsMCPWiring(t *testing.T) {
	// R-4FW1-V9QL — notify opts into appkit's WWW loader/static mount while keeping MCP enabled.
	spec := notifySpec()
	if !spec.WWW {
		t.Fatal("notifySpec().WWW = false, want true")
	}
	if !spec.MCP {
		t.Fatal("notifySpec().MCP = false, want true")
	}

	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)
	for _, want := range []string{
		`WWW:   true,`,
		`r.Handle("GET /{$}", landingHandler(r.WWW(), r.Service(), r.Version()))`,
		`pushClient := push.NewClient(cfg.ntfyBase, cfg.ntfyTopic, cfg.ntfyToken, r.Logger())`,
		`r.Handle("POST /mcp", r.RequireIdentity(`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/notify/main.go missing %q", want)
		}
	}
	for _, forbidden := range []string{
		`"notify/internal/web"`,
		`r.Handle("GET /static/"`,
		`web.LandingHandler`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/notify/main.go still contains %q", forbidden)
		}
	}
}

func TestWWWSiteLoadsRealShareTree(t *testing.T) {
	// R-4H3Y-91HA — landing and static assets are loaded from share/www, not the deleted internal web package.
	root := wwwRoot(t)
	if strings.Contains(root, "internal/web") {
		t.Fatalf("WWW root %q points at deleted internal web package", root)
	}

	site := loadWWW(t)
	rec := httptest.NewRecorder()
	if err := site.Render(rec, "landing.html", landingData("notify-real", "v1.2.3")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "<title>notify-real") {
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

func TestWWWSiteRendersLandingWithServiceVersionAndHTMLContentType(t *testing.T) {
	// R-LAND-3C8K — render landing.html through appkit/web and assert a 200 response.
	// R-LAND-5D1M — rendered body contains the service name passed in.
	// R-LAND-7E4N — a distinctive injected version appears verbatim.
	// R-LAND-9F6P — landing response Content-Type is text/html; charset=utf-8.
	rec := renderLanding(t, "notify-test", "9.9.9-test")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("content type = %q, want text/html; charset=utf-8", got)
	}

	body := rec.Body.String()
	if !strings.Contains(body, "notify-test") {
		t.Fatalf("body does not render service name: %s", body)
	}
	if !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("body does not render injected version verbatim: %s", body)
	}
}

func TestWWWSiteLandingIncludesHomeLinkToDashboardApex(t *testing.T) {
	// R-HOME-5N7S — GET / renders a top-left Home anchor to the dashboard apex.
	rec := renderLanding(t, "notify", "v1.2.3")

	body := rec.Body.String()
	if !strings.Contains(body, `<a class="home" href="/">Home</a>`) {
		t.Fatalf("body does not render Home link to apex: %s", body)
	}
	if strings.Index(body, `<a class="home" href="/">Home</a>`) < strings.Index(body, "<main>") {
		t.Fatalf("Home link is not inside main: %s", body)
	}
	for _, want := range []string{
		".home {",
		"position: absolute;",
		"top: var(--space-8);",
		"position: relative;",
		".home:hover,\n    .home:focus-visible",
		"color: var(--color-text);",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing Home link style %q: %s", want, body)
		}
	}
}

func TestExactRootRouteDispatchesToLanding(t *testing.T) {
	// R-ROUT-4G2Q — GET / dispatches to the landing handler registered at GET /{$}.
	rec := httptest.NewRecorder()
	composedMux(t, http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "notify") || !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("GET / did not reach landing handler: %s", body)
	}
}

func TestExactRootRouteDoesNotShadowMCP(t *testing.T) {
	// R-ROUT-6H5R — {$} does not shadow a sibling POST /mcp; the stub is reached.
	stub := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusTeapot)
		_, _ = w.Write([]byte("mcp-stub"))
	})

	rec := httptest.NewRecorder()
	composedMux(t, stub).ServeHTTP(rec, httptest.NewRequest(http.MethodPost, "/mcp", nil))

	if rec.Code != http.StatusTeapot || rec.Body.String() != "mcp-stub" {
		t.Fatalf("POST /mcp did not reach stub: code=%d body=%q", rec.Code, rec.Body.String())
	}
	if strings.Contains(rec.Body.String(), "notify") {
		t.Fatalf("POST /mcp was shadowed by the landing page")
	}
}

func TestExactRootRouteDoesNotCaptureNonRootPaths(t *testing.T) {
	// R-ROUT-8J7S — an unregistered non-root path is not captured by {$}.
	rec := httptest.NewRecorder()
	composedMux(t, http.NotFoundHandler()).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/nope", nil))

	if rec.Code != http.StatusNotFound {
		t.Fatalf("GET /nope status = %d, want %d", rec.Code, http.StatusNotFound)
	}
	if strings.Contains(rec.Body.String(), "notify") {
		t.Fatalf("GET /nope returned the landing page")
	}
}

func TestWWWStaticServesTokensCSSWithContentType(t *testing.T) {
	// R-ASST-3K9T — GET /static/tokens.css through the chassis site handler is 200 with a CSS content type.
	rec := readWWWStaticResponse(t, "/static/tokens.css")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); !strings.Contains(got, "text/css") {
		t.Fatalf("content type = %q, want text/css", got)
	}
}

func TestWWWSiteReferencesOnlyDocumentRelativeLocalStaticAssets(t *testing.T) {
	// R-ASST-5L2V — rendered HTML references notify's own static asset path and no cross-service/dashboard URL.
	// R-8M7T-A9VB — GET / renders the stylesheet document-relative, not origin-absolute.
	rec := renderLanding(t, "notify", "v1.2.3")

	body := rec.Body.String()
	if !strings.Contains(body, `href="static/tokens.css"`) {
		t.Fatalf("landing body does not reference its own document-relative static asset path: %s", body)
	}
	if strings.Contains(body, `href="/static/tokens.css"`) {
		t.Fatalf("landing body references origin-absolute static asset path: %s", body)
	}
	for _, forbidden := range []string{"http://", "https://", "dashboard"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing body references a cross-service/external asset URL %q: %s", forbidden, body)
		}
	}
}

func TestWWWSitePreloadsSelfServedFontFiles(t *testing.T) {
	// R-8NFP-O1M0 — GET / preloads above-the-fold fonts with document-relative hrefs matching @font-face src targets.
	rec := renderLanding(t, "notify", "v1.2.3")

	head := htmlHead(t, rec.Body.String())
	css := readWWWStatic(t, "/static/tokens.css")
	for _, font := range []string{"space-grotesk.woff2", "ibm-plex-sans.woff2"} {
		want := `<link rel="preload" as="font" type="font/woff2" crossorigin
        href="static/fonts/` + font + `">`
		if !strings.Contains(head, want) {
			t.Fatalf("landing head missing preload %q:\n%s", want, head)
		}
		if strings.Contains(head, `href="/static/fonts/`+font+`"`) {
			t.Fatalf("landing head preloads %s with origin-absolute href:\n%s", font, head)
		}
		if !strings.Contains(css, `url('fonts/`+font+`')`) {
			t.Fatalf("tokens.css missing @font-face src target for preloaded font %s", font)
		}
	}
}

func TestWWWStaticServesFontWithWoff2ContentType(t *testing.T) {
	// R-ASST-7M4W — GET /static/fonts/space-grotesk.woff2 is 200 with a font/woff2 content type.
	rec := readWWWStaticResponse(t, "/static/fonts/space-grotesk.woff2")

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got := rec.Header().Get("Content-Type"); got != "font/woff2" {
		t.Fatalf("content type = %q, want font/woff2", got)
	}
	if rec.Body.Len() == 0 {
		t.Fatal("font body is empty")
	}
}

func TestWWWTokensCSSUsesDocumentRelativeFontURLs(t *testing.T) {
	// R-8KZW-WI4M — served tokens.css uses document-relative embedded font URLs, never origin-absolute /static/fonts URLs.
	css := readWWWStatic(t, "/static/tokens.css")
	if strings.Contains(css, "url('/static/fonts/") {
		t.Fatalf("tokens.css contains origin-absolute static font URL: %s", css)
	}
	for _, font := range []string{
		"space-grotesk.woff2",
		"ibm-plex-sans.woff2",
		"ibm-plex-mono-400.woff2",
		"ibm-plex-mono-500.woff2",
	} {
		want := "url('fonts/" + font + "')"
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css missing embedded font URL %q", want)
		}
	}
}

func TestWWWTokensCSSUsesOptionalFontDisplay(t *testing.T) {
	// R-8JS0-IQDX — every @font-face block in served tokens.css uses optional display and no block uses swap.
	css := readWWWStatic(t, "/static/tokens.css")
	if strings.Contains(css, "font-display: swap") {
		t.Fatalf("tokens.css still contains font-display: swap: %s", css)
	}
	if got := strings.Count(css, "font-display: optional"); got != 4 {
		t.Fatalf("font-display: optional count = %d, want 4", got)
	}
	if got := strings.Count(css, "@font-face"); got != 4 {
		t.Fatalf("@font-face block count = %d, want 4", got)
	}
}

func TestNginxHasExactMatchLandingLocation(t *testing.T) {
	// R-NGNX-3N6X — the fragment contains an exact-match `location = /srv/notify/ {` block.
	frag := readNginxConfig(t)
	if !strings.Contains(frag, "location = /srv/notify/ {") {
		t.Fatal("missing exact-match `location = /srv/notify/ {` block")
	}
	if strings.Index(frag, "location = /srv/notify/ {") == strings.Index(frag, "location /srv/notify/ {") {
		t.Fatal("exact-match and prefix locations must be distinct blocks")
	}
}

func TestNginxExactMatchUsesSessionAuthn(t *testing.T) {
	// R-NGNX-5P8Y — the exact-match block gates the landing root with session auth, not bearer auth.
	block := nginxLocationBlock(t, readNginxConfig(t), "location = /srv/notify/ {")
	if !strings.Contains(block, "auth_request /_session-authn") {
		t.Errorf("exact-match block missing `auth_request /_session-authn`:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn") {
		t.Errorf("exact-match block must NOT gate landing root with bearer `auth_request /_authn`:\n%s", block)
	}
}

func TestNginxExactMatchProxiesToLoopbackRoot(t *testing.T) {
	// R-NGNX-7Q1Z — the exact-match block proxies to the loopback upstream root with a trailing slash.
	// R-RGDR-4F6Q — the expected loopback upstream is derived from the notify registry entry.
	block := nginxLocationBlock(t, readNginxConfig(t), "location = /srv/notify/ {")
	want := "proxy_pass " + registry.BaseURL("notify") + "/;"
	if !strings.Contains(block, want) {
		t.Errorf("exact-match block missing %q:\n%s", want, block)
	}
}

func TestNginxPreExistingLocationsSurvive(t *testing.T) {
	// R-NGNX-9R3B — the additive edit preserves the bearer-gated prefix location and the unauthenticated PRM bootstrap.
	frag := readNginxConfig(t)
	if !strings.Contains(frag, "location /srv/notify/ {") {
		t.Error("bearer-gated prefix `location /srv/notify/ {` missing")
	}
	if !strings.Contains(frag, "auth_request /_authn;") {
		t.Error("bearer-gated prefix must still use `auth_request /_authn;`")
	}
	if !strings.Contains(frag, "location = /srv/notify/.well-known/oauth-protected-resource {") {
		t.Error("PRM bootstrap location missing")
	}
}

func TestNginxSessionGatesStaticAssets(t *testing.T) {
	// R-8ONM-1TCP — static assets are session-gated while existing landing, bearer, PRM, and rate-limit locations remain.
	frag := readNginxConfig(t)
	block := nginxLocationBlock(t, frag, "location /srv/notify/static/ {")
	wantStatic := "proxy_pass " + registry.BaseURL("notify") + "/static/;"
	for _, want := range []string{
		"auth_request /_session-authn;",
		wantStatic,
		"proxy_set_header Host $host;",
		"proxy_set_header X-Forwarded-Proto $scheme;",
		"proxy_http_version 1.1;",
	} {
		if !strings.Contains(block, want) {
			t.Fatalf("static asset block missing %q:\n%s", want, block)
		}
	}
	for _, want := range []string{
		"location = /srv/notify/ {",
		"location /srv/notify/ {",
		"location = /srv/notify/.well-known/oauth-protected-resource {",
		"location @notify_authn_500 {",
	} {
		if !strings.Contains(frag, want) {
			t.Fatalf("pre-existing nginx location %q missing", want)
		}
	}
}

type capturedNtfyPost struct {
	method string
	path   string
	title  string
	auth   string
	body   string
}

type ntfyRecorder struct {
	mu     sync.Mutex
	posts  []capturedNtfyPost
	posted chan struct{}
	srv    *httptest.Server
}

func newNtfyRecorder(t *testing.T) *ntfyRecorder {
	t.Helper()
	n := &ntfyRecorder{posted: make(chan struct{}, 10)}
	n.srv = httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		n.mu.Lock()
		n.posts = append(n.posts, capturedNtfyPost{
			method: r.Method,
			path:   r.URL.Path,
			title:  r.Header.Get("Title"),
			auth:   r.Header.Get("Authorization"),
			body:   string(body),
		})
		n.mu.Unlock()
		n.posted <- struct{}{}
		w.WriteHeader(http.StatusOK)
	}))
	t.Cleanup(n.srv.Close)
	return n
}

func (n *ntfyRecorder) waitForPost(t *testing.T) capturedNtfyPost {
	t.Helper()
	select {
	case <-n.posted:
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for ntfy POST")
	}
	posts := n.snapshot()
	if len(posts) != 1 {
		t.Fatalf("ntfy posts = %d, want exactly 1", len(posts))
	}
	return posts[0]
}

func (n *ntfyRecorder) assertNoPost(t *testing.T) {
	t.Helper()
	select {
	case <-n.posted:
		t.Fatalf("ntfy posts = %d, want no additional POSTs", len(n.snapshot()))
	case <-time.After(50 * time.Millisecond):
	}
}

func (n *ntfyRecorder) snapshot() []capturedNtfyPost {
	n.mu.Lock()
	defer n.mu.Unlock()
	out := make([]capturedNtfyPost, len(n.posts))
	copy(out, n.posts)
	return out
}

func findConsumer(consumers []appkit.Consumer, source string) (appkit.Consumer, bool) {
	for _, entry := range consumers {
		if entry.Source == source {
			return entry, true
		}
	}
	return appkit.Consumer{}, false
}

func newTestRouter(t *testing.T) *appkit.Router {
	t.Helper()
	var rt *appkit.Router
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "http://resource.test/srv/notify/",
		AuthServer: "http://dashboard.test/",
		Version:    "v0.0.0",
		Service:    "notify",
		Register: func(r *appkit.Router) error {
			rt = r
			return nil
		},
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}
	t.Cleanup(func() { _ = srv.Close() })
	if rt == nil {
		t.Fatal("server.New did not call Register")
	}
	return rt
}

func TestSpecPortComesFromRegistryNotifyPort(t *testing.T) {
	// R-RGSP-4A1K
	if got, ok := registry.Port("notify"); !ok || got != 3201 {
		t.Fatalf("registry.Port(%q) = %d, %v, want 3201, true", "notify", got, ok)
	}
	if got := registry.MustPort("notify"); got != 3201 {
		t.Fatalf("registry.MustPort(%q) = %d, want 3201", "notify", got)
	}
	if got, want := notifySpec().Port, registry.MustPort("notify"); got != want {
		t.Fatalf("notifySpec().Port = %d, want registry.MustPort(%q) = %d", got, "notify", want)
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

func composedMux(t *testing.T, mcp http.Handler) *http.ServeMux {
	t.Helper()
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(loadWWW(t), "notify", "9.9.9-test"))
	mux.Handle("POST /mcp", mcp)
	return mux
}

func readWWWStaticResponse(t *testing.T, path string) *httptest.ResponseRecorder {
	t.Helper()
	rec := httptest.NewRecorder()
	loadWWW(t).Static().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, path, nil))
	return rec
}

func readWWWStatic(t *testing.T, path string) string {
	t.Helper()
	rec := readWWWStaticResponse(t, path)
	if rec.Code != http.StatusOK {
		t.Fatalf("GET %s status = %d, want %d\n%s", path, rec.Code, http.StatusOK, rec.Body.String())
	}
	return rec.Body.String()
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

func copyTree(t *testing.T, src, dst string) {
	t.Helper()
	err := filepath.WalkDir(src, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(src, path)
		if err != nil {
			return err
		}
		target := filepath.Join(dst, rel)
		if entry.IsDir() {
			return os.MkdirAll(target, 0o755)
		}
		info, err := entry.Info()
		if err != nil {
			return err
		}
		b, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		return os.WriteFile(target, b, info.Mode().Perm())
	})
	if err != nil {
		t.Fatalf("copy %s to %s: %v", src, dst, err)
	}
}

func newIdleFeedServer(t *testing.T) *httptest.Server {
	t.Helper()
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/feed" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		if f, ok := w.(http.Flusher); ok {
			f.Flush()
		}
		<-r.Context().Done()
	}))
	t.Cleanup(srv.Close)
	return srv
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
			t.Fatalf("notify exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
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
	t.Fatalf("notify never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
	}
}
