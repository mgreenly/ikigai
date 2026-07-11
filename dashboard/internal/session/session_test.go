package session

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

func testStore(t *testing.T) *SessionStore {
	t.Helper()
	database, err := db.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { database.Close() })
	return NewSessionStore(database)
}

// backdate rewrites a session's timestamp columns directly so a test can
// simulate elapsed time without a clock seam: the store reads time.Now(), so
// pushing the row into the past is how we exercise the expiry ceilings.
func backdate(t *testing.T, s *SessionStore, id, col string, when time.Time) {
	t.Helper()
	if _, err := s.db.Exec(
		`UPDATE web_sessions SET `+col+` = ? WHERE id = ?`,
		when.UTC().Format(time.RFC3339Nano), id,
	); err != nil {
		t.Fatalf("backdate %s: %v", col, err)
	}
}

// TestCreateStoresHashNotCookie is the core security property: the value
// returned to the caller (destined for the browser) is the plaintext cookie,
// while the value persisted is its SHA-256 hash — never the plaintext.
func TestCreateStoresHashNotCookie(t *testing.T) {
	store := testStore(t)
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if issued.ID == "" || issued.CookieValue == "" {
		t.Fatal("Create returned empty id or cookie")
	}

	sum := sha256.Sum256([]byte(issued.CookieValue))
	wantHash := hex.EncodeToString(sum[:])

	var ownerEmail, storedHash string
	if err := store.db.QueryRow(
		`SELECT owner_email, cookie_hash FROM web_sessions WHERE id = ?`, issued.ID,
	).Scan(&ownerEmail, &storedHash); err != nil {
		t.Fatalf("read row: %v", err)
	}
	if ownerEmail != "owner@int.ikigenba.com" {
		t.Errorf("owner_email = %q, want owner@int.ikigenba.com", ownerEmail)
	}
	if storedHash != wantHash {
		t.Errorf("stored hash = %q, want sha256(cookie) %q", storedHash, wantHash)
	}
	if storedHash == issued.CookieValue {
		t.Error("plaintext cookie equals stored hash — they must differ")
	}
}

// TestCreateSetsAbsoluteExpiry checks the absolute ceiling is stamped 12h out.
func TestCreateSetsAbsoluteExpiry(t *testing.T) {
	store := testStore(t)
	before := time.Now().UTC()
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if got := issued.ExpiresAt.Sub(before); got < absoluteTimeout-time.Minute || got > absoluteTimeout+time.Minute {
		t.Errorf("expiry window = %v, want ~%v", got, absoluteTimeout)
	}
}

// TestCreateMintsUniqueValues guards against a botched RNG handing out repeats.
func TestCreateMintsUniqueValues(t *testing.T) {
	store := testStore(t)
	a, err := store.Create(context.Background(), "a@int.ikigenba.com", "owner-a")
	if err != nil {
		t.Fatalf("Create a: %v", err)
	}
	b, err := store.Create(context.Background(), "b@int.ikigenba.com", "owner-b")
	if err != nil {
		t.Fatalf("Create b: %v", err)
	}
	if a.ID == b.ID {
		t.Error("two sessions share an id")
	}
	if a.CookieValue == b.CookieValue {
		t.Error("two sessions share a cookie")
	}
}

// TestLookupReturnsLiveSession is the happy path: a freshly created session is
// found and returned with its owner.
func TestLookupReturnsLiveSession(t *testing.T) {
	store := testStore(t)
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	sess, err := store.Lookup(context.Background(), issued.CookieValue)
	if err != nil {
		t.Fatalf("Lookup: %v", err)
	}
	if sess.ID != issued.ID {
		t.Errorf("Lookup id = %q, want %q", sess.ID, issued.ID)
	}
	if sess.OwnerEmail != "owner@int.ikigenba.com" {
		t.Errorf("Lookup owner = %q, want owner@int.ikigenba.com", sess.OwnerEmail)
	}
}

// TestLookupTouchesLastSeen proves the idle clock restarts: an old last_seen_at
// (still within the idle window) is bumped to ~now on a successful lookup.
func TestLookupTouchesLastSeen(t *testing.T) {
	store := testStore(t)
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	// Half an idle window in the past — live, but stale enough to observe a bump.
	stale := time.Now().UTC().Add(-idleTimeout / 2)
	backdate(t, store, issued.ID, "last_seen_at", stale)

	sess, err := store.Lookup(context.Background(), issued.CookieValue)
	if err != nil {
		t.Fatalf("Lookup: %v", err)
	}
	if !sess.LastSeenAt.After(stale) {
		t.Errorf("last_seen_at = %v, want bumped past %v", sess.LastSeenAt, stale)
	}
}

// TestLookupEmptyCookie rejects an empty cookie value without a DB hit.
func TestLookupEmptyCookie(t *testing.T) {
	store := testStore(t)
	if _, err := store.Lookup(context.Background(), ""); !errors.Is(err, ErrInvalid) {
		t.Errorf("err = %v, want ErrInvalid", err)
	}
}

// TestLookupUnknownCookie treats an unrecognized cookie as not found.
func TestLookupUnknownCookie(t *testing.T) {
	store := testStore(t)
	_, err := store.Lookup(context.Background(), "nonexistent")
	if !errors.Is(err, ErrNotFound) {
		t.Errorf("err = %v, want ErrNotFound", err)
	}
}

// TestLookupAbsoluteExpired rejects a session past its 12h absolute ceiling,
// even though it was just touched (last_seen_at is current).
func TestLookupAbsoluteExpired(t *testing.T) {
	store := testStore(t)
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	backdate(t, store, issued.ID, "expires_at", time.Now().UTC().Add(-time.Minute))

	_, err = store.Lookup(context.Background(), issued.CookieValue)
	if !errors.Is(err, ErrAbsoluteExpired) {
		t.Errorf("err = %v, want ErrAbsoluteExpired", err)
	}
}

// TestLookupIdleExpired rejects a session idle past 1h, even though its absolute
// ceiling is still in the future.
func TestLookupIdleExpired(t *testing.T) {
	store := testStore(t)
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	backdate(t, store, issued.ID, "last_seen_at", time.Now().UTC().Add(-idleTimeout-time.Minute))

	_, err = store.Lookup(context.Background(), issued.CookieValue)
	if !errors.Is(err, ErrIdleExpired) {
		t.Errorf("err = %v, want ErrIdleExpired", err)
	}
}

// TestRevokeRejectsCookie is the logout contract: after Revoke the same cookie
// no longer redeems a session.
func TestRevokeRejectsCookie(t *testing.T) {
	store := testStore(t)
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if err := store.Revoke(context.Background(), issued.CookieValue); err != nil {
		t.Fatalf("Revoke: %v", err)
	}
	_, err = store.Lookup(context.Background(), issued.CookieValue)
	if !errors.Is(err, ErrRevoked) {
		t.Errorf("err = %v, want ErrRevoked", err)
	}
}

// TestRevokeIsIdempotent: a second logout is a no-op and never errors.
func TestRevokeIsIdempotent(t *testing.T) {
	store := testStore(t)
	issued, err := store.Create(context.Background(), "owner@int.ikigenba.com", "owner-1")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if err := store.Revoke(context.Background(), issued.CookieValue); err != nil {
		t.Fatalf("first Revoke: %v", err)
	}
	if err := store.Revoke(context.Background(), issued.CookieValue); err != nil {
		t.Errorf("second Revoke: %v, want nil", err)
	}
}

// TestRevokeUnknownCookie touches nothing and does not error.
func TestRevokeUnknownCookie(t *testing.T) {
	store := testStore(t)
	if err := store.Revoke(context.Background(), "nonexistent"); err != nil {
		t.Errorf("Revoke unknown: %v, want nil", err)
	}
}
