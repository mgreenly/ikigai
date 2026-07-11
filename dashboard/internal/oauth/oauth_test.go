package oauth

import (
	"context"
	"database/sql"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"dashboard/internal/db"
)

// openTestDB opens a fresh migrated SQLite database in a temp dir.
func openTestDB(t *testing.T) *sql.DB {
	t.Helper()
	d, err := db.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { d.Close() })
	return d
}

// clock is a controllable time source: dereference *current for "now".
type clock struct {
	current time.Time
}

func (c *clock) now() time.Time { return c.current }

func newClock() *clock {
	return &clock{current: time.Date(2026, 5, 31, 12, 0, 0, 0, time.UTC)}
}

// ---- ClientStore ----

func TestClientStoreRegisterGetRoundTrip(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	cs := NewClientStore(d)
	cs.Now = clk.now

	uris := []string{"https://example.com/cb", "https://example.com/cb2"}
	c, err := cs.Register(ctx, "Example App", uris)
	if err != nil {
		t.Fatalf("Register: %v", err)
	}
	if c.ClientID == "" {
		t.Fatal("Register returned empty client_id")
	}

	got, err := cs.Get(ctx, c.ClientID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if got.ClientName != "Example App" {
		t.Errorf("client_name = %q, want %q", got.ClientName, "Example App")
	}
	if len(got.RedirectURIs) != 2 || got.RedirectURIs[0] != uris[0] || got.RedirectURIs[1] != uris[1] {
		t.Errorf("redirect_uris = %v, want %v", got.RedirectURIs, uris)
	}
	if !got.RegisteredAt.Equal(clk.current) {
		t.Errorf("registered_at = %v, want %v", got.RegisteredAt, clk.current)
	}
	if got.LastUsedAt != nil {
		t.Errorf("last_used_at = %v, want nil", got.LastUsedAt)
	}
}

func TestClientStoreRegisterEmptyURIs(t *testing.T) {
	d := openTestDB(t)
	cs := NewClientStore(d)
	if _, err := cs.Register(context.Background(), "x", nil); err == nil {
		t.Fatal("expected error for empty redirect_uris")
	}
}

func TestClientStoreGetUnknown(t *testing.T) {
	d := openTestDB(t)
	cs := NewClientStore(d)
	if _, err := cs.Get(context.Background(), "nope"); err != ErrNotFound {
		t.Fatalf("Get unknown = %v, want ErrNotFound", err)
	}
}

func TestClientStoreTouchLastUsed(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	cs := NewClientStore(d)
	cs.Now = clk.now

	c, err := cs.Register(ctx, "App", []string{"https://e/cb"})
	if err != nil {
		t.Fatalf("Register: %v", err)
	}
	touchTime := clk.current.Add(time.Hour)
	clk.current = touchTime
	if err := cs.TouchLastUsed(ctx, c.ClientID); err != nil {
		t.Fatalf("TouchLastUsed: %v", err)
	}
	got, err := cs.Get(ctx, c.ClientID)
	if err != nil {
		t.Fatalf("Get: %v", err)
	}
	if got.LastUsedAt == nil {
		t.Fatal("last_used_at is nil after TouchLastUsed")
	}
	if !got.LastUsedAt.Equal(touchTime) {
		t.Errorf("last_used_at = %v, want %v", got.LastUsedAt, touchTime)
	}
}

func TestClientStorePurgeUnused(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	cs := NewClientStore(d)
	cs.Now = clk.now

	// Old, unused -> purged.
	clk.current = time.Date(2026, 1, 1, 0, 0, 0, 0, time.UTC)
	oldUnused, _ := cs.Register(ctx, "old-unused", []string{"https://e/1"})

	// Old, used -> kept (last_used_at not null).
	oldUsed, _ := cs.Register(ctx, "old-used", []string{"https://e/2"})
	if err := cs.TouchLastUsed(ctx, oldUsed.ClientID); err != nil {
		t.Fatalf("TouchLastUsed: %v", err)
	}

	// New, unused -> kept (registered_at >= cutoff).
	clk.current = time.Date(2026, 5, 30, 0, 0, 0, 0, time.UTC)
	newUnused, _ := cs.Register(ctx, "new-unused", []string{"https://e/3"})

	cutoff := time.Date(2026, 3, 1, 0, 0, 0, 0, time.UTC)
	n, err := cs.PurgeUnused(ctx, cutoff)
	if err != nil {
		t.Fatalf("PurgeUnused: %v", err)
	}
	if n != 1 {
		t.Errorf("PurgeUnused deleted %d rows, want 1", n)
	}
	if _, err := cs.Get(ctx, oldUnused.ClientID); err != ErrNotFound {
		t.Errorf("old-unused still present: %v", err)
	}
	if _, err := cs.Get(ctx, oldUsed.ClientID); err != nil {
		t.Errorf("old-used wrongly purged: %v", err)
	}
	if _, err := cs.Get(ctx, newUnused.ClientID); err != nil {
		t.Errorf("new-unused wrongly purged: %v", err)
	}
}

// ---- AuthCodeStore ----

func TestAuthCodeStoreIssuePersistsHashOnly(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	acs := NewAuthCodeStore(d, 10*time.Minute)
	acs.Now = clk.now

	plaintext, rec, err := acs.Issue(ctx, IssueParams{
		ClientID:            "client-1",
		OwnerEmail:          "owner@example.com",
		CodeChallenge:       "chal",
		CodeChallengeMethod: "S256",
		RedirectURI:         "https://e/cb",
		Resource:            "https://e/mcp",
		OriginalState:       "st",
	})
	if err != nil {
		t.Fatalf("Issue: %v", err)
	}
	if !strings.HasPrefix(plaintext, "ms_aco_") {
		t.Errorf("plaintext = %q, want ms_aco_ prefix", plaintext)
	}
	if rec.ID == "" {
		t.Fatal("rec.ID empty")
	}
	if !rec.ExpiresAt.Equal(clk.current.Add(10 * time.Minute)) {
		t.Errorf("expires_at = %v, want %v", rec.ExpiresAt, clk.current.Add(10*time.Minute))
	}

	// Verify only the hash is persisted, never the plaintext.
	var stored string
	if err := d.QueryRowContext(ctx, `SELECT code_hash FROM oauth_authcodes WHERE id = ?`, rec.ID).Scan(&stored); err != nil {
		t.Fatalf("read code_hash: %v", err)
	}
	if stored == plaintext {
		t.Fatal("code_hash equals plaintext; plaintext leaked")
	}
	if stored != hashString(plaintext) {
		t.Errorf("code_hash = %q, want %q", stored, hashString(plaintext))
	}
}

func TestAuthCodeStoreLookupAndMarkUsed(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	acs := NewAuthCodeStore(d, time.Minute)
	acs.Now = clk.now

	plaintext, rec, err := acs.Issue(ctx, IssueParams{
		ClientID:            "client-1",
		OwnerEmail:          "owner@example.com",
		CodeChallenge:       "chal",
		CodeChallengeMethod: "S256",
		RedirectURI:         "https://e/cb",
		Resource:            "https://e/mcp",
		OriginalState:       "st",
	})
	if err != nil {
		t.Fatalf("Issue: %v", err)
	}

	tx, err := d.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	defer tx.Rollback()

	got, err := acs.LookupTx(ctx, tx, plaintext)
	if err != nil {
		t.Fatalf("LookupTx: %v", err)
	}
	if got.ID != rec.ID || got.ClientID != "client-1" || got.OwnerEmail != "owner@example.com" {
		t.Errorf("LookupTx mismatch: %+v", got)
	}
	if got.Resource != "https://e/mcp" || got.RedirectURI != "https://e/cb" {
		t.Errorf("LookupTx binding mismatch: %+v", got)
	}
	if got.UsedAt != nil || got.ChainID != nil {
		t.Errorf("fresh code already used: %+v", got)
	}

	if err := acs.MarkUsed(ctx, tx, rec.ID, "chain-xyz"); err != nil {
		t.Fatalf("MarkUsed: %v", err)
	}
	// Idempotent: second call is a no-op and does not error.
	if err := acs.MarkUsed(ctx, tx, rec.ID, "chain-other"); err != nil {
		t.Fatalf("MarkUsed idempotent: %v", err)
	}

	after, err := acs.LookupTx(ctx, tx, plaintext)
	if err != nil {
		t.Fatalf("LookupTx after MarkUsed: %v", err)
	}
	if after.UsedAt == nil {
		t.Error("used_at not set after MarkUsed")
	}
	if after.ChainID == nil || *after.ChainID != "chain-xyz" {
		t.Errorf("chain_id = %v, want chain-xyz", after.ChainID)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}
}

// R-VS5Y-UQU4
func TestAuthCodeOwnerIDSurvivesExchangeToChain(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	acs := NewAuthCodeStore(d, time.Minute)
	plaintext, code, err := acs.Issue(ctx, IssueParams{
		ClientID: "client-1", OwnerEmail: "owner@example.com", OwnerID: "identity-handle",
		CodeChallenge: "challenge", CodeChallengeMethod: "S256", RedirectURI: "https://e/cb",
		Resource: "https://e/mcp",
	})
	if err != nil {
		t.Fatalf("Issue: %v", err)
	}
	var storedOwnerID string
	if err := d.QueryRowContext(ctx, `SELECT owner_id FROM oauth_authcodes WHERE id = ?`, code.ID).Scan(&storedOwnerID); err != nil {
		t.Fatalf("read authcode owner_id: %v", err)
	}
	if storedOwnerID != "identity-handle" {
		t.Fatalf("authcode owner_id = %q, want identity-handle", storedOwnerID)
	}
	tx, err := d.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	defer tx.Rollback()
	lookedUp, err := acs.LookupTx(ctx, tx, plaintext)
	if err != nil {
		t.Fatalf("LookupTx: %v", err)
	}
	ts := NewTokenStore(d, time.Minute, time.Hour)
	pair, err := ts.IssueChainAndTokens(ctx, tx, lookedUp.ClientID, lookedUp.OwnerEmail, lookedUp.OwnerID, lookedUp.Resource)
	if err != nil {
		t.Fatalf("IssueChainAndTokens: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}
	if err := d.QueryRowContext(ctx, `SELECT owner_id FROM oauth_chains WHERE id = ?`, pair.ChainID).Scan(&storedOwnerID); err != nil {
		t.Fatalf("read chain owner_id: %v", err)
	}
	if storedOwnerID != "identity-handle" {
		t.Errorf("chain owner_id = %q, want identity-handle", storedOwnerID)
	}
}

func TestAuthCodeStoreLookupUnknown(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	acs := NewAuthCodeStore(d, time.Minute)
	tx, _ := d.BeginTx(ctx, nil)
	defer tx.Rollback()
	if _, err := acs.LookupTx(ctx, tx, "ms_aco_nope"); err != ErrNotFound {
		t.Fatalf("LookupTx unknown = %v, want ErrNotFound", err)
	}
}

// ---- TokenStore ----

func registerClient(t *testing.T, d *sql.DB, name string) string {
	t.Helper()
	cs := NewClientStore(d)
	c, err := cs.Register(context.Background(), name, []string{"https://e/cb"})
	if err != nil {
		t.Fatalf("register client: %v", err)
	}
	return c.ClientID
}

func TestTokenStoreIssueChainAndTokens(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")

	tx, err := d.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	pair, err := ts.IssueChainAndTokens(ctx, tx, clientID, "owner@example.com", "owner-1", "https://e/mcp")
	if err != nil {
		tx.Rollback()
		t.Fatalf("IssueChainAndTokens: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}

	if !strings.HasPrefix(pair.AccessToken, AccessPrefix) {
		t.Errorf("access token prefix = %q, want %q", pair.AccessToken, AccessPrefix)
	}
	if !strings.HasPrefix(pair.RefreshToken, RefreshPrefix) {
		t.Errorf("refresh token prefix = %q, want %q", pair.RefreshToken, RefreshPrefix)
	}
	if pair.ChainID == "" || pair.ChainPublicID == "" {
		t.Error("chain ids empty")
	}
	if !pair.AccessExpiresAt.Equal(clk.current.Add(15 * time.Minute)) {
		t.Errorf("access expiry = %v", pair.AccessExpiresAt)
	}
	if !pair.RefreshExpiresAt.Equal(clk.current.Add(24 * time.Hour)) {
		t.Errorf("refresh expiry = %v", pair.RefreshExpiresAt)
	}

	// Chain row exists.
	ch, err := ts.GetChain(ctx, pair.ChainID)
	if err != nil {
		t.Fatalf("GetChain: %v", err)
	}
	if ch.ClientID != clientID || ch.OwnerEmail != "owner@example.com" || ch.Resource != "https://e/mcp" {
		t.Errorf("chain mismatch: %+v", ch)
	}
}

func issueChain(t *testing.T, d *sql.DB, ts *TokenStore, clientID, owner, resource string) TokenPair {
	t.Helper()
	ctx := context.Background()
	tx, err := d.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	pair, err := ts.IssueChainAndTokens(ctx, tx, clientID, owner, "owner-test", resource)
	if err != nil {
		tx.Rollback()
		t.Fatalf("IssueChainAndTokens: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}
	return pair
}

func TestTokenStoreValidateAccessHappy(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")
	pair := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	vt, err := ts.ValidateAccess(ctx, pair.AccessToken)
	if err != nil {
		t.Fatalf("ValidateAccess: %v", err)
	}
	if vt.Token.Kind != KindAccess {
		t.Errorf("kind = %q, want access", vt.Token.Kind)
	}
	if vt.Chain.ID != pair.ChainID {
		t.Errorf("chain id = %q, want %q", vt.Chain.ID, pair.ChainID)
	}
	if vt.Chain.OwnerEmail != "owner@example.com" {
		t.Errorf("owner = %q", vt.Chain.OwnerEmail)
	}
}

func TestTokenStoreValidateAccessBadPrefix(t *testing.T) {
	d := openTestDB(t)
	ts := NewTokenStore(d, time.Minute, time.Hour)
	if _, err := ts.ValidateAccess(context.Background(), "garbage_token"); err != ErrBadPrefix {
		t.Fatalf("ValidateAccess bad prefix = %v, want ErrBadPrefix", err)
	}
	// A refresh-prefixed token is not a valid access prefix.
	if _, err := ts.ValidateAccess(context.Background(), RefreshPrefix+"x"); err != ErrBadPrefix {
		t.Fatalf("ValidateAccess refresh-prefixed = %v, want ErrBadPrefix", err)
	}
}

func TestTokenStoreValidateAccessExpired(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")
	pair := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	// Advance past access TTL.
	clk.current = clk.current.Add(16 * time.Minute)
	if _, err := ts.ValidateAccess(ctx, pair.AccessToken); err != ErrExpired {
		t.Fatalf("ValidateAccess expired = %v, want ErrExpired", err)
	}
}

func TestTokenStoreValidateAccessRevoked(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")
	pair := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	if err := ts.RevokeChain(ctx, pair.ChainID); err != nil {
		t.Fatalf("RevokeChain: %v", err)
	}
	if _, err := ts.ValidateAccess(ctx, pair.AccessToken); err != ErrRevoked {
		t.Fatalf("ValidateAccess revoked = %v, want ErrRevoked", err)
	}
}

func TestTokenStoreLookupAndMarkRefreshUsed(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")
	pair := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	// Bad prefix path on refresh lookup.
	tx0, _ := d.BeginTx(ctx, nil)
	if _, _, err := ts.LookupRefreshTx(ctx, tx0, "bad_token"); err != ErrBadPrefix {
		t.Fatalf("LookupRefreshTx bad prefix = %v, want ErrBadPrefix", err)
	}
	tx0.Rollback()

	tx, err := d.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	tok, chain, err := ts.LookupRefreshTx(ctx, tx, pair.RefreshToken)
	if err != nil {
		tx.Rollback()
		t.Fatalf("LookupRefreshTx: %v", err)
	}
	if tok.Kind != KindRefresh {
		t.Errorf("kind = %q, want refresh", tok.Kind)
	}
	if chain.ID != pair.ChainID {
		t.Errorf("chain id = %q, want %q", chain.ID, pair.ChainID)
	}
	if tok.UsedAt != nil {
		t.Error("fresh refresh token already used")
	}

	if err := ts.MarkRefreshUsed(ctx, tx, tok.ID); err != nil {
		tx.Rollback()
		t.Fatalf("MarkRefreshUsed: %v", err)
	}

	tok2, _, err := ts.LookupRefreshTx(ctx, tx, pair.RefreshToken)
	if err != nil {
		tx.Rollback()
		t.Fatalf("LookupRefreshTx after mark: %v", err)
	}
	if tok2.UsedAt == nil {
		t.Error("used_at not set after MarkRefreshUsed")
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}
}

func TestTokenStoreIssueSuccessorTokensTx(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")
	pair := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	tx, err := d.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	succ, err := ts.IssueSuccessorTokensTx(ctx, tx, pair.ChainID)
	if err != nil {
		tx.Rollback()
		t.Fatalf("IssueSuccessorTokensTx: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("Commit: %v", err)
	}

	if succ.ChainID != pair.ChainID {
		t.Errorf("successor chain id = %q, want %q", succ.ChainID, pair.ChainID)
	}
	if succ.AccessToken == pair.AccessToken || succ.RefreshToken == pair.RefreshToken {
		t.Error("successor tokens equal predecessor tokens")
	}
	// Both old and new access tokens validate (same live chain).
	if _, err := ts.ValidateAccess(ctx, succ.AccessToken); err != nil {
		t.Fatalf("ValidateAccess successor: %v", err)
	}
}

func TestTokenStoreRevokeChainIdempotent(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")
	pair := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	if err := ts.RevokeChain(ctx, pair.ChainID); err != nil {
		t.Fatalf("RevokeChain: %v", err)
	}
	// Idempotent.
	if err := ts.RevokeChain(ctx, pair.ChainID); err != nil {
		t.Fatalf("RevokeChain second call: %v", err)
	}

	ch, err := ts.GetChain(ctx, pair.ChainID)
	if err != nil {
		t.Fatalf("GetChain: %v", err)
	}
	if ch.RevokedAt == nil {
		t.Error("chain revoked_at not set")
	}

	// Both tokens carry revoked_at.
	var count int
	if err := d.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM oauth_tokens WHERE chain_id = ? AND revoked_at IS NOT NULL`,
		pair.ChainID).Scan(&count); err != nil {
		t.Fatalf("count revoked tokens: %v", err)
	}
	if count != 2 {
		t.Errorf("revoked token count = %d, want 2", count)
	}
}

func TestTokenStoreListChainsByOwner(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "Named App")

	// Oldest chain.
	clk.current = time.Date(2026, 5, 1, 0, 0, 0, 0, time.UTC)
	older := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	// Newer chain.
	clk.current = time.Date(2026, 5, 2, 0, 0, 0, 0, time.UTC)
	newer := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	// A revoked chain (must be excluded).
	clk.current = time.Date(2026, 5, 3, 0, 0, 0, 0, time.UTC)
	revoked := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")
	if err := ts.RevokeChain(ctx, revoked.ChainID); err != nil {
		t.Fatalf("RevokeChain: %v", err)
	}

	// A chain for a different owner (must be excluded).
	_ = issueChain(t, d, ts, clientID, "other@example.com", "https://e/mcp")

	summaries, err := ts.ListChainsByOwner(ctx, "owner@example.com")
	if err != nil {
		t.Fatalf("ListChainsByOwner: %v", err)
	}
	if len(summaries) != 2 {
		t.Fatalf("got %d summaries, want 2", len(summaries))
	}
	// Newest first.
	if summaries[0].PublicID != newer.ChainPublicID {
		t.Errorf("summaries[0] = %q, want newer %q", summaries[0].PublicID, newer.ChainPublicID)
	}
	if summaries[1].PublicID != older.ChainPublicID {
		t.Errorf("summaries[1] = %q, want older %q", summaries[1].PublicID, older.ChainPublicID)
	}
	if summaries[0].ClientName != "Named App" {
		t.Errorf("client_name = %q, want Named App", summaries[0].ClientName)
	}
	if summaries[0].ClientID != clientID {
		t.Errorf("client_id = %q, want %q", summaries[0].ClientID, clientID)
	}
	if summaries[0].LastUsedAt.IsZero() {
		t.Error("last_used_at is zero")
	}
}

func TestTokenStoreGetChainByPublicID(t *testing.T) {
	d := openTestDB(t)
	ctx := context.Background()
	clk := newClock()
	ts := NewTokenStore(d, 15*time.Minute, 24*time.Hour)
	ts.Now = clk.now
	clientID := registerClient(t, d, "App")
	pair := issueChain(t, d, ts, clientID, "owner@example.com", "https://e/mcp")

	ch, err := ts.GetChainByPublicID(ctx, pair.ChainPublicID)
	if err != nil {
		t.Fatalf("GetChainByPublicID: %v", err)
	}
	if ch.ID != pair.ChainID || ch.PublicID != pair.ChainPublicID {
		t.Errorf("chain mismatch: %+v", ch)
	}

	if _, err := ts.GetChainByPublicID(ctx, "no-such-public-id"); err != ErrNotFound {
		t.Fatalf("GetChainByPublicID unknown = %v, want ErrNotFound", err)
	}
}

func TestTokenStoreGetChainUnknown(t *testing.T) {
	d := openTestDB(t)
	ts := NewTokenStore(d, time.Minute, time.Hour)
	if _, err := ts.GetChain(context.Background(), "no-such-id"); err != ErrNotFound {
		t.Fatalf("GetChain unknown = %v, want ErrNotFound", err)
	}
}
