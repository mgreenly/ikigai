package server

import (
	"net/http"
	"strings"
	"testing"

	"dashboard/internal/googleidp"
)

// TestIndexLoggedIn: a request carrying a live session cookie renders the
// identity-aware page — the owner's email and a POST /logout control, and not
// the logged-out sign-in link.
func TestIndexLoggedIn(t *testing.T) {
	srv := testServer(t)
	sess := liveSession(t, srv)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/", map[string]string{"Cookie": sess.Name + "=" + sess.Value})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	if !strings.Contains(body, googleidp.StubIdentity.Email) {
		t.Errorf("logged-in index missing owner email %q:\n%s", googleidp.StubIdentity.Email, body)
	}
	if !strings.Contains(body, `action="/logout"`) {
		t.Errorf("logged-in index missing logout control:\n%s", body)
	}
	if strings.Contains(body, `href="/login"`) {
		t.Errorf("logged-in index still shows sign-in link:\n%s", body)
	}
}

// TestIndexLoggedOut: no cookie renders the logged-out landing — the sign-in
// link, no owner.
func TestIndexLoggedOut(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	if !strings.Contains(body, `href="/login"`) {
		t.Errorf("logged-out index missing sign-in link:\n%s", body)
	}
	if strings.Contains(body, `action="/logout"`) {
		t.Errorf("logged-out index shows a logout control:\n%s", body)
	}
}

func TestIndexLoggedOutShowsNameOriginColophon(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	wall := signinWallMain(t, body)
	aside := nameOriginAside(t, body)
	signInLink := `<a href="/login" class="btn btn-primary btn-lg btn-accent-link">Sign in with Google</a>`
	if strings.Index(wall, `<aside class="name-origin"`) <= strings.Index(wall, signInLink) {
		t.Errorf("name-origin colophon is not after the sign-in CTA:\n%s", wall)
	}
	if !strings.Contains(wall, "</aside>\n  </main>") {
		t.Errorf("name-origin colophon is not last inside the sign-in wall:\n%s", wall)
	}
	for _, want := range []string{
		`<aside class="name-origin" aria-label="What ikigenba means">`,
		`<p class="name-origin-lede"><b>ikigenba</b> — A portmanteau of two Japanese words:</p>`,
		`<dl class="name-origin-parts">`,
		`<dt><b class="seam">iki</b>gai <span lang="ja">生き甲斐</span></dt>`,
		`<dd>&ldquo;reason for being&rdquo;; work worth doing.</dd>`,
		`<dt><b class="seam">genba</b> <span lang="ja">現場</span></dt>`,
		`<dd>the actual place; where the work happens.</dd>`,
	} {
		// R-DB17-ORIG
		if !strings.Contains(body, want) {
			t.Errorf("logged-out index missing name-origin content %q:\n%s", want, body)
		}
	}
	if got := strings.Count(aside, `<p class="name-origin-lede">`); got != 1 {
		t.Errorf("name-origin lede count = %d, want 1:\n%s", got, aside)
	}
	if got := strings.Count(aside, "<p"); got != 2 {
		t.Errorf("name-origin paragraph count = %d, want lede and pronunciation foot:\n%s", got, aside)
	}
	if got := strings.Count(aside, `<dl class="name-origin-parts">`); got != 1 {
		t.Errorf("name-origin parts list count = %d, want 1:\n%s", got, aside)
	}
	if got := strings.Count(aside, "<dt>"); got != 2 {
		t.Errorf("name-origin dt count = %d, want 2:\n%s", got, aside)
	}
	if got := strings.Count(aside, "<dd>"); got != 2 {
		t.Errorf("name-origin dd count = %d, want 2:\n%s", got, aside)
	}
	if got := strings.Count(aside, `<b class="seam">`); got != 2 {
		t.Errorf("name-origin seam count = %d, want 2:\n%s", got, aside)
	}
	if got := strings.Count(aside, `span lang="ja"`); got != 2 {
		t.Errorf("name-origin Japanese span count = %d, want 2:\n%s", got, aside)
	}
	if strings.Contains(aside, "name-origin-roots") {
		t.Errorf("name-origin uses stale roots class instead of parts:\n%s", aside)
	}
}

func TestIndexLoggedOutShowsNameOriginPronunciationFoot(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	aside := nameOriginAside(t, body)
	parts := `<dl class="name-origin-parts">`
	say := `<p class="name-origin-say">pronounced <b>EE-kee-GEN-buh</b></p>`

	// R-O7K1-XEN7
	if got := strings.Count(aside, `<p class="name-origin-say">`); got != 1 {
		t.Fatalf("name-origin pronunciation count = %d, want 1:\n%s", got, aside)
	}
	if !strings.Contains(aside, say) {
		t.Errorf("name-origin pronunciation foot missing phonetic string:\n%s", aside)
	}
	if got := strings.Count(aside, "<p"); got != 2 {
		t.Errorf("name-origin paragraph count = %d, want lede and pronunciation foot:\n%s", got, aside)
	}
	if strings.Index(aside, say) <= strings.Index(aside, parts) {
		t.Errorf("name-origin pronunciation foot is not after the parts list:\n%s", aside)
	}
	if !strings.HasSuffix(aside, say+"\n    </aside>") {
		t.Errorf("name-origin pronunciation foot is not last inside the aside:\n%s", aside)
	}

	sess := liveSession(t, srv)
	rec = do(t, srv, "GET", "https://int.ikigenba.com/", map[string]string{"Cookie": sess.Name + "=" + sess.Value})
	if rec.Code != http.StatusOK {
		t.Fatalf("signed-in status = %d, want 200", rec.Code)
	}
	if strings.Contains(rec.Body.String(), `name-origin-say`) || strings.Contains(rec.Body.String(), `EE-kee-GEN-buh`) {
		t.Errorf("signed-in index includes logged-out pronunciation guide:\n%s", rec.Body.String())
	}
}

func TestIndexLoggedOutKeepsSigninCopy(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	for _, want := range []string{
		`<p class="wordmark" style="font-family: var(--font-display); font-size: var(--text-h3-size); font-weight: var(--text-h3-weight); margin: 0;">ikigenba</p>`,
		`<h1>Your account's control plane</h1>`,
		`<p>Sign in to access your services.</p>`,
		`<a href="/login" class="btn btn-primary btn-lg btn-accent-link">Sign in with Google</a>`,
	} {
		// R-DB18-KEEP
		if !strings.Contains(body, want) {
			t.Errorf("logged-out index no longer keeps sign-in copy %q:\n%s", want, body)
		}
	}
}

func TestIndexLoggedInOmitsNameOriginColophon(t *testing.T) {
	srv := testServer(t)
	sess := liveSession(t, srv)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/", map[string]string{"Cookie": sess.Name + "=" + sess.Value})

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	// R-DB19-LAND
	if strings.Contains(body, `class="name-origin"`) {
		t.Errorf("logged-in index includes logged-out name-origin colophon:\n%s", body)
	}
	if strings.Contains(body, `<main class="signin-wall">`) {
		t.Errorf("logged-in index rendered the logged-out sign-in wall:\n%s", body)
	}
	for _, want := range []string{
		`<main class="page">`,
		`<h2>Connect your agent</h2>`,
		`/install/claude`,
		`/install/codex`,
	} {
		if !strings.Contains(body, want) {
			t.Errorf("logged-in index missing dashboard landing fragment %q:\n%s", want, body)
		}
	}
	if !strings.Contains(body, googleidp.StubIdentity.Email) {
		t.Errorf("logged-in index missing owner email %q:\n%s", googleidp.StubIdentity.Email, body)
	}
}

func nameOriginAside(t *testing.T, body string) string {
	t.Helper()

	start := strings.Index(body, `<aside class="name-origin"`)
	if start < 0 {
		t.Fatalf("body missing name-origin aside:\n%s", body)
	}
	end := strings.Index(body[start:], `</aside>`)
	if end < 0 {
		t.Fatalf("name-origin aside is not closed:\n%s", body[start:])
	}
	return body[start : start+end+len(`</aside>`)]
}

func signinWallMain(t *testing.T, body string) string {
	t.Helper()

	start := strings.Index(body, `<main class="signin-wall">`)
	if start < 0 {
		t.Fatalf("body missing sign-in wall:\n%s", body)
	}
	end := strings.Index(body[start:], `</main>`)
	if end < 0 {
		t.Fatalf("sign-in wall is not closed:\n%s", body[start:])
	}
	return body[start : start+end+len(`</main>`)]
}

func TestIndexLoggedOutKeepsLandingOnly(t *testing.T) {
	srv := testServer(t)

	rec := do(t, srv, "GET", "https://int.ikigenba.com/", nil)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	// R-DB02-LND7
	if !strings.Contains(body, `href="/login"`) {
		t.Errorf("logged-out landing missing sign-in link:\n%s", body)
	}
	if strings.Contains(body, googleidp.StubIdentity.Email) {
		t.Errorf("logged-out landing leaked owner email:\n%s", body)
	}
	if strings.Contains(body, `action="/pat"`) {
		t.Errorf("logged-out landing exposed PAT form:\n%s", body)
	}
	if strings.Contains(body, `id="grants-block"`) {
		t.Errorf("logged-out landing exposed grants block:\n%s", body)
	}
	if strings.Contains(body, `href="/profile"`) {
		t.Errorf("logged-out landing exposed profile control:\n%s", body)
	}
}

// TestIndexDeadCookie: a present-but-invalid cookie (here, unknown) renders
// logged-out and clears the cookie so the browser stops resending a value that
// can never redeem.
func TestIndexDeadCookie(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/",
		map[string]string{"Cookie": sessionCookieName + "=bogus-value"})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if !strings.Contains(rec.Body.String(), `href="/login"`) {
		t.Errorf("dead-cookie index not logged-out:\n%s", rec.Body.String())
	}

	var cleared *http.Cookie
	for _, c := range rec.Result().Cookies() {
		if c.Name == sessionCookieName {
			cleared = c
		}
	}
	if cleared == nil {
		t.Fatalf("dead cookie was not cleared (no %s Set-Cookie)", sessionCookieName)
	}
	if cleared.MaxAge >= 0 {
		t.Errorf("cleared cookie MaxAge = %d, want negative (delete)", cleared.MaxAge)
	}
}
