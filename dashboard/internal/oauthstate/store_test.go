package oauthstate

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"path/filepath"
	"testing"
	"time"

	"dashboard/internal/db"
)

func testStore(t *testing.T, ttl time.Duration) *HandshakeStore {
	t.Helper()
	database, err := db.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { database.Close() })
	return NewHandshakeStore(database, ttl)
}

// TestCreateStoresHashNotCookie is the core security property: the value returned
// to the caller (and destined for the browser) is the plaintext cookie, while the
// value persisted is its SHA-256 hash — never the plaintext.
func TestCreateStoresHashNotCookie(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	handshake, cookie, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if handshake.ID == "" || cookie == "" {
		t.Fatal("Create returned empty id or cookie")
	}

	sum := sha256.Sum256([]byte(cookie))
	wantHash := hex.EncodeToString(sum[:])
	if handshake.BindingCookieHash != wantHash {
		t.Errorf("hash = %q, want sha256(cookie) %q", handshake.BindingCookieHash, wantHash)
	}
	if handshake.BindingCookieHash == cookie {
		t.Error("plaintext cookie equals stored hash — they must differ")
	}

	// The persisted row matches the returned handshake (and holds the hash).
	var id, hash string
	if err := store.db.QueryRow(
		`SELECT id, binding_cookie_hash FROM oauth_state WHERE id = ?`, handshake.ID,
	).Scan(&id, &hash); err != nil {
		t.Fatalf("read row: %v", err)
	}
	if id != handshake.ID || hash != handshake.BindingCookieHash {
		t.Errorf("row (%q,%q) != returned (%q,%q)", id, hash, handshake.ID, handshake.BindingCookieHash)
	}
}

// TestCreateSetsTTL checks the expiry window equals the store's configured TTL.
func TestCreateSetsTTL(t *testing.T) {
	ttl := 3 * time.Minute
	store := testStore(t, ttl)
	handshake, _, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if got := handshake.ExpiresAt.Sub(handshake.CreatedAt); got != ttl {
		t.Errorf("expiry window = %v, want %v", got, ttl)
	}
}

// TestCreateMintsUniqueValues guards against a botched RNG handing out repeats.
func TestCreateMintsUniqueValues(t *testing.T) {
	store := testStore(t, time.Minute)
	a, cookieA, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create a: %v", err)
	}
	b, cookieB, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create b: %v", err)
	}
	if a.ID == b.ID {
		t.Error("two handshakes share an id")
	}
	if cookieA == cookieB {
		t.Error("two handshakes share a cookie")
	}
}

// TestConsumeReturnsHandshake is the happy path: a freshly created handshake,
// consumed with the right id and cookie, returns the stored row.
func TestConsumeReturnsHandshake(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	created, cookie, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}

	got, err := store.Consume(context.Background(), created.ID, cookie)
	if err != nil {
		t.Fatalf("Consume: %v", err)
	}
	if got.ID != created.ID {
		t.Errorf("Consume id = %q, want %q", got.ID, created.ID)
	}
	if got.BindingCookieHash != created.BindingCookieHash {
		t.Errorf("Consume hash = %q, want %q", got.BindingCookieHash, created.BindingCookieHash)
	}
}

// TestConsumeIsSingleUse proves delete-on-consume: the first consume succeeds,
// the second finds no row. This is the replay defense.
func TestConsumeIsSingleUse(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	created, cookie, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if _, err := store.Consume(context.Background(), created.ID, cookie); err != nil {
		t.Fatalf("first Consume: %v", err)
	}
	_, err = store.Consume(context.Background(), created.ID, cookie)
	if !errors.Is(err, ErrHandshakeNotFound) {
		t.Errorf("second Consume err = %v, want ErrHandshakeNotFound", err)
	}
}

// TestConsumeUnknownID treats an unrecognized state value the same as a replay:
// no row, ErrHandshakeNotFound.
func TestConsumeUnknownID(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	_, err := store.Consume(context.Background(), "nonexistent", "whatever")
	if !errors.Is(err, ErrHandshakeNotFound) {
		t.Errorf("err = %v, want ErrHandshakeNotFound", err)
	}
}

// TestConsumeExpired rejects a handshake past its TTL. The negative TTL makes the
// row expired the instant it is created, so the check is deterministic.
func TestConsumeExpired(t *testing.T) {
	store := testStore(t, -time.Minute)
	created, cookie, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	_, err = store.Consume(context.Background(), created.ID, cookie)
	if !errors.Is(err, ErrHandshakeExpired) {
		t.Errorf("err = %v, want ErrHandshakeExpired", err)
	}
}

// TestConsumeBindingMismatch rejects a wrong cookie and, crucially, does not burn
// the handshake: the hash check runs before the delete, so the legitimate browser
// can still complete the flow afterward with the right cookie.
func TestConsumeBindingMismatch(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	created, cookie, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}

	_, err = store.Consume(context.Background(), created.ID, cookie+"x")
	if !errors.Is(err, ErrBindingMismatch) {
		t.Fatalf("err = %v, want ErrBindingMismatch", err)
	}
	if _, err := store.Consume(context.Background(), created.ID, cookie); err != nil {
		t.Errorf("Consume after mismatch: %v, want success", err)
	}
}

// sampleMCPContext is a representative MCP /oauth/authorize context for tests.
func sampleMCPContext() MCPContext {
	return MCPContext{
		ClientID:            "client-abc",
		RedirectURI:         "https://client.example/callback",
		CodeChallenge:       "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM",
		CodeChallengeMethod: "S256",
		ClientState:         "client-state-xyz",
		Resource:            "https://int.ikigenba.com/mcp/crm",
	}
}

// assertMCPFields checks a handshake carries exactly the given MCP context.
func assertMCPFields(t *testing.T, got Handshake, want MCPContext) {
	t.Helper()
	if got.MCPClientID != want.ClientID {
		t.Errorf("MCPClientID = %q, want %q", got.MCPClientID, want.ClientID)
	}
	if got.MCPRedirectURI != want.RedirectURI {
		t.Errorf("MCPRedirectURI = %q, want %q", got.MCPRedirectURI, want.RedirectURI)
	}
	if got.MCPCodeChallenge != want.CodeChallenge {
		t.Errorf("MCPCodeChallenge = %q, want %q", got.MCPCodeChallenge, want.CodeChallenge)
	}
	if got.MCPCodeChallengeMethod != want.CodeChallengeMethod {
		t.Errorf("MCPCodeChallengeMethod = %q, want %q", got.MCPCodeChallengeMethod, want.CodeChallengeMethod)
	}
	if got.MCPClientState != want.ClientState {
		t.Errorf("MCPClientState = %q, want %q", got.MCPClientState, want.ClientState)
	}
	if got.MCPResource != want.Resource {
		t.Errorf("MCPResource = %q, want %q", got.MCPResource, want.Resource)
	}
}

// TestCreateWebOrigin guards the regression: a web Create → Consume round-trip
// reports OriginWeb and carries no MCP context.
func TestCreateWebOrigin(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	created, cookie, err := store.Create(context.Background())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if created.Origin != OriginWeb {
		t.Errorf("Create origin = %q, want %q", created.Origin, OriginWeb)
	}

	got, err := store.Consume(context.Background(), created.ID, cookie)
	if err != nil {
		t.Fatalf("Consume: %v", err)
	}
	if got.Origin != OriginWeb {
		t.Errorf("Consume origin = %q, want %q", got.Origin, OriginWeb)
	}
	if got.MCPClientID != "" || got.MCPRedirectURI != "" || got.MCPCodeChallenge != "" ||
		got.MCPCodeChallengeMethod != "" || got.MCPClientState != "" || got.MCPResource != "" {
		t.Errorf("web handshake carried MCP fields: %+v", got)
	}
}

// TestCreateMCPPersistsContext verifies CreateMCP stamps origin='mcp' and the
// full MCP context, both on the returned handshake and in the persisted row.
func TestCreateMCPPersistsContext(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	mcp := sampleMCPContext()

	created, cookie, err := store.CreateMCP(context.Background(), mcp)
	if err != nil {
		t.Fatalf("CreateMCP: %v", err)
	}
	if created.ID == "" || cookie == "" {
		t.Fatal("CreateMCP returned empty id or cookie")
	}
	if created.Origin != OriginMCP {
		t.Errorf("CreateMCP origin = %q, want %q", created.Origin, OriginMCP)
	}
	assertMCPFields(t, created, mcp)

	var origin, clientID, resource string
	if err := store.db.QueryRow(
		`SELECT origin, mcp_client_id, mcp_resource FROM oauth_state WHERE id = ?`, created.ID,
	).Scan(&origin, &clientID, &resource); err != nil {
		t.Fatalf("read row: %v", err)
	}
	if origin != OriginMCP || clientID != mcp.ClientID || resource != mcp.Resource {
		t.Errorf("row (%q,%q,%q) != (%q,%q,%q)",
			origin, clientID, resource, OriginMCP, mcp.ClientID, mcp.Resource)
	}
}

// TestConsumeMCPReturnsContext is the MCP happy path: a freshly created MCP
// handshake, consumed with the right id and cookie, returns OriginMCP and the
// exact MCP context.
func TestConsumeMCPReturnsContext(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	mcp := sampleMCPContext()

	created, cookie, err := store.CreateMCP(context.Background(), mcp)
	if err != nil {
		t.Fatalf("CreateMCP: %v", err)
	}

	got, err := store.Consume(context.Background(), created.ID, cookie)
	if err != nil {
		t.Fatalf("Consume: %v", err)
	}
	if got.ID != created.ID {
		t.Errorf("Consume id = %q, want %q", got.ID, created.ID)
	}
	if got.Origin != OriginMCP {
		t.Errorf("Consume origin = %q, want %q", got.Origin, OriginMCP)
	}
	assertMCPFields(t, got, mcp)
}

// TestConsumeMCPSingleUse proves delete-on-consume applies to MCP handshakes too.
func TestConsumeMCPSingleUse(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	created, cookie, err := store.CreateMCP(context.Background(), sampleMCPContext())
	if err != nil {
		t.Fatalf("CreateMCP: %v", err)
	}
	if _, err := store.Consume(context.Background(), created.ID, cookie); err != nil {
		t.Fatalf("first Consume: %v", err)
	}
	_, err = store.Consume(context.Background(), created.ID, cookie)
	if !errors.Is(err, ErrHandshakeNotFound) {
		t.Errorf("second Consume err = %v, want ErrHandshakeNotFound", err)
	}
}

// TestConsumeMCPExpired rejects an MCP handshake past its TTL.
func TestConsumeMCPExpired(t *testing.T) {
	store := testStore(t, -time.Minute)
	created, cookie, err := store.CreateMCP(context.Background(), sampleMCPContext())
	if err != nil {
		t.Fatalf("CreateMCP: %v", err)
	}
	_, err = store.Consume(context.Background(), created.ID, cookie)
	if !errors.Is(err, ErrHandshakeExpired) {
		t.Errorf("err = %v, want ErrHandshakeExpired", err)
	}
}

// TestConsumeMCPBindingMismatch rejects a wrong cookie for an MCP handshake and,
// crucially, does not burn it.
func TestConsumeMCPBindingMismatch(t *testing.T) {
	store := testStore(t, 5*time.Minute)
	created, cookie, err := store.CreateMCP(context.Background(), sampleMCPContext())
	if err != nil {
		t.Fatalf("CreateMCP: %v", err)
	}

	_, err = store.Consume(context.Background(), created.ID, cookie+"x")
	if !errors.Is(err, ErrBindingMismatch) {
		t.Fatalf("err = %v, want ErrBindingMismatch", err)
	}
	if _, err := store.Consume(context.Background(), created.ID, cookie); err != nil {
		t.Errorf("Consume after mismatch: %v, want success", err)
	}
}
