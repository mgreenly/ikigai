package web

import (
	"context"
	"io/fs"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"wiki/internal/ask"
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

func TestHomeHandlerServesExactRootHTML(t *testing.T) {
	// R-LAND-PG01
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	NewHandler("wiki", "v-test", "/srv/wiki/").ServeHTTP(rec, req)

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

	NewHandler(service, version, "/srv/wiki/").ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, "<footer>"+service+" "+version+"</footer>") {
		t.Fatalf("body does not contain service/version footer %q %q: %s", service, version, body)
	}
}

func TestHomeHandlerRendersInjectedMountBase(t *testing.T) {
	// R-WDA6-B2C8
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	NewHandler("wiki", "v-test", "/srv/zzz/").ServeHTTP(rec, req)

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

	NewHandler("wiki", "v-test", "/srv/wiki/").ServeHTTP(rec, req)

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

	NewHandler("wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(rec, req)

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

	NewHandler("wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(rec, req)

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

func TestAskHandlerRendersMentionedSubjectsFromAnswerText(t *testing.T) {
	// R-AU31-XI76
	asker := &stubAsker{answer: ask.Answer{
		Found: true,
		Text:  "Acme Corp uses the Widget.",
	}}
	mentioner := &stubMentioner{refs: []Ref{
		{Href: "subject/entity/acme-corp", Name: "Acme Corp"},
		{Href: "subject/concept/widget", Name: "Widget"},
	}}
	req := httptest.NewRequest(http.MethodGet, "/?q=tools", nil)
	rec := httptest.NewRecorder()

	NewHandler("wiki", "v-test", "/srv/wiki/", WithAsker(asker), WithMentioner(mentioner)).ServeHTTP(rec, req)

	body := rec.Body.String()
	if mentioner.called != 1 || mentioner.text != "Acme Corp uses the Widget." {
		t.Fatalf("MentionsIn called %d with %q, want answer text", mentioner.called, mentioner.text)
	}
	for _, want := range []string{
		`<a href="subject/entity/acme-corp">Acme Corp</a>`,
		`<a href="subject/concept/widget">Widget</a>`,
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

	NewHandler("wiki", "v-test", "/srv/wiki/", WithAsker(asker)).ServeHTTP(rec, req)

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

	NewHandler("wiki", "v-test", "/srv/wiki/", WithAsker(asker), WithMentioner(&stubMentioner{})).ServeHTTP(rec, req)

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

	NewHandler("wiki", "v-test", "/srv/wiki/", WithAsker(asker), WithOrphanLister(lister)).ServeHTTP(rec, req)

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

func TestHomeHandlerRendersOrphansAsMountRelativeLinksInOrder(t *testing.T) {
	// R-ONZU-Z1EX
	lister := &stubOrphanLister{refs: []Ref{
		{Href: "subject/entity/acme-corp", Name: "Acme Corp"},
		{Href: "subject/event/tulsa-launch", Name: "Tulsa Launch"},
	}}
	req := httptest.NewRequest(http.MethodGet, "/", nil)
	rec := httptest.NewRecorder()

	NewHandler("wiki", "v-test", "/srv/wiki/", WithOrphanLister(lister)).ServeHTTP(rec, req)

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

	NewHandler("wiki", "v-test", "/srv/wiki/", WithOrphanLister(&stubOrphanLister{})).ServeHTTP(rec, req)

	body := rec.Body.String()
	if !strings.Contains(body, `<form action="" method="get" role="search">`) {
		t.Fatalf("body missing search form: %s", body)
	}
	if strings.Contains(body, "<nav") || strings.Contains(body, "<ul") {
		t.Fatalf("body contains orphan nav/list despite empty orphans: %s", body)
	}
}

func TestHomeHandlerUsesEmbeddedCarbonAssets(t *testing.T) {
	// R-LAND-CARB
	if _, err := fs.Stat(assets, "static/tokens.css"); err != nil {
		t.Fatalf("embedded tokens.css missing: %v", err)
	}
	fonts, err := fs.Glob(assets, "static/fonts/*.woff2")
	if err != nil {
		t.Fatalf("glob fonts: %v", err)
	}
	if len(fonts) == 0 {
		t.Fatal("embedded woff2 fonts missing")
	}

	pageReq := httptest.NewRequest(http.MethodGet, "/", nil)
	pageRec := httptest.NewRecorder()
	NewHandler("wiki", "v-test", "/srv/wiki/").ServeHTTP(pageRec, pageReq)
	if !strings.Contains(pageRec.Body.String(), `href="static/tokens.css"`) {
		t.Fatalf("home page does not reference tokens.css through base-relative href: %s", pageRec.Body.String())
	}

	mux := NewHandler("wiki", "v-test", "/srv/wiki/")
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
}

func TestHomeMuxDoesNotServeRootPageForOtherPaths(t *testing.T) {
	// R-LAND-ROOT
	mux := NewHandler("wiki", "v-test", "/srv/wiki/")

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

	NewHandler("wiki", "v-test", "/srv/wiki/").ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200 without identity headers; body=%s", rec.Code, rec.Body.String())
	}
	if req.Header.Get("X-Owner-Email") != "" || req.Header.Get("Authorization") != "" {
		t.Fatal("test request unexpectedly carried identity or bearer headers")
	}
}
