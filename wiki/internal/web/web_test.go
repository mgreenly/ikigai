package web

import (
	"context"
	"database/sql"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	appdb "appkit/db"
	appkitweb "appkit/web"

	"wiki/internal/ask"
	wikidb "wiki/internal/db"
	wikidomain "wiki/internal/wiki"
)

type stubOrphanLister struct {
	refs   []Ref
	called int
}

func (s *stubOrphanLister) Orphans(context.Context) ([]Ref, error) {
	s.called++
	return s.refs, nil
}

type stubAsker struct {
	answer   ask.Answer
	called   int
	owner    string
	question string
}

func (s *stubAsker) Ask(_ context.Context, owner, question string) (ask.Answer, error) {
	s.called++
	s.owner = owner
	s.question = question
	return s.answer, nil
}

type stubMentioner struct {
	refs   []Ref
	called int
	text   string
}

func (s *stubMentioner) MentionsIn(_ context.Context, text string) ([]Ref, error) {
	s.called++
	s.text = text
	return s.refs, nil
}

type stubPageFinder struct {
	view   SubjectView
	err    error
	called int
	paths  []string
}

type linkifiedPageFinder struct {
	resolver *wikidomain.Resolver
	service  *wikidomain.Service
	base     string
}

func (f linkifiedPageFinder) PageByPath(ctx context.Context, path string) (SubjectView, error) {
	subject, err := f.resolver.ResolveByPath(ctx, path)
	if err != nil {
		return SubjectView{}, err
	}
	page, err := f.service.PageWithLinks(ctx, subject.ID)
	if err != nil {
		return SubjectView{}, err
	}
	body, err := f.service.LinkifyMentions(ctx, page.Body, f.base, subject.ID)
	if err != nil {
		return SubjectView{}, err
	}
	return SubjectView{SubjectID: subject.ID, Path: wikidomain.Path(subject), Title: page.Title, Body: body}, nil
}

func migratedWebDB(t *testing.T, ctx context.Context) *sql.DB {
	t.Helper()

	conn, err := appdb.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	migrations, err := appdb.LoadMigrations(wikidb.FS, "migrations")
	if err != nil {
		conn.Close()
		t.Fatalf("LoadMigrations: %v", err)
	}
	if err := appdb.Migrate(ctx, conn, migrations); err != nil {
		conn.Close()
		t.Fatalf("Migrate: %v", err)
	}
	return conn
}

func (s *stubPageFinder) PageByPath(_ context.Context, path string) (SubjectView, error) {
	s.called++
	s.paths = append(s.paths, path)
	return s.view, s.err
}

func readAsset(t *testing.T, name string) string {
	t.Helper()

	b, err := os.ReadFile(filepath.Join(testWWWPath(), name))
	if err != nil {
		t.Fatalf("read www asset %s: %v", name, err)
	}
	return string(b)
}

func testWWWPath() string {
	return filepath.Join("..", "..", "share", "www")
}

func loadTestSite(t *testing.T) *appkitweb.Site {
	t.Helper()

	site, err := appkitweb.Load(testWWWPath())
	if err != nil {
		t.Fatalf("load test site: %v", err)
	}
	return site
}

func newTestHandler(t *testing.T, service, version, mount string, opts ...Option) http.Handler {
	t.Helper()
	return NewHandler(service, version, mount, loadTestSite(t), opts...)
}

func readSurfaceBody(t *testing.T, path string, opts ...Option) string {
	t.Helper()

	req := httptest.NewRequest(http.MethodGet, path, nil)
	rec := httptest.NewRecorder()
	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", opts...).ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("%s status = %d, want 200; body=%s", path, rec.Code, rec.Body.String())
	}
	return rec.Body.String()
}

func TestHomeHandlerServesExactRootHTML(t *testing.T) {
	// R-LAND-PG01
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
	if !strings.Contains(rec.Body.String(), "<!doctype html>") {
		t.Fatalf("body does not look like HTML: %s", rec.Body.String())
	}
}

func TestHomeHandlerRendersInjectedNameAndVersionInFooter(t *testing.T) {
	// R-LAND-NMVR
	const service = "wiki-service"
	const version = "v69-home-test"
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, service, version, "/srv/wiki/").ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, "<footer>"+service+" "+version+"</footer>") {
		t.Fatalf("body does not contain service/version footer %q %q: %s", service, version, body)
	}
}

func TestHomeHandlerRendersInjectedMountBase(t *testing.T) {
	// R-WDA6-B2C8
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/zzz/").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	if !strings.Contains(rec.Body.String(), `<base href="/srv/zzz/">`) {
		t.Fatalf("body does not contain injected base href: %s", rec.Body.String())
	}
}

func TestHomeHandlerRendersSearchFormTargetingBase(t *testing.T) {
	// R-OMRY-L9O8
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/").ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, `<base href="/srv/wiki/">`) {
		t.Fatalf("body missing base href: %s", body)
	}
	if !strings.Contains(body, `<form action="" method="get" role="search">`) {
		t.Fatalf("body missing GET search form with empty action: %s", body)
	}
	if got := strings.Count(body, `type="text" name="q"`); got != 1 {
		t.Fatalf("q text input count = %d, want 1; body=%s", got, body)
	}
}

func TestAskHandlerCallsAskerWithQuestionAndOwner(t *testing.T) {
	// R-WFPZ-2LTM
	asker := &stubAsker{answer: ask.Answer{
		Found: true,
		Text:  "Grace owns the scheduler.",
	}}
	req := httptest.NewRequest(http.MethodGet, "/?q=Who+owns+the+scheduler%3F", nil)
	req.Header.Set("X-Owner-Email", "owner@example.com")
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	if asker.called != 1 {
		t.Fatalf("Ask calls = %d, want 1", asker.called)
	}
	if asker.owner != "owner@example.com" || asker.question != "Who owns the scheduler?" {
		t.Fatalf("Ask called with owner=%q question=%q, want owner header and trimmed q", asker.owner, asker.question)
	}
}

func TestAskHandlerRendersAnswerAndCitationsAsSubjectLinks(t *testing.T) {
	// R-ARN9-5YPS
	asker := &stubAsker{answer: ask.Answer{
		Found: true,
		Text:  "Grace owns the scheduler.",
		Citations: []ask.Citation{
			{Path: "entity/grace-hopper", Title: "Grace Hopper"},
		},
	}}
	req := httptest.NewRequest(http.MethodGet, "/?q=scheduler", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{
		"<article",
		"Grace owns the scheduler.",
		`<a href="subject/entity/grace-hopper">Grace Hopper</a>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("ask page missing %q: %s", want, body)
		}
	}
}

func TestAskHandlerRendersMarkdownAnswerHTML(t *testing.T) {
	// R-NPVU-26CX
	asker := &stubAsker{answer: ask.Answer{
		Found: true,
		Text:  "**Acme** makes widgets.",
	}}
	req := httptest.NewRequest(http.MethodGet, "/?q=widgets", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, "<strong>Acme</strong>") {
		t.Fatalf("ask page missing rendered strong markdown: %s", body)
	}
	if strings.Contains(body, "**Acme**") {
		t.Fatalf("ask page rendered literal markdown: %s", body)
	}
}

func TestAskHandlerLinkifiesFirstMentionInAnswerProse(t *testing.T) {
	// R-8FQU-M1J4
	ctx := context.Background()
	conn := migratedWebDB(t, ctx)
	defer conn.Close()

	if err := wikidomain.NewSubjectStore(conn).Save(ctx, wikidomain.Subject{
		ID: "subject-acme", Name: "Acme Corp", Type: "entity",
	}); err != nil {
		t.Fatalf("Save subject: %v", err)
	}
	if err := wikidomain.NewPageStore(conn).Upsert(ctx, wikidomain.Page{
		ID: "page-acme", SubjectID: "subject-acme", Title: "Acme Corp", Body: "Acme Corp makes widgets.",
	}); err != nil {
		t.Fatalf("Upsert page: %v", err)
	}
	service := wikidomain.NewService(conn, nil, nil, time.Now)
	base := "https://acct.ikigenba.com/srv/wiki/subject/"
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/",
		WithAsker(&stubAsker{answer: ask.Answer{Found: true, Text: "Acme Corp makes widgets."}}),
		WithLinkifier(service, base),
	).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/?q=widgets", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	want := `<a href="https://acct.ikigenba.com/srv/wiki/subject/entity/acme-corp" rel="nofollow">Acme Corp</a>`
	if !strings.Contains(rec.Body.String(), want) {
		t.Fatalf("answer prose missing first-mention link %q: %s", want, rec.Body.String())
	}
}

func TestAskHandlerRendersMentionedSubjectsFromAnswerText(t *testing.T) {
	// R-AU31-XI76
	asker := &stubAsker{answer: ask.Answer{
		Found: true,
		Text:  "Acme Corp uses the Widget.",
	}}
	mentioner := &stubMentioner{refs: []Ref{
		{Href: "https://acct.ikigenba.com/srv/wiki/subject/entity/acme-corp", Name: "Acme Corp"},
		{Href: "https://acct.ikigenba.com/srv/wiki/subject/concept/widget", Name: "Widget"},
	}}
	req := httptest.NewRequest(http.MethodGet, "/?q=tools", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker), WithMentioner(mentioner)).ServeHTTP(rec, req)

	body := rec.Body.String()
	if mentioner.called != 1 || mentioner.text != "Acme Corp uses the Widget." {
		t.Fatalf("MentionsIn called %d with %q, want answer text", mentioner.called, mentioner.text)
	}
	for _, want := range []string{
		`<a href="https://acct.ikigenba.com/srv/wiki/subject/entity/acme-corp">Acme Corp</a>`,
		`<a href="https://acct.ikigenba.com/srv/wiki/subject/concept/widget">Widget</a>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("ask page missing mention link %q: %s", want, body)
		}
	}
}

func TestAskHandlerRendersHonestEmptyWithoutCitationNav(t *testing.T) {
	// R-ASV5-JQGH
	asker := &stubAsker{answer: ask.Answer{
		Found: false,
		Text:  "The wiki holds nothing on that question.",
	}}
	req := httptest.NewRequest(http.MethodGet, "/?q=unknown", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, "The wiki holds nothing on that question.") {
		t.Fatalf("ask page missing honest-empty text: %s", body)
	}
	if strings.Contains(body, `aria-label="Cited pages"`) {
		t.Fatalf("ask page rendered citation nav for empty answer: %s", body)
	}
}

func TestAskHandlerOmitsMentionSectionWhenAnswerMentionsAreEmpty(t *testing.T) {
	// R-AVAY-B9XV
	asker := &stubAsker{answer: ask.Answer{
		Found: true,
		Text:  "No known subject is named here.",
	}}
	req := httptest.NewRequest(http.MethodGet, "/?q=unknown", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker), WithMentioner(&stubMentioner{})).ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, "No known subject is named here.") {
		t.Fatalf("ask page missing answer text: %s", body)
	}
	if strings.Contains(body, `aria-label="Mentioned subjects"`) {
		t.Fatalf("ask page rendered mention section for empty mentions: %s", body)
	}
}

func TestBlankQueryKeepsHomePageOnOrphanSeam(t *testing.T) {
	asker := &stubAsker{}
	lister := &stubOrphanLister{refs: []Ref{{Href: "subject/entity/acme", Name: "Acme"}}}
	req := httptest.NewRequest(http.MethodGet, "/?q=+++", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker), WithOrphanLister(lister)).ServeHTTP(rec, req)

	if asker.called != 0 {
		t.Fatalf("Ask calls = %d, want blank q to stay on home page", asker.called)
	}
	if lister.called != 1 {
		t.Fatalf("Orphans calls = %d, want home page orphan seam", lister.called)
	}
	if !strings.Contains(rec.Body.String(), `<a href="subject/entity/acme">Acme</a>`) {
		t.Fatalf("home page missing orphan link: %s", rec.Body.String())
	}
}

func TestAskAndSubjectPagesWrapRenderedBodiesInProse(t *testing.T) {
	// R-9EPS-LWWY
	asker := &stubAsker{answer: ask.Answer{
		Found: true,
		Text:  "Acme makes widgets.",
	}}
	askReq := httptest.NewRequest(http.MethodGet, "/?q=widgets", nil)
	askRec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(askRec, askReq)

	if !strings.Contains(askRec.Body.String(), `<div class="prose"><p>Acme makes widgets.</p>`) {
		t.Fatalf("ask page missing prose wrapper around rendered answer: %s", askRec.Body.String())
	}

	finder := &stubPageFinder{view: SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."}}
	subjectReq := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	subjectRec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder)).ServeHTTP(subjectRec, subjectReq)

	if !strings.Contains(subjectRec.Body.String(), `<div class="prose"><p>Acme makes widgets.</p>`) {
		t.Fatalf("subject page missing prose wrapper around rendered body: %s", subjectRec.Body.String())
	}
}

func TestHomeHandlerRendersOrphansAsMountRelativeLinksInOrder(t *testing.T) {
	// R-ONZU-Z1EX
	lister := &stubOrphanLister{refs: []Ref{
		{Href: "subject/entity/acme-corp", Name: "Acme Corp"},
		{Href: "subject/event/tulsa-launch", Name: "Tulsa Launch"},
	}}
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithOrphanLister(lister)).ServeHTTP(rec, req)

	body := rec.Body.String()
	if lister.called != 1 {
		t.Fatalf("Orphans called = %d, want 1", lister.called)
	}
	first := `<a href="subject/entity/acme-corp">Acme Corp</a>`
	second := `<a href="subject/event/tulsa-launch">Tulsa Launch</a>`
	firstAt := strings.Index(body, first)
	secondAt := strings.Index(body, second)
	if firstAt < 0 || secondAt < 0 {
		t.Fatalf("body missing orphan links %q and %q: %s", first, second, body)
	}
	if firstAt > secondAt {
		t.Fatalf("orphan links out of order: %s", body)
	}
}

func TestHomeHandlerOmitsOrphanSectionWhenEmpty(t *testing.T) {
	// R-OP7R-CT5M
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithOrphanLister(&stubOrphanLister{})).ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, `<form action="" method="get" role="search">`) {
		t.Fatalf("body missing search form: %s", body)
	}
	if strings.Contains(body, "<nav") || strings.Contains(body, "<ul") {
		t.Fatalf("body contains orphan nav/list despite empty orphans: %s", body)
	}
}

func TestLoadedSiteRendersHomeThroughHandler(t *testing.T) {
	// R-JGZ2-0BMY
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki-surface", "v79-home", "/srv/wiki/").ServeHTTP(rec, req)

	body := rec.Body.String()
	if rec.Code != http.StatusOK {
		t.Fatalf("home status = %d, want 200; body=%s", rec.Code, body)
	}
	if !strings.Contains(body, "<footer>wiki-surface v79-home</footer>") {
		t.Fatalf("home page missing injected service/version footer: %s", body)
	}
	if !strings.Contains(body, `<form action="" method="get" role="search">`) {
		t.Fatalf("home page missing search form: %s", body)
	}
}

func TestLoadedSiteRendersDistinctSubjectPageThroughHandler(t *testing.T) {
	// R-JI6Y-E3DN
	finder := &stubPageFinder{view: SubjectView{
		Title: "Acme Corp",
		Body:  "Acme makes widgets.",
		Outbound: []Ref{
			{Href: "subject/entity/beta", Name: "Beta"},
		},
		Inbound: []Ref{
			{Href: "subject/event/deal-q3", Name: "Deal Q3"},
		},
	}}
	subjectReq := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	subjectRec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder)).ServeHTTP(subjectRec, subjectReq)
	home := readSurfaceBody(t, "/")
	subject := subjectRec.Body.String()
	if subjectRec.Code != http.StatusOK {
		t.Fatalf("subject status = %d, want 200; body=%s", subjectRec.Code, subject)
	}
	for _, want := range []string{
		`<article aria-label="Subject page">`,
		"<h1>Acme Corp</h1>",
		`<div class="prose"><p>Acme makes widgets.</p>`,
		`<nav aria-label="Mentions">`,
		`<a href="subject/entity/beta">Beta</a>`,
		`<nav aria-label="Mentioned by">`,
		`<a href="subject/event/deal-q3">Deal Q3</a>`,
	} {
		if !strings.Contains(subject, want) {
			t.Fatalf("subject page missing %q: %s", want, subject)
		}
	}
	if strings.Contains(subject, `<form action="" method="get" role="search">`) || subject == home {
		t.Fatalf("subject page was not distinct from home:\nsubject=%s\nhome=%s", subject, home)
	}
}

func TestHomeHandlerUsesCarbonAssets(t *testing.T) {
	// R-LAND-CARB
	if _, err := os.Stat(filepath.Join(testWWWPath(), "static", "tokens.css")); err != nil {
		t.Fatalf("www tokens.css missing: %v", err)
	}

	pageReq := httptest.NewRequest(http.MethodGet, "/", nil)
	pageRec := httptest.NewRecorder()
	newTestHandler(t, "wiki", "v-test", "/srv/wiki/").ServeHTTP(pageRec, pageReq)
	if !strings.Contains(pageRec.Body.String(), `href="static/tokens.css"`) {
		t.Fatalf("home page does not reference tokens.css through base-relative href: %s", pageRec.Body.String())
	}
}

func TestChassisStaticServesLoadedSiteAssets(t *testing.T) {
	// R-JJEU-RV4C
	site := loadTestSite(t)
	mux := http.NewServeMux()
	mux.Handle("GET /static/", site.Static())

	wikiMux := newTestHandler(t, "wiki", "v-test", "/srv/wiki/")
	wikiStaticReq := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	wikiStaticRec := httptest.NewRecorder()
	wikiMux.ServeHTTP(wikiStaticRec, wikiStaticReq)
	if wikiStaticRec.Code == http.StatusOK {
		t.Fatalf("wiki-side mux served /static/tokens.css; want chassis-only static mount")
	}

	cssReq := httptest.NewRequest(http.MethodGet, "/static/tokens.css", nil)
	cssRec := httptest.NewRecorder()
	mux.ServeHTTP(cssRec, cssReq)
	if cssRec.Code != http.StatusOK {
		t.Fatalf("tokens.css status = %d, want 200; body=%s", cssRec.Code, cssRec.Body.String())
	}
	if got := cssRec.Header().Get("Content-Type"); !strings.HasPrefix(got, "text/css") {
		t.Fatalf("tokens.css Content-Type = %q, want text/css", got)
	}
	if !strings.Contains(cssRec.Body.String(), "--color-accent") {
		t.Fatalf("tokens.css does not contain Carbon token: %s", cssRec.Body.String())
	}

	fontReq := httptest.NewRequest(http.MethodGet, "/static/fonts/space-grotesk.woff2", nil)
	fontRec := httptest.NewRecorder()
	mux.ServeHTTP(fontRec, fontReq)
	if fontRec.Code != http.StatusOK {
		t.Fatalf("space-grotesk.woff2 status = %d, want 200; body=%s", fontRec.Code, fontRec.Body.String())
	}
	if got := fontRec.Header().Get("Content-Type"); got != "font/woff2" {
		t.Fatalf("space-grotesk.woff2 Content-Type = %q, want font/woff2", got)
	}
}

func TestTokensCSSDoesNotDeclareWebFonts(t *testing.T) {
	// R-KFVF-EMEO
	css := readAsset(t, "static/tokens.css")

	for _, forbidden := range []string{"@font-face", "url(", ".woff2", "static/fonts"} {
		if strings.Contains(css, forbidden) {
			t.Fatalf("tokens.css contains web-font fetch surface %q: %s", forbidden, css)
		}
	}
}

func TestTokensCSSUsesSystemFontAliases(t *testing.T) {
	// R-KH3B-SE5D
	css := readAsset(t, "static/tokens.css")

	for _, want := range []string{
		`--font-display: system-ui, -apple-system`,
		`--font-body:    system-ui, -apple-system`,
		`--font-mono:    ui-monospace`,
	} {
		if !strings.Contains(css, want) {
			t.Fatalf("tokens.css missing system font alias %q: %s", want, css)
		}
	}
	for _, forbidden := range []string{"Space Grotesk", "IBM Plex Sans", "IBM Plex Mono"} {
		if strings.Contains(css, forbidden) {
			t.Fatalf("tokens.css still names vendored font family %q: %s", forbidden, css)
		}
	}
}

func TestReadLayoutDoesNotEmitFontResourceHints(t *testing.T) {
	// R-KIB8-65W2
	body := readSurfaceBody(t, "/")

	if got := strings.Count(body, `href="static/tokens.css"`); got != 1 {
		t.Fatalf("tokens.css link count = %d, want 1; body=%s", got, body)
	}
	for _, forbidden := range []string{`rel="preload"`, `rel="preconnect"`, `as="font"`, "static/fonts", ".woff2"} {
		if strings.Contains(body, forbidden) {
			t.Fatalf("read layout contains font resource hint/reference %q: %s", forbidden, body)
		}
	}
}

func TestReadSurfacesUseSystemFontFallbacks(t *testing.T) {
	// R-KJJ4-JXMR
	bodies := map[string]string{
		"home": readSurfaceBody(t, "/"),
		"ask": readSurfaceBody(t, "/?q=widgets", WithAsker(&stubAsker{answer: ask.Answer{
			Found: true,
			Text:  "Acme makes widgets.",
		}})),
		"subject": readSurfaceBody(t, "/subject/entity/acme-corp", WithPageFinder(&stubPageFinder{
			view: SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."},
		})),
	}

	for name, body := range bodies {
		for _, want := range []string{
			`--read-font-body: system-ui, -apple-system`,
			`--read-font-mono: ui-monospace`,
			`font-family: var(--font-body, var(--read-font-body))`,
			`font-family: var(--font-display, var(--read-font-body))`,
			`font-family: var(--font-mono, var(--read-font-mono))`,
		} {
			if !strings.Contains(body, want) {
				t.Fatalf("%s read surface missing system fallback %q: %s", name, want, body)
			}
		}
		for _, forbidden := range []string{"Space Grotesk", "IBM Plex Sans", "IBM Plex Mono", "static/fonts", ".woff2"} {
			if strings.Contains(body, forbidden) {
				t.Fatalf("%s read surface contains vendored font reference %q: %s", name, forbidden, body)
			}
		}
	}
}

func TestHomeMuxDoesNotServeRootPageForOtherPaths(t *testing.T) {
	// R-LAND-ROOT
	mux := newTestHandler(t, "wiki", "v-test", "/srv/wiki/")

	for _, path := range []string{"/health", "/mcp", "/nope"} {
		req := httptest.NewRequest(http.MethodGet, path, nil)
		rec := httptest.NewRecorder()
		mux.ServeHTTP(rec, req)
		if rec.Code == http.StatusOK && strings.Contains(rec.Body.String(), `<form action="" method="get" role="search">`) {
			t.Fatalf("%s received home page unexpectedly: status=%d body=%s", path, rec.Code, rec.Body.String())
		}
	}
}

func TestHomeHandlerIsAvailableWithoutIdentityHeaders(t *testing.T) {
	// R-LAND-UNGT
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200 without identity headers; body=%s", rec.Code, rec.Body.String())
	}
	if req.Header.Get("X-Owner-Email") != "" || req.Header.Get("Authorization") != "" {
		t.Fatal("test request unexpectedly carried identity or bearer headers")
	}
}

func TestSubjectMuxDispatchesOnlyTwoSegmentSubjectPaths(t *testing.T) {
	// R-WC29-XALJ
	finder := &stubPageFinder{view: SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."}}
	mux := newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder))

	req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("valid subject status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	if finder.called != 1 {
		t.Fatalf("PageByPath calls after valid subject = %d, want 1", finder.called)
	}

	for _, path := range []string{"/subject/onlyoneseg", "/mcp", "/health", "/feed", "/static/x"} {
		req := httptest.NewRequest(http.MethodGet, path, nil)
		rec := httptest.NewRecorder()
		mux.ServeHTTP(rec, req)
		if rec.Code == http.StatusOK {
			t.Fatalf("%s status = 200, want non-OK from mux/static miss", path)
		}
	}
	if finder.called != 1 {
		t.Fatalf("PageByPath calls after unrelated paths = %d, want still 1", finder.called)
	}
}

func TestSubjectHandlerCallsPageFinderWithPublicPathOnce(t *testing.T) {
	// R-WGXV-GDKB
	finder := &stubPageFinder{view: SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."}}
	req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder)).ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	if finder.called != 1 || len(finder.paths) != 1 || finder.paths[0] != "entity/acme-corp" {
		t.Fatalf("PageByPath calls=%d paths=%v, want exactly entity/acme-corp once", finder.called, finder.paths)
	}
}

func TestSubjectHandlerRendersTitleAndBody(t *testing.T) {
	// R-PH2F-47LB
	finder := &stubPageFinder{view: SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."}}
	req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder)).ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{"<h1>Acme Corp</h1>", "<p>Acme makes widgets.</p>"} {
		if !strings.Contains(body, want) {
			t.Fatalf("subject page missing %q: %s", want, body)
		}
	}
}

func TestSubjectHandlerRendersMarkdownBodyHTML(t *testing.T) {
	// R-NONX-OEM8
	finder := &stubPageFinder{view: SubjectView{
		Title: "Acme Corp",
		Body:  "## Overview\n\nAcme makes **widgets**.",
	}}
	req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder)).ServeHTTP(rec, req)

	body := rec.Body.String()
	for _, want := range []string{"<h2", "<strong>widgets</strong>"} {
		if !strings.Contains(body, want) {
			t.Fatalf("subject page missing rendered markdown %q: %s", want, body)
		}
	}
	if strings.Contains(body, "## Overview") || strings.Contains(body, "**widgets**") {
		t.Fatalf("subject page rendered literal markdown: %s", body)
	}
}

func TestSubjectHandlerLinkifiesOtherSubjectButNotOwnNameInProse(t *testing.T) {
	// R-8GYQ-ZT9T
	ctx := context.Background()
	conn := migratedWebDB(t, ctx)
	defer conn.Close()

	subjects := wikidomain.NewSubjectStore(conn)
	for _, subject := range []wikidomain.Subject{
		{ID: "subject-acme", Name: "Acme Corp", Type: "entity"},
		{ID: "subject-beta", Name: "Beta", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save subject %s: %v", subject.ID, err)
		}
	}
	pages := wikidomain.NewPageStore(conn)
	for _, page := range []wikidomain.Page{
		{ID: "page-acme", SubjectID: "subject-acme", Title: "Acme Corp", Body: "Acme Corp works with Beta."},
		{ID: "page-beta", SubjectID: "subject-beta", Title: "Beta", Body: "Beta is a partner."},
	} {
		if err := pages.Upsert(ctx, page); err != nil {
			t.Fatalf("Upsert page %s: %v", page.ID, err)
		}
	}
	base := "https://acct.ikigenba.com/srv/wiki/subject/"
	service := wikidomain.NewService(conn, nil, nil, time.Now)
	finder := linkifiedPageFinder{
		resolver: wikidomain.NewResolver(conn),
		service:  service,
		base:     base,
	}
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder)).ServeHTTP(
		rec, httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil),
	)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	if want := `<a href="https://acct.ikigenba.com/srv/wiki/subject/entity/beta" rel="nofollow">Beta</a>`; !strings.Contains(body, want) {
		t.Fatalf("subject prose missing other-subject link %q: %s", want, body)
	}
	if strings.Contains(body, `href="https://acct.ikigenba.com/srv/wiki/subject/entity/acme-corp"`) {
		t.Fatalf("subject prose linked its own name: %s", body)
	}
}

func TestSubjectHandlerRendersFoundHTMLShell(t *testing.T) {
	// R-PH2F-47LB
	req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(&stubPageFinder{
		view: SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."},
	})).ServeHTTP(rec, req)

	body := rec.Body.String()
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, body)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
	for _, want := range []string{"<!doctype html>", `<base href="/srv/wiki/">`, "<footer>wiki v-test</footer>"} {
		if !strings.Contains(body, want) {
			t.Fatalf("found subject page missing %q: %s", want, body)
		}
	}
}

func TestSubjectHandlerRendersMentionsBeforeMentionedBy(t *testing.T) {
	// R-PIAB-HZC0
	finder := &stubPageFinder{view: SubjectView{
		Title: "Acme Corp",
		Body:  "Acme makes widgets.",
		Outbound: []Ref{
			{Href: "https://acct.ikigenba.com/srv/wiki/subject/entity/beta", Name: "Beta"},
		},
		Inbound: []Ref{
			{Href: "https://acct.ikigenba.com/srv/wiki/subject/event/deal-q3", Name: "Deal Q3"},
		},
	}}
	req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(finder)).ServeHTTP(rec, req)

	body := rec.Body.String()
	mentionsAt := strings.Index(body, `<nav aria-label="Mentions">`)
	mentionedByAt := strings.Index(body, `<nav aria-label="Mentioned by">`)
	if mentionsAt < 0 || mentionedByAt < 0 || mentionsAt > mentionedByAt {
		t.Fatalf("subject page did not render Mentions before Mentioned by: %s", body)
	}
	for _, want := range []string{
		`<a href="https://acct.ikigenba.com/srv/wiki/subject/entity/beta">Beta</a>`,
		`<a href="https://acct.ikigenba.com/srv/wiki/subject/event/deal-q3">Deal Q3</a>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("subject page missing link %q: %s", want, body)
		}
	}
}

func TestWebNavigationStaysRelativeBesideAbsoluteSubjectLinks(t *testing.T) {
	// R-8JEJ-RCR7
	base := "https://acct.ikigenba.com/srv/wiki/subject/"

	t.Run("answer", func(t *testing.T) {
		rec := httptest.NewRecorder()
		newTestHandler(t, "wiki", "v-test", "/srv/wiki/",
			WithAsker(&stubAsker{answer: ask.Answer{Found: true, Text: "TSR is ready."}}),
			WithMentioner(&stubMentioner{refs: []Ref{{Href: base + "entity/tsr", Name: "TSR"}}}),
		).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/?q=tsr", nil))

		body := rec.Body.String()
		if !strings.Contains(body, `<a href="https://acct.ikigenba.com/srv/wiki/subject/entity/tsr">TSR</a>`) {
			t.Fatalf("answer page missing absolute subject link: %s", body)
		}
		if !strings.Contains(body, `<form action="" method="get" role="search">`) || strings.Contains(body, `action="https://acct.ikigenba.com/srv/wiki/"`) {
			t.Fatalf("answer navigation was absolutized: %s", body)
		}
	})

	t.Run("subject", func(t *testing.T) {
		rec := httptest.NewRecorder()
		newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(&stubPageFinder{view: SubjectView{
			Title: "Acme Corp",
			Body:  "Acme uses TSR.",
			Outbound: []Ref{{
				Href: base + "entity/tsr",
				Name: "TSR",
			}},
		}})).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil))

		body := rec.Body.String()
		if !strings.Contains(body, `<a href="https://acct.ikigenba.com/srv/wiki/subject/entity/tsr">TSR</a>`) {
			t.Fatalf("subject page missing absolute subject link: %s", body)
		}
		if !strings.Contains(body, `<a href="">Ask another question</a>`) || strings.Contains(body, `href="https://acct.ikigenba.com/srv/wiki/">Ask another question</a>`) {
			t.Fatalf("subject navigation was absolutized: %s", body)
		}
	})
}

func TestSubjectHandlerOmitsEmptyLinkSections(t *testing.T) {
	// R-PJI7-VR2P
	for _, tc := range []struct {
		name           string
		view           SubjectView
		forbiddenLabel string
	}{
		{
			name: "outbound only",
			view: SubjectView{
				Title:    "Acme Corp",
				Body:     "Acme makes widgets.",
				Outbound: []Ref{{Href: "subject/entity/beta", Name: "Beta"}},
			},
			forbiddenLabel: `<nav aria-label="Mentioned by">`,
		},
		{
			name: "inbound only",
			view: SubjectView{
				Title:   "Acme Corp",
				Body:    "Acme makes widgets.",
				Inbound: []Ref{{Href: "subject/event/deal-q3", Name: "Deal Q3"}},
			},
			forbiddenLabel: `<nav aria-label="Mentions">`,
		},
		{
			name:           "no links",
			view:           SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."},
			forbiddenLabel: `<nav aria-label=`,
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
			rec := httptest.NewRecorder()

			newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(&stubPageFinder{view: tc.view})).ServeHTTP(rec, req)

			body := rec.Body.String()
			if !strings.Contains(body, "<p>Acme makes widgets.</p>") {
				t.Fatalf("subject page missing prose: %s", body)
			}
			if strings.Contains(body, tc.forbiddenLabel) {
				t.Fatalf("subject page rendered forbidden empty section %q: %s", tc.forbiddenLabel, body)
			}
		})
	}
}

func TestSubjectHandlerLinksAskAnotherQuestionOnFoundAndNotFound(t *testing.T) {
	// R-PKQ4-9ITE
	for _, tc := range []struct {
		name   string
		finder *stubPageFinder
	}{
		{
			name:   "found",
			finder: &stubPageFinder{view: SubjectView{Title: "Acme Corp", Body: "Acme makes widgets."}},
		},
		{
			name:   "not found",
			finder: &stubPageFinder{err: ErrNotFound},
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodGet, "/subject/entity/acme-corp", nil)
			rec := httptest.NewRecorder()

			newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(tc.finder)).ServeHTTP(rec, req)

			if !strings.Contains(rec.Body.String(), `<a href="">Ask another question</a>`) {
				t.Fatalf("subject page missing ask-another link to base: %s", rec.Body.String())
			}
		})
	}
}

func TestSubjectHandlerRendersNotFoundHTMLShell(t *testing.T) {
	// R-PLY0-NAK3
	req := httptest.NewRequest(http.MethodGet, "/subject/entity/missing", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/", WithPageFinder(&stubPageFinder{err: ErrNotFound})).ServeHTTP(rec, req)

	body := rec.Body.String()
	if rec.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want 404; body=%s", rec.Code, body)
	}
	if got := rec.Header().Get("Content-Type"); got != "text/html; charset=utf-8" {
		t.Fatalf("Content-Type = %q, want text/html; charset=utf-8", got)
	}
	for _, want := range []string{"<!doctype html>", `<base href="/srv/wiki/">`, "Subject not found", "<footer>wiki v-test</footer>"} {
		if !strings.Contains(body, want) {
			t.Fatalf("not-found page missing %q: %s", want, body)
		}
	}
	if strings.Contains(body, "404 page not found") {
		t.Fatalf("not-found page used plaintext default body: %s", body)
	}
}

func TestLayoutProseStylesMarkdownElementsWithTokens(t *testing.T) {
	// R-9FXO-ZONN
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	newTestHandler(t, "wiki", "v-test", "/srv/wiki/").ServeHTTP(rec, req)

	styleStart := strings.Index(rec.Body.String(), "<style>")
	styleEnd := strings.Index(rec.Body.String(), "</style>")
	if styleStart < 0 || styleEnd < 0 || styleEnd <= styleStart {
		t.Fatalf("served page missing inline style: %s", rec.Body.String())
	}
	style := rec.Body.String()[styleStart:styleEnd]
	proseStart := strings.Index(style, ".prose")
	proseEnd := strings.Index(style, "    form {")
	if proseStart < 0 || proseEnd < 0 || proseEnd <= proseStart {
		t.Fatalf("inline style missing prose rule block: %s", style)
	}
	proseRules := style[proseStart:proseEnd]
	for _, want := range []string{
		".prose h1",
		".prose h6",
		".prose ul",
		".prose ol",
		".prose li",
		".prose code",
		".prose pre",
		".prose blockquote",
		".prose table",
	} {
		if !strings.Contains(proseRules, want) {
			t.Fatalf("prose styles missing selector %q: %s", want, proseRules)
		}
	}
	if !strings.Contains(proseRules, "var(--") {
		t.Fatalf("prose styles do not reference design tokens: %s", proseRules)
	}
}
