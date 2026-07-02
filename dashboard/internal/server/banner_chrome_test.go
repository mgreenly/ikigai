package server

import (
	"net/http"
	"strings"
	"testing"

	"dashboard/internal/googleidp"
)

func TestSignedInBannersUseWordmarkHomeLink(t *testing.T) {
	srv := landingServerWithCRM(t)
	cookie := liveSession(t, srv)
	headers := map[string]string{"Cookie": cookie.Name + "=" + cookie.Value}

	tests := []struct {
		name   string
		target string
	}{
		{name: "landing", target: "https://int.ikigenba.com/"},
		{name: "profile", target: "https://int.ikigenba.com/profile"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			rec := do(t, srv, "GET", tt.target, headers)
			if rec.Code != http.StatusOK {
				t.Fatalf("status = %d, want 200", rec.Code)
			}
			banner := bannerChrome(t, rec.Body.String())

			// R-VTIE-IUFA
			if !strings.Contains(banner, `<a href="/" class="wordmark">ikigenba</a>`) {
				t.Errorf("%s banner wordmark is not a home link:\n%s", tt.name, banner)
			}
			if strings.Contains(banner, `<p class="wordmark">ikigenba</p>`) {
				t.Errorf("%s banner still renders wordmark as inert paragraph:\n%s", tt.name, banner)
			}
		})
	}
}

func TestProfileBannerRendersSharedIdentityChrome(t *testing.T) {
	srv := testServer(t)
	cookie := liveSession(t, srv)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/profile", map[string]string{
		"Cookie": cookie.Name + "=" + cookie.Value,
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	banner := bannerChrome(t, rec.Body.String())
	email := googleidp.StubIdentity.Email
	avatar := `<a href="/profile" class="avatar" aria-label="Profile — ` + email + `" title="` + email + `">` + ownerInitial(email) + `</a>`
	signOut := `<form method="POST" action="/logout">`

	// R-VUQA-WM5Z
	if !strings.Contains(banner, avatar) {
		t.Errorf("profile banner missing monogram avatar %q:\n%s", avatar, banner)
	}
	if !strings.Contains(banner, signOut) {
		t.Errorf("profile banner missing sign-out control:\n%s", banner)
	}
	signOutAt := strings.Index(banner, signOut)
	avatarAt := strings.Index(banner, avatar)
	if signOutAt < 0 || avatarAt < 0 || signOutAt >= avatarAt {
		t.Errorf("profile banner should order sign-out before avatar; signOutAt=%d avatarAt=%d:\n%s", signOutAt, avatarAt, banner)
	}
	if strings.Contains(banner, `<a href="/" class="btn`) {
		t.Errorf("profile banner still renders bordered Home button:\n%s", banner)
	}
	if strings.Contains(banner, `<span class="owner">`) {
		t.Errorf("profile banner still renders owner email span:\n%s", banner)
	}
}

func TestAppCSSDefinesAvatarHoverAffordance(t *testing.T) {
	srv := testServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/static/app.css", nil)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	body := rec.Body.String()
	rule := cssRule(t, body, `.identity .avatar:hover`)

	// R-VVY7-ADWO
	if !strings.Contains(rule, `background: var(--accent-700);`) {
		t.Errorf("avatar hover rule does not darken to accent-700:\n%s", rule)
	}
	if !strings.Contains(body, `transition: background var(--duration-fast) var(--ease-standard);`) {
		t.Errorf("avatar rule missing background transition:\n%s", body)
	}
}

func bannerChrome(t *testing.T, body string) string {
	t.Helper()

	start := strings.Index(body, `<header class="banner">`)
	if start < 0 {
		t.Fatalf("body missing banner header:\n%s", body)
	}
	end := strings.Index(body[start:], `</header>`)
	if end < 0 {
		t.Fatalf("banner header is not closed:\n%s", body[start:])
	}
	return body[start : start+end+len(`</header>`)]
}

func cssRule(t *testing.T, css, selector string) string {
	t.Helper()

	start := strings.Index(css, selector+" {")
	if start < 0 {
		t.Fatalf("CSS missing %s rule:\n%s", selector, css)
	}
	end := strings.Index(css[start:], `}`)
	if end < 0 {
		t.Fatalf("CSS rule %s is not closed:\n%s", selector, css[start:])
	}
	return css[start : start+end+len(`}`)]
}
