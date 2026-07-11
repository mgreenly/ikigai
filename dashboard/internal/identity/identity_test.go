package identity

import (
	"context"
	"database/sql"
	"errors"
	"path/filepath"
	"regexp"
	"testing"
	"time"

	"dashboard/internal/db"
)

func newTestStore(t *testing.T) (*Store, *time.Time) {
	t.Helper()
	conn, err := db.Open(filepath.Join(t.TempDir(), "identity.db"))
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	now := time.Date(2026, 7, 10, 12, 0, 0, 0, time.UTC)
	s := NewStore(conn)
	s.Now = func() time.Time { return now }
	return s, &now
}

func TestResolveOrCreateInsertsAndLookupReturnsIdentity(t *testing.T) {
	// R-VJMO-6CN9
	s, _ := newTestStore(t)
	claims := Claims{Iss: "https://accounts.google.com", Sub: "subject-1", Email: "a@example.com", Name: "Ada", Picture: "https://example.com/a.png"}

	id, err := s.ResolveOrCreate(context.Background(), claims)
	if err != nil {
		t.Fatalf("ResolveOrCreate: %v", err)
	}
	var count int
	if err := s.DB.QueryRow(`SELECT count(*) FROM identities`).Scan(&count); err != nil {
		t.Fatalf("count identities: %v", err)
	}
	if count != 1 {
		t.Fatalf("identities count = %d, want 1", count)
	}
	got, err := s.Lookup(context.Background(), id)
	if err != nil {
		t.Fatalf("Lookup: %v", err)
	}
	if got.ID != id || got.Iss != claims.Iss || got.Sub != claims.Sub || got.Email != claims.Email || got.Name != claims.Name || got.Picture != claims.Picture {
		t.Errorf("Lookup = %+v, want handle and claims %+v", got, claims)
	}
}

func TestResolveOrCreateUsesOpaqueGeneratedHandle(t *testing.T) {
	// R-VKUK-K4DY
	s, _ := newTestStore(t)
	s.New = func() string { return "test-opaque-handle" }
	stubID, err := s.ResolveOrCreate(context.Background(), Claims{Iss: "issuer", Sub: "stub", Email: "stub@example.com"})
	if err != nil {
		t.Fatalf("ResolveOrCreate stub: %v", err)
	}
	if stubID != "test-opaque-handle" {
		t.Fatalf("stub handle = %q, want injected value", stubID)
	}

	s.New = NewStore(s.DB).New
	first, err := s.ResolveOrCreate(context.Background(), Claims{Iss: "issuer", Sub: "one", Email: "one@example.com"})
	if err != nil {
		t.Fatalf("ResolveOrCreate first: %v", err)
	}
	second, err := s.ResolveOrCreate(context.Background(), Claims{Iss: "issuer", Sub: "two", Email: "two@example.com"})
	if err != nil {
		t.Fatalf("ResolveOrCreate second: %v", err)
	}
	if first == second || regexp.MustCompile(`^[0-9]+$`).MatchString(first) || regexp.MustCompile(`^[0-9]+$`).MatchString(second) || first == "one@example.com" || second == "two@example.com" {
		t.Errorf("real handles = %q, %q; want distinct opaque non-email non-numeric values", first, second)
	}
}

func TestResolveOrCreateSameSubjectKeepsOneIdentity(t *testing.T) {
	// R-VM2G-XW4N
	s, _ := newTestStore(t)
	ctx := context.Background()
	first, err := s.ResolveOrCreate(ctx, Claims{Iss: "issuer", Sub: "subject", Email: "old@example.com", Name: "Old", Picture: "old"})
	if err != nil {
		t.Fatalf("first ResolveOrCreate: %v", err)
	}
	second, err := s.ResolveOrCreate(ctx, Claims{Iss: "issuer", Sub: "subject", Email: "new@example.com", Name: "New", Picture: "new"})
	if err != nil {
		t.Fatalf("second ResolveOrCreate: %v", err)
	}
	var count int
	if err := s.DB.QueryRowContext(ctx, `SELECT count(*) FROM identities WHERE iss = ? AND sub = ?`, "issuer", "subject").Scan(&count); err != nil {
		t.Fatalf("count identities: %v", err)
	}
	if first != second || count != 1 {
		t.Errorf("ids/count = %q/%q/%d, want same handle and one row", first, second, count)
	}
}

func TestResolveOrCreateRefreshesAttributesWithoutChangingCreation(t *testing.T) {
	// R-VNAD-BNVC
	s, now := newTestStore(t)
	ctx := context.Background()
	id, err := s.ResolveOrCreate(ctx, Claims{Iss: "issuer", Sub: "subject", Email: "old@example.com", Name: "Old", Picture: "old"})
	if err != nil {
		t.Fatalf("first ResolveOrCreate: %v", err)
	}
	var created string
	if err := s.DB.QueryRowContext(ctx, `SELECT created_at FROM identities WHERE id = ?`, id).Scan(&created); err != nil {
		t.Fatalf("read created_at: %v", err)
	}
	var updated string
	if err := s.DB.QueryRowContext(ctx, `SELECT updated_at FROM identities WHERE id = ?`, id).Scan(&updated); err != nil {
		t.Fatalf("read updated_at: %v", err)
	}
	*now = now.Add(time.Minute)
	updatedID, err := s.ResolveOrCreate(ctx, Claims{Iss: "issuer", Sub: "subject", Email: "new@example.com", Name: "New", Picture: "new"})
	if err != nil {
		t.Fatalf("second ResolveOrCreate: %v", err)
	}
	got, err := s.Lookup(ctx, id)
	if err != nil {
		t.Fatalf("Lookup: %v", err)
	}
	var createdAfter string
	if err := s.DB.QueryRowContext(ctx, `SELECT created_at FROM identities WHERE id = ?`, id).Scan(&createdAfter); err != nil {
		t.Fatalf("read created_at after update: %v", err)
	}
	var updatedAfter string
	if err := s.DB.QueryRowContext(ctx, `SELECT updated_at FROM identities WHERE id = ?`, id).Scan(&updatedAfter); err != nil {
		t.Fatalf("read updated_at after update: %v", err)
	}
	if updatedID != id || createdAfter != created || updatedAfter == updated || got.Email != "new@example.com" || got.Name != "New" || got.Picture != "new" {
		t.Errorf("updated identity = %+v, id=%q, created=%q, updated=%q; want same id/created with refreshed attributes/timestamp", got, updatedID, createdAfter, updatedAfter)
	}
}

func TestLookupUnknownHandleReturnsNotFound(t *testing.T) {
	// R-VOI9-PFM1
	s, _ := newTestStore(t)
	got, err := s.Lookup(context.Background(), "missing")
	if !errors.Is(err, sql.ErrNoRows) {
		t.Fatalf("Lookup unknown error = %v, want sql.ErrNoRows", err)
	}
	if got != (Identity{}) {
		t.Errorf("Lookup unknown identity = %+v, want zero value", got)
	}
}
