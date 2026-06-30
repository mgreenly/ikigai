package server

import (
	"net/http"
	"regexp"
	"strings"
	"testing"
)

var fontFaceRule = regexp.MustCompile(`(?m)^@font-face\s*\{`)

func TestTokensCSSUsesOptionalFontDisplay(t *testing.T) {
	srv := testServer(t)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/static/tokens.css", nil)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	faceCount := len(fontFaceRule.FindAllString(body, -1))
	// R-P97M-GIJ1
	if got := strings.Count(body, "font-display: optional;"); got != faceCount {
		t.Errorf("font-display optional count = %d, want %d @font-face blocks:\n%s", got, faceCount, body)
	}
	if strings.Contains(body, "font-display: swap") {
		t.Errorf("tokens CSS still contains font-display swap:\n%s", body)
	}
}

func TestRenderedPagesPreloadSelfHostedFonts(t *testing.T) {
	srv := testServer(t)
	sess := liveSession(t, srv)
	cookie := map[string]string{"Cookie": sess.Name + "=" + sess.Value}

	cases := []struct {
		name    string
		target  string
		headers map[string]string
	}{
		{name: "logged-out index", target: "https://int.ikigenba.com/", headers: nil},
		{name: "logged-in index", target: "https://int.ikigenba.com/", headers: cookie},
		{name: "signed-in profile", target: "https://int.ikigenba.com/profile", headers: cookie},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			rec := do(t, srv, "GET", tc.target, tc.headers)
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want 200", rec.Code)
			}
			head := htmlHead(t, rec.Body.String())
			// R-PAFI-UA9Q
			assertFontPreload(t, head, "/static/fonts/space-grotesk.woff2")
			assertFontPreload(t, head, "/static/fonts/ibm-plex-sans.woff2")
		})
	}
}

func TestStaticRouteServesSelfHostedFonts(t *testing.T) {
	srv := testServer(t)

	for _, path := range []string{
		"/static/fonts/space-grotesk.woff2",
		"/static/fonts/ibm-plex-sans.woff2",
	} {
		t.Run(path, func(t *testing.T) {
			rec := do(t, srv, "GET", "https://int.ikigenba.com"+path, nil)

			// R-PBNF-820F
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want 200", rec.Code)
			}
			ct := rec.Header().Get("Content-Type")
			if !strings.HasPrefix(ct, "font/woff2") && !strings.HasPrefix(ct, "application/octet-stream") {
				t.Errorf("Content-Type = %q, want font/woff2 or application/octet-stream", ct)
			}
			if rec.Body.Len() == 0 {
				t.Fatal("font response body is empty")
			}
		})
	}
}

func assertFontPreload(t *testing.T, head, href string) {
	t.Helper()

	link := `<link rel="preload" as="font" type="font/woff2" crossorigin href="` + href + `">`
	if !strings.Contains(head, link) {
		t.Errorf("head missing font preload %q:\n%s", link, head)
	}
}

func htmlHead(t *testing.T, body string) string {
	t.Helper()

	start := strings.Index(body, "<head>")
	if start < 0 {
		t.Fatalf("body missing <head>:\n%s", body)
	}
	end := strings.Index(body[start:], "</head>")
	if end < 0 {
		t.Fatalf("body missing </head>:\n%s", body)
	}
	return body[start : start+end+len("</head>")]
}
