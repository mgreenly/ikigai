package server

import (
	"context"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

// streamResult carries a backgrounded SSE request's recorder and a done channel
// closed when the handler returns (after the request context is cancelled).
type streamResult struct {
	rec  *httptest.ResponseRecorder
	done chan struct{}
}

// doCtx runs a request on a background goroutine with the given (cancellable)
// context, so a long-lived handler like the SSE stream can be driven and then
// torn down by cancelling ctx. The caller must wait on .done before reading
// .rec.
func doCtx(t *testing.T, srv *http.Server, ctx context.Context, method, target string, hdr map[string]string) *streamResult {
	t.Helper()
	req := httptest.NewRequest(method, target, nil).WithContext(ctx)
	for k, v := range hdr {
		req.Header.Set(k, v)
	}
	rec := httptest.NewRecorder()
	sr := &streamResult{rec: rec, done: make(chan struct{})}
	go func() {
		srv.Handler.ServeHTTP(rec, req)
		close(sr.done)
	}()
	return sr
}

// grantsTestServer builds a real handler over one migrated SQLite db and
// returns it alongside the deps so the test can mint a session and issue a
// chain directly against the same store the handler reads.
func grantsTestServer(t *testing.T) (*http.Server, serverDeps) {
	t.Helper()
	deps := newServerDeps(t)
	srv, err := New(deps.opts())
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return srv, deps
}

// mintSession creates a live web session for owner and returns its cookie.
func mintSession(t *testing.T, deps serverDeps, owner string) *http.Cookie {
	t.Helper()
	issued, err := deps.sessions.Create(context.Background(), owner)
	if err != nil {
		t.Fatalf("sessions.Create: %v", err)
	}
	return &http.Cookie{Name: sessionCookieName, Value: issued.CookieValue}
}

// issueChain registers a client and issues one token chain for owner, returning
// the chain's public_id and the client name (so tests can assert on rendering).
func issueChain(t *testing.T, deps serverDeps, owner, clientName string) (publicID, gotClientName string) {
	t.Helper()
	ctx := context.Background()
	client, err := deps.clients.Register(ctx, clientName, []string{"https://app.example/cb"})
	if err != nil {
		t.Fatalf("clients.Register: %v", err)
	}
	tx, err := deps.db.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	pair, err := deps.tokens.IssueChainAndTokens(ctx, tx, client.ClientID, owner, testResource)
	if err != nil {
		tx.Rollback()
		t.Fatalf("IssueChainAndTokens: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}
	return pair.ChainPublicID, client.ClientName
}

// TestGrantsFragmentListsOwnGrant: a signed-in user's fragment lists their
// grant (the client name appears).
func TestGrantsFragmentListsOwnGrant(t *testing.T) {
	srv, deps := grantsTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	_, clientName := issueChain(t, deps, owner, "Claude Code")

	rec := do(t, srv, "GET", "https://int.ikigenba.com/grants/fragment",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if ct := rec.Header().Get("Content-Type"); !strings.HasPrefix(ct, "text/html") {
		t.Errorf("Content-Type = %q, want text/html", ct)
	}
	if rec.Header().Get("Cache-Control") != "no-store" {
		t.Errorf("Cache-Control = %q, want no-store", rec.Header().Get("Cache-Control"))
	}
	body := rec.Body.String()
	if !strings.Contains(body, clientName) {
		t.Errorf("fragment missing client name %q:\n%s", clientName, body)
	}
	if !strings.Contains(body, "/revoke") {
		t.Errorf("fragment missing a revoke form:\n%s", body)
	}
}

// TestGrantsFragmentRequiresSession: no cookie → 401, matching the authed
// route contract.
func TestGrantsFragmentRequiresSession(t *testing.T) {
	srv, _ := grantsTestServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/grants/fragment", nil)
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
}

// TestGrantRevoke: a same-origin POST revokes the grant; a follow-up fragment
// no longer lists it.
func TestGrantRevoke(t *testing.T) {
	srv, deps := grantsTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID, clientName := issueChain(t, deps, owner, "Claude Code")

	rec := do(t, srv, "POST", "https://int.ikigenba.com/grants/"+publicID+"/revoke",
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("revoke status = %d, want 303", rec.Code)
	}
	if loc := rec.Header().Get("Location"); loc != "/profile" {
		t.Errorf("Location = %q, want /profile", loc)
	}

	frag := do(t, srv, "GET", "https://int.ikigenba.com/grants/fragment",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})
	if strings.Contains(frag.Body.String(), clientName) {
		t.Errorf("revoked grant still listed:\n%s", frag.Body.String())
	}
}

// TestGrantRevokeCrossOrigin: a POST without a matching Origin/Referer is
// rejected as cross-origin (403) and the grant survives.
func TestGrantRevokeCrossOrigin(t *testing.T) {
	srv, deps := grantsTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)
	publicID, clientName := issueChain(t, deps, owner, "Claude Code")

	rec := do(t, srv, "POST", "https://int.ikigenba.com/grants/"+publicID+"/revoke",
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://evil.example",
		})
	if rec.Code != http.StatusForbidden {
		t.Fatalf("cross-origin revoke status = %d, want 403", rec.Code)
	}
	frag := do(t, srv, "GET", "https://int.ikigenba.com/grants/fragment",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})
	if !strings.Contains(frag.Body.String(), clientName) {
		t.Errorf("grant was wrongly revoked on a cross-origin request")
	}
}

// TestGrantRevokeOtherOwner: revoking a chain owned by someone else is
// indistinguishable from not-found (404) and does not revoke it.
func TestGrantRevokeOtherOwner(t *testing.T) {
	srv, deps := grantsTestServer(t)
	cookie := mintSession(t, deps, "attacker@int.ikigenba.com")
	publicID, clientName := issueChain(t, deps, "victim@int.ikigenba.com", "Victim Client")

	rec := do(t, srv, "POST", "https://int.ikigenba.com/grants/"+publicID+"/revoke",
		map[string]string{
			"Cookie": cookie.Name + "=" + cookie.Value,
			"Origin": "https://int.ikigenba.com",
		})
	if rec.Code != http.StatusNotFound {
		t.Fatalf("foreign-chain revoke status = %d, want 404", rec.Code)
	}
	// Victim's grant still present.
	victimCookie := mintSession(t, deps, "victim@int.ikigenba.com")
	frag := do(t, srv, "GET", "https://int.ikigenba.com/grants/fragment",
		map[string]string{"Cookie": victimCookie.Name + "=" + victimCookie.Value})
	if !strings.Contains(frag.Body.String(), clientName) {
		t.Errorf("victim's grant was revoked by another owner")
	}
}

// TestGrantsStreamEmitsChainsEvent: the SSE stream sets event-stream headers,
// emits the initial "chains" event, and emits another after a Publish. The
// request context is cancelled to end the stream.
func TestGrantsStreamEmitsChainsEvent(t *testing.T) {
	srv, deps := grantsTestServer(t)
	const owner = "owner@int.ikigenba.com"
	cookie := mintSession(t, deps, owner)

	ctx, cancel := context.WithCancel(context.Background())
	rec := doCtx(t, srv, ctx, "GET", "https://int.ikigenba.com/grants/stream",
		map[string]string{"Cookie": cookie.Name + "=" + cookie.Value})

	// Give the handler time to write the initial event and subscribe, then
	// publish a change, then end the stream.
	time.Sleep(50 * time.Millisecond)
	deps.grants.Publish(owner)
	time.Sleep(50 * time.Millisecond)
	cancel()
	<-rec.done

	if ct := rec.rec.Header().Get("Content-Type"); ct != "text/event-stream" {
		t.Errorf("Content-Type = %q, want text/event-stream", ct)
	}
	body := rec.rec.Body.String()
	// Initial event + the published one => at least two "chains" events.
	if got := strings.Count(body, "event: chains"); got < 2 {
		t.Errorf("got %d chains events, want >= 2:\n%s", got, body)
	}
}

// TestGrantsStreamRequiresSession: no cookie → 401.
func TestGrantsStreamRequiresSession(t *testing.T) {
	srv, _ := grantsTestServer(t)
	rec := do(t, srv, "GET", "https://int.ikigenba.com/grants/stream", nil)
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want 401", rec.Code)
	}
}
