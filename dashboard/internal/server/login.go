package server

import (
	"net/http"
	"net/url"
	"strings"
)

// handleLogin starts the Google sign-in flow: it mints a one-time state record,
// binds it to the browser with a cookie, then redirects to Google's authorize
// URL carrying that state.
func (a *app) handleLogin() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		handshake, cookie, err := a.handshakes.CreateWeb(r.Context(), safeReturnTo(r.URL.Query().Get("return_to")))
		if err != nil {
			a.logger.Error("login.create_handshake", "err", err)
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}
		setBindingCookie(w, r, cookie)
		redirectURI := a.publicBaseURL + "/oauth/google/callback"
		http.Redirect(w, r, a.idpProvider.AuthorizeURL(handshake.ID, redirectURI), http.StatusFound)
	}
}

// safeReturnTo returns p if it is a same-site absolute path, else "".
func safeReturnTo(p string) string {
	if p == "" || !strings.HasPrefix(p, "/") || strings.HasPrefix(p, "//") || strings.HasPrefix(p, "/\\") {
		return ""
	}
	for i := 0; i < len(p); i++ {
		if p[i] == '\\' || p[i] < 0x20 || p[i] == 0x7f {
			return ""
		}
	}
	u, err := url.Parse(p)
	if err != nil || u.Scheme != "" || u.Host != "" {
		return ""
	}
	return p
}

// bindingCookieName is the cookie carrying a handshake's plaintext binding secret
// to the browser. The matching hash lives server-side on the handshake row; the
// callback re-hashes this cookie's value to prove the round-trip is the same
// browser that started it.
const bindingCookieName = "dashboard_oauth_state"

// setBindingCookie hands the browser the plaintext binding cookie for a freshly
// created handshake. Secure is gated on isHTTPS so the cookie works on plain-http
// localhost and is Secure on the deployed TLS host. It is a session cookie (no
// Max-Age): the server-side handshake TTL is the authoritative expiry.
func setBindingCookie(w http.ResponseWriter, r *http.Request, cookie string) {
	http.SetCookie(w, &http.Cookie{
		Name:     bindingCookieName,
		Value:    cookie,
		Path:     "/",
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
		Secure:   isHTTPS(r),
	})
}
