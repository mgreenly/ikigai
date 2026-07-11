package pat

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
	return &clock{current: time.Date(2026, 6, 8, 12, 0, 0, 0, time.UTC)}
}

func newStore(t *testing.T) (*Store, *clock) {
	t.Helper()
	s := NewStore(openTestDB(t))
	clk := newClock()
	s.Now = clk.now
	return s, clk
}

func TestCreateValidateRoundTrip(t *testing.T) {
	s, clk := newStore(t)
	ctx := context.Background()

	plaintext, p, err := s.Create(ctx, "alice@example.com", "owner-alice", "Claude Code")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if !strings.HasPrefix(plaintext, Prefix) {
		t.Fatalf("plaintext = %q, want prefix %q", plaintext, Prefix)
	}
	if p.ID == "" || p.PublicID == "" {
		t.Fatal("Create returned empty id/public_id")
	}
	if p.OwnerEmail != "alice@example.com" || p.Label != "Claude Code" {
		t.Errorf("owner/label = %q/%q", p.OwnerEmail, p.Label)
	}
	if !p.CreatedAt.Equal(clk.current) {
		t.Errorf("created_at = %v, want %v", p.CreatedAt, clk.current)
	}
	if p.ExpiresAt != nil {
		t.Errorf("expires_at = %v, want nil", p.ExpiresAt)
	}

	got, err := s.ValidatePAT(ctx, plaintext)
	if err != nil {
		t.Fatalf("ValidatePAT: %v", err)
	}
	if got.ID != p.ID || got.PublicID != p.PublicID {
		t.Errorf("validated id/public_id mismatch: %+v vs %+v", got, p)
	}
}

func TestStoredRowHoldsHashNotPlaintext(t *testing.T) {
	s := NewStore(openTestDB(t))
	ctx := context.Background()

	plaintext, p, err := s.Create(ctx, "alice@example.com", "owner-alice", "lbl")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	var hash string
	if err := s.DB.QueryRowContext(ctx, `SELECT token_hash FROM personal_tokens WHERE id = ?`, p.ID).Scan(&hash); err != nil {
		t.Fatalf("select token_hash: %v", err)
	}
	if hash == plaintext {
		t.Fatal("stored token_hash equals plaintext")
	}
	if hash != hashString(plaintext) {
		t.Errorf("stored hash = %q, want %q", hash, hashString(plaintext))
	}
}

func TestValidateBadPrefix(t *testing.T) {
	s := NewStore(openTestDB(t))
	if _, err := s.ValidatePAT(context.Background(), "ms_oat_whatever"); err != ErrBadPrefix {
		t.Fatalf("ValidatePAT bad prefix = %v, want ErrBadPrefix", err)
	}
}

func TestValidateNotFound(t *testing.T) {
	s := NewStore(openTestDB(t))
	if _, err := s.ValidatePAT(context.Background(), Prefix+"doesnotexist"); err != ErrNotFound {
		t.Fatalf("ValidatePAT unknown = %v, want ErrNotFound", err)
	}
}

func TestValidateRevoked(t *testing.T) {
	s, _ := newStore(t)
	ctx := context.Background()

	plaintext, p, err := s.Create(ctx, "alice@example.com", "owner-alice", "lbl")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if err := s.Revoke(ctx, p.ID); err != nil {
		t.Fatalf("Revoke: %v", err)
	}
	if _, err := s.ValidatePAT(ctx, plaintext); err != ErrRevoked {
		t.Fatalf("ValidatePAT revoked = %v, want ErrRevoked", err)
	}
}

func TestValidateExpired(t *testing.T) {
	s, clk := newStore(t)
	ctx := context.Background()

	plaintext, p, err := s.Create(ctx, "alice@example.com", "owner-alice", "lbl")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	// v1 never sets expires_at; synthesize an expired row to prove the gate honors it.
	past := clk.current.Add(-time.Hour).UTC().Format(time.RFC3339Nano)
	if _, err := s.DB.ExecContext(ctx, `UPDATE personal_tokens SET expires_at = ? WHERE id = ?`, past, p.ID); err != nil {
		t.Fatalf("set expires_at: %v", err)
	}
	if _, err := s.ValidatePAT(ctx, plaintext); err != ErrExpired {
		t.Fatalf("ValidatePAT expired = %v, want ErrExpired", err)
	}
}

func TestRevokeIdempotent(t *testing.T) {
	s, _ := newStore(t)
	ctx := context.Background()

	_, p, err := s.Create(ctx, "alice@example.com", "owner-alice", "lbl")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if err := s.Revoke(ctx, p.ID); err != nil {
		t.Fatalf("Revoke first: %v", err)
	}
	if err := s.Revoke(ctx, p.ID); err != nil {
		t.Fatalf("Revoke second: %v", err)
	}
}

func TestListByOwnerExcludesRevokedNewestFirst(t *testing.T) {
	s, clk := newStore(t)
	ctx := context.Background()

	// First PAT.
	_, p1, err := s.Create(ctx, "alice@example.com", "owner-alice", "first")
	if err != nil {
		t.Fatalf("Create p1: %v", err)
	}
	// Second PAT, created later.
	clk.current = clk.current.Add(time.Minute)
	_, p2, err := s.Create(ctx, "alice@example.com", "owner-alice", "second")
	if err != nil {
		t.Fatalf("Create p2: %v", err)
	}
	// A revoked PAT that must be excluded.
	clk.current = clk.current.Add(time.Minute)
	_, p3, err := s.Create(ctx, "alice@example.com", "owner-alice", "revoked")
	if err != nil {
		t.Fatalf("Create p3: %v", err)
	}
	if err := s.Revoke(ctx, p3.ID); err != nil {
		t.Fatalf("Revoke p3: %v", err)
	}
	// A different owner's PAT that must be excluded.
	if _, _, err := s.Create(ctx, "bob@example.com", "owner-bob", "other"); err != nil {
		t.Fatalf("Create bob: %v", err)
	}

	list, err := s.ListByOwner(ctx, "alice@example.com")
	if err != nil {
		t.Fatalf("ListByOwner: %v", err)
	}
	if len(list) != 2 {
		t.Fatalf("len(list) = %d, want 2", len(list))
	}
	if list[0].ID != p2.ID || list[1].ID != p1.ID {
		t.Errorf("order = [%s,%s], want newest-first [%s,%s]", list[0].ID, list[1].ID, p2.ID, p1.ID)
	}
}

func TestGetByPublicID(t *testing.T) {
	s, _ := newStore(t)
	ctx := context.Background()

	_, p, err := s.Create(ctx, "alice@example.com", "owner-alice", "lbl")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	got, err := s.GetByPublicID(ctx, p.PublicID)
	if err != nil {
		t.Fatalf("GetByPublicID: %v", err)
	}
	if got.ID != p.ID {
		t.Errorf("id = %q, want %q", got.ID, p.ID)
	}
	if _, err := s.GetByPublicID(ctx, "nope"); err != ErrNotFound {
		t.Fatalf("GetByPublicID unknown = %v, want ErrNotFound", err)
	}
}
