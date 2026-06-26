package web

import (
	"context"
	"io/fs"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

type stubOrphanLister struct {
	refs   []Ref
	called int
}

func (s *stubOrphanLister) Orphans(context.Context) ([]Ref, error) {
	s.called++
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
