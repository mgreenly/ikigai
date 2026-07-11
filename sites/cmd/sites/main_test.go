package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
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

	"appkit"
	sqlkit "appkit/db"
	"appkit/manifest"
	appweb "appkit/web"
	"github.com/chromedp/cdproto/browser"
	"github.com/chromedp/cdproto/runtime"
	"github.com/chromedp/chromedp"
	"registry"

	sitedb "sites/internal/db"
	sitesdomain "sites/internal/sites"
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
		if bytes.HasPrefix(line, []byte("SITES_DB_PATH=")) || bytes.HasPrefix(line, []byte("SITES_GENERATION_PATH=")) {
			t.Fatalf("committed manifest.env contains runtime path line %q", line)
		}
	}
}

// R-8IAN-FB87
func TestManifestLibraryByteEqualsCommittedFile(t *testing.T) {
	spec := sitesSpec()
	got := manifest.Emit(manifest.Fields{
		App:      spec.App,
		Mount:    spec.Mount,
		Default:  spec.Default,
		Port:     spec.Port,
		MCP:      spec.MCP,
		Feed:     spec.Feed,
		Consumes: spec.Consumes,
		Extras:   manifestExtras(spec.ManifestExtras),
	})
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatalf("read committed manifest.env: %v", err)
	}

	if got != string(committed) {
		t.Fatalf("manifest.Emit output != committed etc/manifest.env\n--- emit ---\n%s\n--- committed ---\n%s", got, committed)
	}
}

// R-7K2P-QN4D
func TestSitesSpecUsesRegistryPort(t *testing.T) {
	if got, want := sitesSpec().Port, registry.MustPort("sites"); got != want {
		t.Fatalf("sitesSpec().Port = %d, want registry.MustPort(%q) = %d", got, "sites", want)
	}
}

// R-7L9F-XW3H
func TestGoSourcesUseRegistryForLoopbackPorts(t *testing.T) {
	root := filepath.Join("..", "..")
	standalonePort := regexp.MustCompile(`(^|[^A-Za-z0-9_])3004([^A-Za-z0-9_]|$)`)
	var offenders []string

	err := filepath.WalkDir(root, func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() || filepath.Ext(path) != ".go" || strings.HasSuffix(path, "_test.go") {
			return nil
		}

		content, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		text := string(content)
		if strings.Contains(text, "127.0.0.1:30") || standalonePort.MatchString(text) {
			rel, relErr := filepath.Rel(root, path)
			if relErr != nil {
				rel = path
			}
			offenders = append(offenders, rel)
		}
		return nil
	})
	if err != nil {
		t.Fatalf("walk Go sources under module root: %v", err)
	}
	if len(offenders) > 0 {
		t.Fatalf("Go sources still contain hardcoded sites loopback port literals: %s", strings.Join(offenders, ", "))
	}
}

// R-7M4C-BV8J
func TestDropboxRegistryBaseURL(t *testing.T) {
	if got, want := registry.BaseURL("dropbox"), "http://127.0.0.1:3200"; got != want {
		t.Fatalf("registry.BaseURL(%q) = %q, want %q", "dropbox", got, want)
	}
}

// R-7N6R-TZ2Q
func TestGoModRequiresAndReplacesRegistry(t *testing.T) {
	content, err := os.ReadFile(filepath.Join("..", "..", "go.mod"))
	if err != nil {
		t.Fatalf("read go.mod: %v", err)
	}
	text := string(content)
	requireRegistry := regexp.MustCompile(`(?m)^\s*registry\s+v0\.0\.0\s*$`)
	replaceRegistry := regexp.MustCompile(`(?m)^replace\s+registry\s+=>\s+\.\./registry\s*$`)
	if !requireRegistry.MatchString(text) {
		t.Fatalf("go.mod is missing require registry v0.0.0")
	}
	if !replaceRegistry.MatchString(text) {
		t.Fatalf("go.mod is missing replace registry => ../registry")
	}
}

func TestSitesSpecEnablesChassisWWWAndKeepsMCPWiring(t *testing.T) {
	// R-0SF5-VPQF — sites opts into the chassis WWW loader while keeping the MCP surface enabled.
	spec := sitesSpec()
	if !spec.WWW {
		t.Fatal("sitesSpec().WWW = false, want true")
	}
	if !spec.MCP {
		t.Fatal("sitesSpec().MCP = false, want true")
	}

	src, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatal(err)
	}
	main := string(src)
	for _, want := range []string{
		`WWW:        true,`,
		`list, err := store.List(r.Context())`,
		`landingView{`,
		`landingHandler(store, rt.WWW(), rt.Service(), rt.Version(), baseURL)`,
		`CreatedAt:     s.CreatedAt.UTC().Format(time.RFC3339),`,
		`renderer.Render(w, "landing.html", view)`,
		`mirror := sites.NewMirrorClient(base)`,
		`handler, err := mcp.NewHandler(store, layout, baseURL, mirror, rt)`,
		`if err != nil {`,
		`rt.Handle("POST /mcp", rt.RequireIdentity(handler))`,
	} {
		if !strings.Contains(main, want) {
			t.Fatalf("cmd/sites/main.go missing %q", want)
		}
	}
	for _, forbidden := range []string{
		`"sites/internal/web"`,
		`rt.Handle("GET /static/"`,
		`web.LandingHandler`,
		`web.StaticHandler`,
	} {
		if strings.Contains(main, forbidden) {
			t.Fatalf("cmd/sites/main.go still contains %q", forbidden)
		}
	}
}

func TestWWWSiteLoadsRealShareTree(t *testing.T) {
	// R-0TN2-9HH4 — landing and static assets are loaded from share/www, not an embedded internal web package.
	root := wwwRoot(t)
	if strings.Contains(root, "internal/web") {
		t.Fatalf("WWW root %q points at deleted internal web package", root)
	}

	site := loadWWW(t)
	rec := httptest.NewRecorder()
	if err := site.Render(rec, "landing.html", landingData("sites-real", "v1.2.3")); err != nil {
		t.Fatalf("render landing.html from share/www: %v", err)
	}
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "<title>sites-real") {
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
	rec := renderLanding(t, "sites", "9.9.9-test")
	body := rec.Body.String()

	// R-LAND-3C9K
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	// R-LAND-5E2M
	if !strings.Contains(body, "sites") {
		t.Fatalf("body does not contain service name: %q", body)
	}
	// R-LAND-7G4P
	if !strings.Contains(body, "9.9.9-test") {
		t.Fatalf("body does not contain version: %q", body)
	}
	// R-LAND-9J6R
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
}

func TestWWWLandingRendersExistingSites(t *testing.T) {
	rec := httptest.NewRecorder()
	err := loadWWW(t).Render(rec, "landing.html", landingView{
		Service: "sites",
		Version: "phase18-test",
		Sites: []siteRow{
			{Slug: "atlas", Public: true, CreatedBy: "alice@example.com", CreatedAt: "2026-07-08T14:15:16Z"},
			{Slug: "vault", Public: false, CreatedBy: "bob@example.com", CreatedAt: "2026-07-09T01:02:03Z"},
		},
	})
	if err != nil {
		t.Fatalf("render landing.html with sites: %v", err)
	}
	body := rec.Body.String()

	// R-RAW6-IUN5
	for _, want := range []string{
		"atlas",
		"public",
		"alice@example.com",
		"2026-07-08T14:15:16Z",
		"vault",
		"private",
		"bob@example.com",
		"2026-07-09T01:02:03Z",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing site field %q in:\n%s", want, body)
		}
	}
}

func TestWWWLandingRendersEmptySitesWithVersion(t *testing.T) {
	rec := httptest.NewRecorder()
	err := loadWWW(t).Render(rec, "landing.html", landingView{
		Service: "sites",
		Version: "empty-sites-version",
		Sites:   nil,
	})
	if err != nil {
		t.Fatalf("render landing.html with empty sites: %v", err)
	}
	body := rec.Body.String()

	// R-RC42-WMDU
	if !strings.Contains(body, "empty-sites-version") {
		t.Fatalf("landing HTML missing service version in empty state:\n%s", body)
	}
	if !strings.Contains(body, "No sites have been created yet.") {
		t.Fatalf("landing HTML missing explicit empty state:\n%s", body)
	}
}

func TestLandingHandlerRendersJSONIslandFromSiteRows(t *testing.T) {
	store := newLandingTestStore(t, landingSeed{name: "atlas", public: true}, landingSeed{name: "vault", public: false})
	rec := httptest.NewRecorder()
	landingHandler(store, loadWWW(t), "sites", "phase24", "https://suite.example/srv/sites/").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	match := regexp.MustCompile(`(?s)<script type="application/json" id="sites-data">(.*?)</script>`).FindStringSubmatch(rec.Body.String())
	if len(match) != 2 {
		t.Fatalf("landing HTML missing sites data island:\n%s", rec.Body.String())
	}
	var rows []landingSiteData
	if err := json.Unmarshal([]byte(match[1]), &rows); err != nil {
		t.Fatalf("unmarshal sites data island: %v\n%s", err, match[1])
	}

	// R-IDOL-PV70
	if len(rows) != 2 || rows[0].Slug != "atlas" || rows[0].URL != "https://suite.example/srv/sites/public/atlas/" ||
		!rows[0].Public || rows[0].CreatedBy != "atlas@example.com" || rows[0].CreatedAt != "2026-07-08T12:00:00Z" ||
		rows[0].CreatedAtSort != "2026-07-08T12:00:00Z" {
		t.Fatalf("sites data = %#v, want rendered site fields with its row URL and sortable UTC timestamp", rows)
	}
	if rows[1].URL != "https://suite.example/srv/sites/private/vault/" || rows[1].Public {
		t.Fatalf("sites data = %#v, want private row URL and visibility", rows)
	}
}

func TestWWWLandingRendersProgressiveControlMarkup(t *testing.T) {
	rec := httptest.NewRecorder()
	if err := loadWWW(t).Render(rec, "landing.html", landingView{
		Service: "sites",
		Version: "phase24-controls",
		Sites:   []siteRow{{Slug: "atlas"}},
	}); err != nil {
		t.Fatalf("render landing.html with a site: %v", err)
	}
	body := rec.Body.String()

	// R-IEWI-3MXP
	for _, want := range []string{
		`class="no-js"`,
		`class="controls js-only" hidden`,
		`id="site-search" aria-label="Search sites"`,
		`id="site-clear"`,
		`class="pager js-only" hidden`,
		`id="pager-prev"`,
		`id="pager-label"`,
		`id="pager-next"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing progressive control markup %q:\n%s", want, body)
		}
	}

	// R-IG4E-HEOE
	for _, want := range []string{
		`<th scope="col" data-sort-key="name">Slug</th>`,
		`<th scope="col" data-sort-key="createdBy">Creator</th>`,
		`<th scope="col" data-sort-key="createdAt">Created</th>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing sortable header %q:\n%s", want, body)
		}
	}
	if strings.Contains(body, `<th scope="col" data-sort-key="visibility">Visibility</th>`) {
		t.Fatalf("landing HTML makes visibility sortable:\n%s", body)
	}

	// R-IHCA-V6F3
	emptyBody := renderLanding(t, "sites", "phase24-empty").Body.String()
	if !strings.Contains(emptyBody, `<script type="application/json" id="sites-data">[]</script>`) {
		t.Fatalf("empty landing HTML does not expose an empty JSON data island:\n%s", emptyBody)
	}

	// R-ICGP-C3GB
	if !strings.Contains(body, `<script src="static/landing.js" defer></script>`) {
		t.Fatalf("landing HTML does not defer its control script:\n%s", body)
	}
	asset, err := os.ReadFile(filepath.Join(wwwRoot(t), "static", "landing.js"))
	if err != nil || len(asset) == 0 {
		t.Fatalf("read shipped landing.js: err=%v size=%d", err, len(asset))
	}
}

func TestWWWLandingPlacesEnhancementsAfterTheTable(t *testing.T) {
	rec := httptest.NewRecorder()
	if err := loadWWW(t).Render(rec, "landing.html", landingView{Sites: []siteRow{{Slug: "atlas"}}}); err != nil {
		t.Fatalf("render landing.html with a site: %v", err)
	}
	body := rec.Body.String()

	// R-83NK-DUW1
	controls := strings.Index(body, `class="controls js-only" hidden`)
	table := strings.Index(body, `<table class="site-table">`)
	noMatch := strings.Index(body, `class="no-match js-only" hidden`)
	pager := strings.Index(body, `class="pager js-only" hidden`)
	if controls < 0 || table < 0 || noMatch < 0 || pager < 0 || !(controls < table && table < noMatch && noMatch < pager) {
		t.Fatalf("progressive controls are not ordered lead < controls < table < no-match < pager:\n%s", body)
	}

	// R-84VG-RMMQ
	for _, want := range []string{
		`[hidden] {`,
		`display: none !important;`,
		`.site-table th[data-sort-key] {`,
		`cursor: pointer;`,
		`.site-table th[aria-sort="ascending"]::after`,
		`content: " ▲";`,
		`.site-table th[aria-sort="descending"]::after`,
		`content: " ▼";`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing enhanced-control style %q:\n%s", want, body)
		}
	}
	if regexp.MustCompile(`<th[^>]+aria-sort=`).MatchString(body) {
		t.Fatalf("server-rendered landing HTML must not pre-stamp aria-sort:\n%s", body)
	}
}

func TestWWWLandingRendersServiceAndVersionInOneHeading(t *testing.T) {
	rec := renderLanding(t, "sites", "v20-test")
	body := rec.Body.String()
	headingRe := regexp.MustCompile(`(?s)<h1 id="page-title">(.+?)</h1>`)
	match := headingRe.FindStringSubmatch(body)
	if len(match) != 2 {
		t.Fatalf("landing HTML missing page-title h1:\n%s", body)
	}
	heading := match[1]

	// R-WKGI-FVFJ
	if !strings.Contains(heading, "sites") || !strings.Contains(heading, `<span class="version">v20-test</span>`) {
		t.Fatalf("h1 does not contain service and version on one heading line: %q", heading)
	}
	for _, forbidden := range []string{
		"POST /mcp",
		`aria-label="Service details"`,
		"<dt>Service</dt>",
		"<dt>Version</dt>",
		"<dt>API</dt>",
	} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("landing HTML still contains removed service detail markup %q in:\n%s", forbidden, body)
		}
	}
}

func TestWWWLandingRendersStaticWebsiteHostCopy(t *testing.T) {
	rec := renderLanding(t, "sites", "v20-copy")
	body := rec.Body.String()

	// R-WLOE-TN68
	for _, want := range []string{
		"Static website host",
		"Hosts file-backed static websites and serves them through the suite gateway.",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing copy %q in:\n%s", want, body)
		}
	}
	if strings.Contains(body, "Sites hosts file-backed") {
		t.Fatalf("landing HTML still contains old lead copy:\n%s", body)
	}
}

func TestLandingHandlerLinksSlugsToVisibilityURLs(t *testing.T) {
	store := newLandingTestStore(t, landingSeed{name: "X", public: true}, landingSeed{name: "Y", public: false})
	baseURL := "https://suite.example/srv/sites/"
	rec := httptest.NewRecorder()

	landingHandler(store, loadWWW(t), "sites", "phase20", baseURL).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))
	body := rec.Body.String()

	// R-WMWB-7EWX
	for _, want := range []string{
		`<td data-label="Slug"><a href="https://suite.example/srv/sites/public/X/">X</a></td>`,
		`<td data-label="Slug"><a href="https://suite.example/srv/sites/private/Y/">Y</a></td>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing HTML missing slug link %q in:\n%s", want, body)
		}
	}
	if strings.Contains(body, `href="https://suite.example/srv/sites/public/Y/"`) ||
		strings.Contains(body, `href="https://suite.example/srv/sites/private/X/"`) {
		t.Fatalf("landing HTML mixed visibility tiers between rows:\n%s", body)
	}
}

func TestExactRootRouteDispatchesWithoutShadowingSiblings(t *testing.T) {
	mux := composedMux(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusAccepted)
		_, _ = w.Write([]byte("mcp"))
	}))

	root := httptest.NewRecorder()
	mux.ServeHTTP(root, httptest.NewRequest(http.MethodGet, "/", nil))
	rootBody := root.Body.String()
	// R-ROUT-4Q8B
	if root.Code != http.StatusOK || !strings.Contains(rootBody, "sites") || !strings.Contains(rootBody, "1.2.3") {
		t.Fatalf("GET / returned status %d body %q, want landing page", root.Code, rootBody)
	}

	mcp := httptest.NewRecorder()
	mux.ServeHTTP(mcp, httptest.NewRequest(http.MethodPost, "/mcp", nil))
	// R-ROUT-6S1D
	if mcp.Code != http.StatusAccepted || strings.Contains(mcp.Body.String(), "sites") {
		t.Fatalf("POST /mcp returned status %d body %q, want sibling handler", mcp.Code, mcp.Body.String())
	}

	nope := httptest.NewRecorder()
	mux.ServeHTTP(nope, httptest.NewRequest(http.MethodGet, "/nope", nil))
	// R-ROUT-8U3F
	if nope.Code == http.StatusOK || strings.Contains(nope.Body.String(), "sites") {
		t.Fatalf("GET /nope returned status %d body %q, want not found without landing page", nope.Code, nope.Body.String())
	}
}

func TestWWWStaticServesTokensCSS(t *testing.T) {
	rec := readWWWStaticResponse(t, "/static/tokens.css")
	body := rec.Body.String()

	// R-ASST-3H7N
	if rec.Code != http.StatusOK || !strings.Contains(rec.Header().Get("Content-Type"), "text/css") {
		t.Fatalf("GET /static/tokens.css returned status %d Content-Type %q", rec.Code, rec.Header().Get("Content-Type"))
	}

	// R-629P-84O5
	if strings.Contains(body, "font-display: swap") {
		t.Fatalf("tokens.css still uses swap font display: %q", body)
	}
	for _, block := range strings.Split(body, "@font-face") {
		if !strings.Contains(block, "font-family:") {
			continue
		}
		if !strings.Contains(block, "font-display: optional;") {
			t.Fatalf("@font-face block does not use optional font display: %s", block)
		}
	}

	// R-ASST-3H7N
	// R-63HL-LWEU
	if strings.Contains(body, `url('/static/fonts/`) {
		t.Fatalf("tokens.css contains origin-absolute font URLs: %q", body)
	}
	for _, want := range []string{
		`url('fonts/space-grotesk.woff2')`,
		`url('fonts/ibm-plex-sans.woff2')`,
		`url('fonts/ibm-plex-mono-400.woff2')`,
		`url('fonts/ibm-plex-mono-500.woff2')`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing relative font URL %q in: %q", want, body)
		}
	}
	for _, want := range []string{
		"--color-bg:",
		"--space-4:  16px;",
		"--text-display-size:",
		"--text-display-lh:",
		"--text-label-size:",
		"--text-label-weight:",
		"--border-width:",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("tokens.css missing Carbon landing token %q in: %q", want, body)
		}
	}
}

func TestWWWLandingReferencesOwnStaticPath(t *testing.T) {
	rec := renderLanding(t, "sites", "asset-test")
	body := rec.Body.String()
	head := htmlHead(t, body)

	// R-ASST-5K9Q
	// R-64PH-ZO5J
	if !strings.Contains(head, `href="static/tokens.css"`) {
		t.Fatalf("landing HTML head does not reference document-relative tokens.css: %q", head)
	}
	if strings.Contains(head, `href="/static/tokens.css"`) {
		t.Fatalf("landing HTML head references origin-absolute tokens.css: %q", head)
	}
	if strings.Contains(body, "dashboard") || strings.Contains(body, "://") {
		t.Fatalf("landing HTML references a cross-service or remote asset URL: %q", body)
	}
}

func TestWWWLandingPreloadsSelfHostedFonts(t *testing.T) {
	rec := renderLanding(t, "sites", "font-preload-test")
	body := rec.Body.String()
	head := htmlHead(t, body)
	css := readWWWStatic(t, "/static/tokens.css")

	// R-65XE-DFW8
	for _, font := range []string{
		"space-grotesk.woff2",
		"ibm-plex-sans.woff2",
	} {
		want := `<link rel="preload" as="font" type="font/woff2" crossorigin
        href="static/fonts/` + font + `">`
		if !strings.Contains(head, want) {
			t.Fatalf("landing HTML head missing document-relative preload for %s in: %q", font, head)
		}
		if strings.Contains(head, `href="/static/fonts/`+font+`"`) {
			t.Fatalf("landing HTML head references origin-absolute preload for %s in: %q", font, head)
		}
		if !strings.Contains(css, `url('fonts/`+font+`')`) {
			t.Fatalf("tokens.css does not contain matching @font-face source for %s in: %q", font, css)
		}
	}
}

func TestWWWLandingRendersHomeLinkBeforeMain(t *testing.T) {
	rec := renderLanding(t, "sites", "home-link-test")
	body := rec.Body.String()

	// R-HOME-9S3W
	if !strings.Contains(body, `<main>
    <a class="home" href="/">Home</a>`) {
		t.Fatalf("landing HTML does not render Home link as the first body child inside main: %q", body)
	}
	if !strings.Contains(body, `.home {`) || !strings.Contains(body, `.home:hover,
    .home:focus-visible`) {
		t.Fatalf("landing HTML does not include inline .home style rule: %q", body)
	}
}

func TestLandingTemplateConformsToCronCanonicalWithSitesCopy(t *testing.T) {
	webDir := wwwRoot(t)
	sitesLanding := readFile(t, filepath.Join(webDir, "landing.html"))
	cronLanding := string(readFile(t, filepath.Join(webDir, "..", "..", "..", "cron", "share", "www", "landing.html")))

	for _, replacement := range []struct {
		old string
		new string
	}{
		{`<title>{{.Service}} · cron</title>`, `<title>{{.Service}} · sites</title>`},
		{`<div class="eyebrow">Scheduled event emitter</div>`, `<div class="eyebrow">Static website host</div>`},
		{`<p>Cron keeps named schedules in SQLite and emits typed event-plane messages at minute boundaries.</p>`, `<p>Hosts file-backed static websites and serves them through the suite gateway.</p>`},
	} {
		if !strings.Contains(cronLanding, replacement.old) {
			t.Fatalf("cron canonical landing template missing %q", replacement.old)
		}
		if !bytes.Contains(sitesLanding, []byte(replacement.new)) {
			t.Fatalf("sites landing template missing canonical sites copy %q", replacement.new)
		}
	}
}

func TestTokensCSSMatchesCronCanonical(t *testing.T) {
	webDir := wwwRoot(t)
	sitesTokens := readFile(t, filepath.Join(webDir, "static", "tokens.css"))
	cronTokens := readFile(t, filepath.Join(webDir, "..", "..", "..", "cron", "share", "www", "static", "tokens.css"))

	if !bytes.Equal(sitesTokens, cronTokens) {
		t.Fatalf("sites tokens.css differs from cron canonical tokens.css")
	}
}

func TestWWWStaticServesFonts(t *testing.T) {
	// R-ASST-7M2S
	for _, font := range []string{
		"space-grotesk.woff2",
		"ibm-plex-sans.woff2",
		"ibm-plex-mono-400.woff2",
		"ibm-plex-mono-500.woff2",
	} {
		rec := readWWWStaticResponse(t, "/static/fonts/"+font)

		if rec.Code != http.StatusOK || rec.Header().Get("Content-Type") != "font/woff2" {
			t.Fatalf("GET %s returned status %d Content-Type %q", font, rec.Code, rec.Header().Get("Content-Type"))
		}
		if rec.Body.Len() == 0 {
			t.Fatalf("GET %s returned an empty body", font)
		}
	}
}

func TestNginxFragmentGatesAndProxiesLandingRoot(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location = /srv/sites/")

	// R-NGNX-3P6T
	if !strings.Contains(conf, "location = /srv/sites/ {") {
		t.Fatalf("nginx fragment is missing exact-match landing root location")
	}
	if strings.Contains(conf, "location /srv/sites/ {") {
		t.Fatalf("nginx fragment contains a catch-all /srv/sites/ prefix location")
	}
	if !strings.Contains(conf, "location /srv/sites/public/ {") || !strings.Contains(conf, "location /srv/sites/private/ {") {
		t.Fatalf("nginx fragment is missing public/private tier prefixes")
	}

	// R-NGNX-5R8V
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing root block does not use dashboard session auth:\n%s", block)
	}
	if strings.Contains(block, "auth_request /_authn;") {
		t.Fatalf("landing root block uses bearer auth instead of session auth:\n%s", block)
	}

	// R-NGNX-7T1X
	if !strings.Contains(block, "proxy_pass http://127.0.0.1:3004/;") {
		t.Fatalf("landing root block does not proxy to the templated upstream root:\n%s", block)
	}
	if strings.Contains(block, "alias ") {
		t.Fatalf("landing root block is disk-backed instead of proxied:\n%s", block)
	}
}

func TestNginxFragmentPreservesExistingLocations(t *testing.T) {
	conf := readNginxConfig(t)

	prm := nginxLocationBlock(t, conf, "location = /srv/sites/.well-known/oauth-protected-resource")
	mcp := nginxLocationBlock(t, conf, "location = /srv/sites/mcp")
	landing := nginxLocationBlock(t, conf, "location = /srv/sites/")
	public := nginxLocationBlock(t, conf, "location /srv/sites/public/")
	private := nginxLocationBlock(t, conf, "location /srv/sites/private/")
	authn500 := nginxLocationBlock(t, conf, "location @sites_authn_500")

	// R-NGNX-9W4Z
	if strings.Contains(prm, "auth_request") {
		t.Fatalf("PRM bootstrap location unexpectedly requires auth:\n%s", prm)
	}
	if !strings.Contains(mcp, "auth_request /_authn;") {
		t.Fatalf("MCP location does not preserve bearer auth_request:\n%s", mcp)
	}
	if strings.Contains(public, "auth_request") {
		t.Fatalf("public static tier unexpectedly requires auth:\n%s", public)
	}
	if !strings.Contains(private, "auth_request /_session-authn;") {
		t.Fatalf("private static tier does not preserve session auth_request:\n%s", private)
	}
	if !strings.Contains(landing, "proxy_pass http://127.0.0.1:3004/;") {
		t.Fatalf("landing root block does not preserve upstream root proxy:\n%s", landing)
	}
	if !strings.Contains(authn500, "return 429;") || !strings.Contains(authn500, "return 500;") {
		t.Fatalf("authn 500 re-emit location does not preserve expected returns:\n%s", authn500)
	}
}

func TestNginxFragmentGatesAndProxiesLandingStaticAssets(t *testing.T) {
	conf := readNginxConfig(t)
	block := nginxLocationBlock(t, conf, "location /srv/sites/static/")

	// R-675A-R7MX
	if !strings.Contains(block, "auth_request /_session-authn;") {
		t.Fatalf("landing static block does not use dashboard session auth:\n%s", block)
	}
	if !strings.Contains(block, "proxy_pass http://127.0.0.1:3004/static/;") {
		t.Fatalf("landing static block does not proxy to the templated static upstream:\n%s", block)
	}
	if strings.Contains(block, "alias ") {
		t.Fatalf("landing static block is disk-backed instead of proxied:\n%s", block)
	}
}

func TestNginxFragmentProxiesPublicAndSessionGatesPrivateTiers(t *testing.T) {
	conf := readNginxConfig(t)
	public := nginxLocationBlock(t, conf, "location /srv/sites/public/")
	private := nginxLocationBlock(t, conf, "location /srv/sites/private/")

	// R-R78H-DJF2
	if !strings.Contains(public, "proxy_pass http://127.0.0.1:3004/public/;") {
		t.Fatalf("public tier does not proxy to the public upstream:\n%s", public)
	}
	if strings.Contains(public, "auth_request") {
		t.Fatalf("public tier unexpectedly requires auth:\n%s", public)
	}

	// R-R8GD-RB5R
	if !strings.Contains(private, "auth_request /_session-authn;") {
		t.Fatalf("private tier does not use dashboard session auth:\n%s", private)
	}
	if !strings.Contains(private, "proxy_pass http://127.0.0.1:3004/private/;") {
		t.Fatalf("private tier does not proxy to the private upstream:\n%s", private)
	}

	// R-R9OA-52WG
	if strings.Contains(public, "alias ") {
		t.Fatalf("public tier still contains an alias directive:\n%s", public)
	}
	if strings.Contains(private, "alias ") {
		t.Fatalf("private tier still contains an alias directive:\n%s", private)
	}
	if strings.Contains(conf, "alias") {
		t.Fatalf("nginx fragment still contains an alias directive:\n%s", conf)
	}
	if match := regexp.MustCompile(`/opt/sites/[^ \t\n]*www[^ \t\n]*`).FindString(conf); match != "" {
		t.Fatalf("nginx fragment still references an on-disk www path %q:\n%s", match, conf)
	}
	for _, prefix := range []string{
		"location = /srv/sites/",
		"location = /srv/sites/mcp",
		"location = /srv/sites/.well-known/oauth-protected-resource",
		"location @sites_authn_500",
	} {
		if !strings.Contains(conf, prefix) {
			t.Fatalf("nginx fragment is missing %s:\n%s", prefix, conf)
		}
	}
}

func TestNginxFragmentBouncesOnlySessionGatedLocations(t *testing.T) {
	conf := readNginxConfig(t)
	sessionLocations := map[string]string{
		"location = /srv/sites/":       "proxy_pass http://127.0.0.1:3004/;",
		"location /srv/sites/static/":  "proxy_pass http://127.0.0.1:3004/static/;",
		"location /srv/sites/private/": "proxy_pass http://127.0.0.1:3004/private/;",
	}

	// R-XVIT-1NXD
	for location, proxyPass := range sessionLocations {
		block := nginxLocationBlock(t, conf, location)
		for _, directive := range []string{"auth_request /_session-authn;", "error_page 401 = @login_bounce;"} {
			if !strings.Contains(block, directive) {
				t.Fatalf("%s missing session bounce directive %q:\n%s", location, directive, block)
			}
		}

		// R-XXYL-T7ER
		if !strings.Contains(block, proxyPass) {
			t.Fatalf("%s does not preserve proxy directive %q:\n%s", location, proxyPass, block)
		}
	}

	// R-XWQP-FFO2
	for _, location := range []string{"location /srv/sites/public/", "location = /srv/sites/mcp"} {
		block := nginxLocationBlock(t, conf, location)
		if strings.Contains(block, "error_page 401 = @login_bounce;") {
			t.Fatalf("%s must not bounce a 401 to the session login:\n%s", location, block)
		}
	}

	// R-XXYL-T7ER
	for _, location := range []string{
		"location = /srv/sites/.well-known/oauth-protected-resource",
		"location = /srv/sites/mcp",
		"location = /srv/sites/",
		"location /srv/sites/static/",
		"location /srv/sites/public/",
		"location /srv/sites/private/",
		"location @sites_authn_500",
	} {
		_ = nginxLocationBlock(t, conf, location)
	}
}

// R-4LKF-FB23
func TestSitesBootsFromOpsctlLayoutAndServesHealth(t *testing.T) {
	root := t.TempDir()
	appRoot := filepath.Join(root, "sites")
	stateDir := filepath.Join(appRoot, "state")
	cacheDir := filepath.Join(appRoot, "cache")
	wwwDir := filepath.Join(stateDir, "www")
	libexecDir := filepath.Join(appRoot, "libexec")
	binDir := filepath.Join(appRoot, "bin")
	etcDir := filepath.Join(appRoot, "etc")
	shareDir := filepath.Join(appRoot, "share")
	for _, dir := range []string{
		filepath.Join(wwwDir, "public"),
		filepath.Join(wwwDir, "private"),
		filepath.Join(wwwDir, "working"),
		cacheDir,
		libexecDir,
		binDir,
		etcDir,
		shareDir,
	} {
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

	binary := filepath.Join(libexecDir, "sites-"+version)
	build := exec.Command("go", "build", "-o", binary, ".")
	build.Env = os.Environ()
	if out, err := build.CombinedOutput(); err != nil {
		t.Fatalf("go build sites: %v\n%s", err, out)
	}

	run := filepath.Join(binDir, "run")
	if err := os.Symlink("../libexec/sites-"+version, run); err != nil {
		t.Fatalf("symlink bin/run: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(run); err != nil || resolved != binary {
		t.Fatalf("bin/run resolves to %q err=%v, want %q", resolved, err, binary)
	}

	port := freeTCPPort(t)
	dbPath := filepath.Join(stateDir, "sites.db")
	generationPath := filepath.Join(cacheDir, "sites.db.generation")
	dropbox := httptest.NewServer(http.NotFoundHandler())
	t.Cleanup(dropbox.Close)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var stdout, stderr bytes.Buffer
	cmd := exec.CommandContext(ctx, run, "serve")
	cmd.Env = testEnv(map[string]string{
		"IKIGENBA_DOMAIN":  "int.ikigenba.com",
		"IKIGENBA_ROOT":    root,
		"SITES_IP":         "127.0.0.1",
		"SITES_PORT":       fmt.Sprintf("%d", port),
		"DROPBOX_BASE_URL": dropbox.URL,
	}, "SITES_ROOT")
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		t.Fatalf("start sites: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	defer stopProcess(cancel, done)

	doc := waitForHealth(t, port, done, &stdout, &stderr)
	if got := doc["service"]; got != "sites" {
		t.Fatalf("health service = %v, want sites; body=%v", got, doc)
	}
	if got := doc["status"]; got != "ok" {
		t.Fatalf("health status = %v, want ok; body=%v", got, doc)
	}
	if _, err := os.Stat(dbPath); err != nil {
		t.Fatalf("sites did not create DB under state/: %v", err)
	}
	if _, err := os.Stat(generationPath); err != nil {
		t.Fatalf("sites did not create generation sidecar under cache/: %v", err)
	}
	if filepath.Dir(generationPath) != cacheDir {
		t.Fatalf("generation sidecar path %s is not under cache dir %s", generationPath, cacheDir)
	}
	for _, tier := range []string{"public", "private", "working"} {
		if _, err := os.Stat(filepath.Join(wwwDir, tier)); err != nil {
			t.Fatalf("sites www tier %s missing under state/www: %v", tier, err)
		}
	}
}

func TestLandingBrowserControlsWorkThroughRealDOMEvents(t *testing.T) {
	seeds := []landingSeed{
		{name: "docs", public: true, createdAt: "2026-07-04T08:00:00.000000000Z"},
		{name: "dashboard", public: true, createdAt: "2026-07-12T08:00:00.000000000Z"},
		{name: "blog", public: false, createdAt: "2026-07-01T08:00:00.000000000Z"},
	}
	for i := 1; i <= 9; i++ {
		seeds = append(seeds, landingSeed{
			name:      fmt.Sprintf("site-%02d", i),
			public:    i%2 == 0,
			createdAt: fmt.Sprintf("2026-07-%02dT08:00:00.000000000Z", i+2),
		})
	}
	store := newLandingTestStore(t, seeds...)
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", landingHandler(store, loadWWW(t), "sites", "phase26", "http://suite.test/srv/sites/"))
	mux.Handle("/static/", loadWWW(t).Static())
	srv := httptest.NewServer(mux)
	defer srv.Close()

	session, cancel := landingBrowserSession(t)
	defer cancel()

	var slugs []string
	readSlugs := chromedp.Evaluate(`Array.from(document.querySelectorAll("tbody tr td:first-child a"), function (a) { return a.textContent; })`, &slugs)
	var ariaSort string
	var searchValue string
	var pagerLabel string
	var rowCount int

	// R-87B9-J644
	if err := chromedp.Run(session, chromedp.Navigate(srv.URL), chromedp.WaitVisible(`#site-search`)); err != nil {
		t.Fatalf("boot landing controller: %v", err)
	}

	// R-88J5-WXUT
	if err := chromedp.Run(session, chromedp.SendKeys(`#site-search`, "dsb"), readSlugs); err != nil {
		t.Fatalf("filter dashboard: %v", err)
	}
	if got, want := strings.Join(slugs, ","), "dashboard"; got != want {
		t.Fatalf("filtered slugs = %q, want %q", got, want)
	}

	// R-89R2-APLI
	if err := chromedp.Run(session,
		chromedp.Click(`th[data-sort-key="name"]`),
		chromedp.Evaluate(`document.querySelector('th[data-sort-key="name"]').getAttribute("aria-sort")`, &ariaSort),
	); err != nil {
		t.Fatalf("sort slugs ascending: %v", err)
	}
	if ariaSort != "ascending" {
		t.Fatalf("slug ascending aria-sort = %q, want ascending", ariaSort)
	}
	if err := chromedp.Run(session,
		chromedp.Click(`th[data-sort-key="name"]`),
		chromedp.Evaluate(`document.querySelector('th[data-sort-key="name"]').getAttribute("aria-sort")`, &ariaSort),
	); err != nil {
		t.Fatalf("sort slugs descending: %v", err)
	}
	if ariaSort != "descending" {
		t.Fatalf("slug descending aria-sort = %q, want descending", ariaSort)
	}

	// R-8AYY-OHC7
	if err := chromedp.Run(session,
		chromedp.Click(`#site-clear`),
		chromedp.Evaluate(`document.querySelector("#site-search").value`, &searchValue),
		readSlugs,
		chromedp.Text(`#pager-label`, &pagerLabel),
	); err != nil {
		t.Fatalf("clear landing controls: %v", err)
	}
	if searchValue != "" || len(slugs) != 10 || pagerLabel != "Page 1 of 2" || slugs[0] != "dashboard" {
		t.Fatalf("clear state = search %q, slugs %#v, pager %q; want default first page", searchValue, slugs, pagerLabel)
	}

	// R-8DER-G0TL
	if err := chromedp.Run(session,
		chromedp.Click(`#pager-next`),
		chromedp.Text(`#pager-label`, &pagerLabel),
		chromedp.Evaluate(`document.querySelectorAll("tbody tr").length`, &rowCount),
	); err != nil {
		t.Fatalf("go to second page: %v", err)
	}
	if pagerLabel != "Page 2 of 2" || rowCount != 2 {
		t.Fatalf("second page = %q with %d rows, want Page 2 of 2 with 2 rows", pagerLabel, rowCount)
	}
	if err := chromedp.Run(session, chromedp.Click(`#pager-prev`), chromedp.Text(`#pager-label`, &pagerLabel)); err != nil {
		t.Fatalf("return to first page: %v", err)
	}
	if pagerLabel != "Page 1 of 2" {
		t.Fatalf("previous page label = %q, want Page 1 of 2", pagerLabel)
	}

	// R-NN9H-UKP3
	wantURL := "http://suite.test/srv/sites/public/dashboard/"
	copySelector := `button.copy-btn[data-url="` + wantURL + `"]`
	var clipboardText, copiedLabel, iconNamespace, rowURL string
	var iconWidth, iconHeight, tableWidthBefore, tableWidthAfter float64

	// R-VYEF-053C
	if err := chromedp.Run(session,
		browser.SetPermission(&browser.PermissionDescriptor{Name: "clipboard-read"}, browser.PermissionSettingGranted).WithOrigin(srv.URL),
		browser.SetPermission(&browser.PermissionDescriptor{Name: "clipboard-write"}, browser.PermissionSettingGranted).WithOrigin(srv.URL),
		chromedp.Evaluate(`document.querySelector('`+copySelector+`').closest('tr').querySelector('td:first-child a').href`, &rowURL),
		chromedp.Evaluate(`document.querySelector('`+copySelector+` svg').namespaceURI`, &iconNamespace),
		chromedp.Evaluate(`document.querySelector('`+copySelector+` svg').getBoundingClientRect().width`, &iconWidth),
		chromedp.Evaluate(`document.querySelector('`+copySelector+` svg').getBoundingClientRect().height`, &iconHeight),
		// R-VZMB-DWU1
		chromedp.Evaluate(`document.querySelector('.site-table').getBoundingClientRect().width`, &tableWidthBefore),
		chromedp.Click(copySelector),
		chromedp.ActionFunc(func(ctx context.Context) error {
			result, exception, err := runtime.Evaluate(`navigator.clipboard.readText()`).WithAwaitPromise(true).WithReturnByValue(true).Do(ctx)
			if err != nil {
				return err
			}
			if exception != nil {
				return exception
			}
			return json.Unmarshal(result.Value, &clipboardText)
		}),
		chromedp.Evaluate(`document.querySelector('`+copySelector+` .copy-label').textContent`, &copiedLabel),
		chromedp.Evaluate(`document.querySelector('.site-table').getBoundingClientRect().width`, &tableWidthAfter),
	); err != nil {
		t.Fatalf("copy dashboard URL to clipboard: %v", err)
	}
	if rowURL != wantURL {
		t.Fatalf("dashboard row URL = %q, want %q", rowURL, wantURL)
	}
	if clipboardText != rowURL {
		t.Fatalf("clipboard text = %q, want row URL %q", clipboardText, rowURL)
	}
	if copiedLabel != "Copied" {
		t.Fatalf("copy button label = %q, want Copied", copiedLabel)
	}
	if iconNamespace != "http://www.w3.org/2000/svg" || iconWidth <= 0 || iconHeight <= 0 {
		t.Fatalf("copy icon = namespace %q, %gx%g; want SVG namespace with non-zero box", iconNamespace, iconWidth, iconHeight)
	}
	if tableWidthAfter != tableWidthBefore {
		t.Fatalf("table width after copy = %g, want unchanged %g", tableWidthAfter, tableWidthBefore)
	}
}

func TestProductionImportGraphExcludesBrowserTestDependencies(t *testing.T) {
	// R-8EMN-TSKA
	cmd := exec.Command("go", "list", "-deps", "./cmd/sites")
	cmd.Dir = filepath.Join("..", "..")
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("go list production dependencies: %v\n%s", err, output)
	}
	for _, forbidden := range []string{"github.com/chromedp/chromedp", "github.com/dop251/goja"} {
		if strings.Contains(string(output), forbidden) {
			t.Fatalf("production import graph contains test-only dependency %q:\n%s", forbidden, output)
		}
	}
}

func landingBrowserSession(t *testing.T) (context.Context, context.CancelFunc) {
	t.Helper()
	timeout, cancelTimeout := context.WithTimeout(context.Background(), 30*time.Second)
	for attempt := 1; attempt <= 2; attempt++ {
		allocator, cancelAllocator := chromedp.NewExecAllocator(timeout, chromedp.DefaultExecAllocatorOptions[:]...)
		session, cancelSession := chromedp.NewContext(allocator)
		if err := chromedp.Run(session); err == nil {
			return session, func() {
				cancelSession()
				cancelAllocator()
				cancelTimeout()
			}
		} else {
			cancelSession()
			cancelAllocator()
			if attempt == 2 {
				cancelTimeout()
				t.Fatalf("launch Chrome after %d attempts: %v", attempt, err)
			}
		}
	}
	t.Fatal("unreachable Chrome launch state")
	return nil, nil
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

func landingData(service, version string) landingView {
	return landingView{
		Service: service,
		Version: version,
	}
}

type landingSeed struct {
	name      string
	public    bool
	createdAt string
}

func newLandingTestStore(t *testing.T, seeds ...landingSeed) *sitesdomain.Store {
	t.Helper()
	path := filepath.Join(t.TempDir(), "sites_test.db")
	conn, err := sqlkit.Open(path)
	if err != nil {
		t.Fatalf("open test db: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := sqlkit.LoadMigrations(sitedb.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := sqlkit.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate test db: %v", err)
	}
	for _, seed := range seeds {
		public := 0
		if seed.public {
			public = 1
		}
		createdAt := seed.createdAt
		if createdAt == "" {
			createdAt = "2026-07-08T12:00:00.000000000Z"
		}
		_, err := conn.ExecContext(context.Background(),
			`INSERT INTO sites (name, source_path, public, created_by, created_at, updated_at)
			 VALUES (?, NULL, ?, ?, ?, ?)`,
			seed.name,
			public,
			seed.name+"@example.com",
			createdAt,
			createdAt,
		)
		if err != nil {
			t.Fatalf("seed site %q: %v", seed.name, err)
		}
	}
	return sitesdomain.NewStore(conn)
}

func composedMux(t *testing.T, mcp http.Handler) *http.ServeMux {
	t.Helper()
	mux := http.NewServeMux()
	mux.Handle("GET /{$}", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		_ = loadWWW(t).Render(w, "landing.html", landingData("sites", "1.2.3"))
	}))
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

func readFile(t *testing.T, name string) []byte {
	t.Helper()
	body, err := os.ReadFile(name)
	if err != nil {
		t.Fatal(err)
	}
	return body
}

func htmlHead(t *testing.T, body string) string {
	t.Helper()
	start := strings.Index(body, "<head>")
	end := strings.Index(body, "</head>")
	if start == -1 || end == -1 || end < start {
		t.Fatalf("HTML does not contain a closed head: %q", body)
	}
	return body[start : end+len("</head>")]
}

func readNginxConfig(t *testing.T) string {
	t.Helper()
	body, err := os.ReadFile(filepath.Join("..", "..", "etc", "nginx.conf"))
	if err != nil {
		t.Fatalf("read nginx fragment: %v", err)
	}
	return string(body)
}

func nginxLocationBlock(t *testing.T, conf, prefix string) string {
	t.Helper()
	start := strings.Index(conf, prefix+" {")
	if start == -1 {
		t.Fatalf("nginx fragment is missing %q location", prefix)
	}

	depth := 0
	for i := start; i < len(conf); i++ {
		switch conf[i] {
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				return conf[start : i+1]
			}
		}
	}
	t.Fatalf("nginx location %q is not closed", prefix)
	return ""
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

func manifestExtras(in []appkit.ManifestKV) []manifest.KV {
	out := make([]manifest.KV, 0, len(in))
	for _, kv := range in {
		out = append(out, manifest.KV{Key: kv.Key, Value: kv.Value})
	}
	return out
}

func testEnv(overrides map[string]string, remove ...string) []string {
	env := os.Environ()
	out := make([]string, 0, len(env)+len(overrides))
	removed := make(map[string]bool, len(remove))
	for _, key := range remove {
		removed[key] = true
	}
	for _, kv := range env {
		key, _, _ := strings.Cut(kv, "=")
		if _, ok := overrides[key]; ok || removed[key] {
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
			t.Fatalf("sites exited before health: %v\nstdout:\n%s\nstderr:\n%s", err, stdout.String(), stderr.String())
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
	t.Fatalf("sites never served health at %s: %s\nstdout:\n%s\nstderr:\n%s", url, last, stdout.String(), stderr.String())
	return nil
}

func stopProcess(cancel context.CancelFunc, done <-chan error) {
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
	}
}
