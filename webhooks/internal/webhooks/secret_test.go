package webhooks

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
	"time"

	chassis "appkit/db"

	"webhooks/internal/db"
)

// fixedClock is a deterministic Clock for reproducible created_at values.
type fixedClock struct{ t time.Time }

func (c fixedClock) Now() time.Time { return c.t }

// newTestService stands up a real temp-file SQLite (never :memory:), migrates
// it, and returns a Service with a deterministic clock plus the raw *sql.DB for
// re-reading rows directly.
func newTestService(t *testing.T) (*Service, *sql.DB, time.Time) {
	t.Helper()
	dbPath := filepath.Join(t.TempDir(), "webhooks.db")
	conn, err := chassis.Open(dbPath)
	if err != nil {
		t.Fatalf("chassis.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := chassis.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("chassis.LoadMigrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("chassis.Migrate: %v", err)
	}
	now := time.Date(2026, 6, 25, 12, 0, 0, 0, time.UTC)
	return NewService(conn, fixedClock{t: now}), conn, now
}

// readHash re-reads secret_hash straight from the row, bypassing the Store.
func readHash(t *testing.T, conn *sql.DB, name string) string {
	t.Helper()
	var h string
	if err := conn.QueryRow(`SELECT secret_hash FROM webhooks WHERE name = ?`, name).Scan(&h); err != nil {
		t.Fatalf("read secret_hash for %q: %v", name, err)
	}
	return h
}

func sha256hex(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:])
}

// R-37GT-C05G — Create persists only the hash of a prefixed secret.
func TestCreatePersistsHashOfPrefixedSecret(t *testing.T) {
	svc, conn, _ := newTestService(t)
	ctx := context.Background()

	w, secret, err := svc.Create(ctx, "alice@example.com", "deploy-hook")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if !strings.HasPrefix(secret, "ms_wh_") {
		t.Fatalf("secret %q does not begin with ms_wh_", secret)
	}
	if w.Name != "deploy-hook" {
		t.Fatalf("Webhook.Name = %q, want deploy-hook", w.Name)
	}

	stored := readHash(t, conn, "deploy-hook")
	if stored == secret {
		t.Fatal("stored secret_hash equals the plaintext secret; must be hashed")
	}
	if stored != sha256hex(secret) {
		t.Fatalf("stored secret_hash = %q, want sha256hex(secret) = %q", stored, sha256hex(secret))
	}
}

// R-38OP-PRW5 — verifySecret matches exactly the secret whose hash is stored.
func TestVerifySecret(t *testing.T) {
	secret := "ms_wh_ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	h := sha256hex(secret)

	if !verifySecret(secret, h) {
		t.Fatal("verifySecret returned false for the exact secret")
	}
	if verifySecret("ms_wh_completely-different", h) {
		t.Fatal("verifySecret returned true for an unrelated secret")
	}
	// Differ by a single character.
	oneOff := secret[:len(secret)-1] + "2"
	if oneOff == secret {
		t.Fatal("test bug: oneOff equals secret")
	}
	if verifySecret(oneOff, h) {
		t.Fatal("verifySecret returned true for a one-character-different secret")
	}
}

// R-39WM-3JMU — Rotate swaps the secret while leaving identity columns intact.
func TestRotateReplacesSecretKeepsIdentity(t *testing.T) {
	svc, conn, now := newTestService(t)
	ctx := context.Background()

	w, oldSecret, err := svc.Create(ctx, "alice@example.com", "rotate-me")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	oldHash := readHash(t, conn, "rotate-me")

	newSecret, err := svc.Rotate(ctx, "alice@example.com", "rotate-me")
	if err != nil {
		t.Fatalf("Rotate: %v", err)
	}
	if newSecret == oldSecret {
		t.Fatal("Rotate returned the same secret")
	}

	newHash := readHash(t, conn, "rotate-me")
	if !verifySecret(newSecret, newHash) {
		t.Fatal("new secret does not verify against the rotated hash")
	}
	if verifySecret(oldSecret, newHash) {
		t.Fatal("old secret still verifies after rotation")
	}
	if newHash == oldHash {
		t.Fatal("secret_hash unchanged after Rotate")
	}

	// Identity columns unchanged.
	got, _, _, ok, err := svc.store.GetByName(ctx, "rotate-me")
	if err != nil || !ok {
		t.Fatalf("GetByName after rotate: ok=%v err=%v", ok, err)
	}
	if got.Name != w.Name || got.OwnerEmail != "alice@example.com" {
		t.Fatalf("identity changed: %+v", got)
	}
	if !got.CreatedAt.Equal(now) {
		t.Fatalf("created_at changed: got %v want %v", got.CreatedAt, now)
	}
}

// R-39WM-3JMU (cont.) — Rotate on a missing or not-owned webhook is ErrNotFound.
func TestRotateNotFound(t *testing.T) {
	svc, _, _ := newTestService(t)
	ctx := context.Background()

	if _, err := svc.Rotate(ctx, "alice@example.com", "nope"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Rotate missing: err = %v, want ErrNotFound", err)
	}

	if _, _, err := svc.Create(ctx, "alice@example.com", "owned"); err != nil {
		t.Fatalf("Create: %v", err)
	}
	if _, err := svc.Rotate(ctx, "mallory@example.com", "owned"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("Rotate not-owned: err = %v, want ErrNotFound", err)
	}
}

// R-3CCE-V348 — neither the Webhook value object nor a listed item exposes the
// secret or its hash; plaintext is only ever returned by Create/Rotate.
func TestWebhookValueCarriesNoSecret(t *testing.T) {
	svc, _, _ := newTestService(t)
	ctx := context.Background()

	if _, _, err := svc.Create(ctx, "alice@example.com", "hidden"); err != nil {
		t.Fatalf("Create: %v", err)
	}

	list, err := svc.store.ListByOwner(ctx, "alice@example.com")
	if err != nil || len(list) != 1 {
		t.Fatalf("ListByOwner: n=%d err=%v", len(list), err)
	}
	blob, err := json.Marshal(list[0])
	if err != nil {
		t.Fatalf("marshal list item: %v", err)
	}
	js := strings.ToLower(string(blob))
	if strings.Contains(js, "secret") || strings.Contains(js, "hash") {
		t.Fatalf("serialized webhook leaks secret material: %s", blob)
	}

	got, secretHash, _, ok, err := svc.store.GetByName(ctx, "hidden")
	if err != nil || !ok {
		t.Fatalf("GetByName: ok=%v err=%v", ok, err)
	}
	gblob, _ := json.Marshal(got)
	gjs := strings.ToLower(string(gblob))
	if strings.Contains(gjs, "secret") || strings.Contains(gjs, "hash") {
		t.Fatalf("GetByName Webhook leaks secret material: %s", gblob)
	}
	// The hash comes back as a separate return value, never on the value object.
	if secretHash == "" {
		t.Fatal("expected a secret hash returned separately from the Webhook")
	}
}

// R-3DKB-8UUX — invalid user names are rejected with no row written; a valid
// name is accepted and persisted.
func TestCreateValidatesUserName(t *testing.T) {
	svc, _, _ := newTestService(t)
	ctx := context.Background()

	invalid := []string{
		"has/slash",
		"has.dot",
		"has space",
		strings.Repeat("a", 65),
		"bad!char",
	}
	for _, name := range invalid {
		_, _, err := svc.Create(ctx, "alice@example.com", name)
		if !errors.Is(err, ErrInvalidName) {
			t.Fatalf("Create(%q): err = %v, want ErrInvalidName", name, err)
		}
		if _, _, _, ok, err := svc.store.GetByName(ctx, name); err != nil || ok {
			t.Fatalf("invalid name %q wrote a row (ok=%v err=%v)", name, ok, err)
		}
	}
	if list, err := svc.store.ListByOwner(ctx, "alice@example.com"); err != nil || len(list) != 0 {
		t.Fatalf("expected no rows after invalid Creates: n=%d err=%v", len(list), err)
	}

	w, _, err := svc.Create(ctx, "alice@example.com", "valid_Name-09")
	if err != nil {
		t.Fatalf("Create valid name: %v", err)
	}
	if _, _, _, ok, err := svc.store.GetByName(ctx, w.Name); err != nil || !ok {
		t.Fatalf("valid name not persisted: ok=%v err=%v", ok, err)
	}
}

// R-3DKB-8UUX (cont.) — a duplicate name maps to ErrNameTaken.
func TestCreateDuplicateName(t *testing.T) {
	svc, _, _ := newTestService(t)
	ctx := context.Background()

	if _, _, err := svc.Create(ctx, "alice@example.com", "dup"); err != nil {
		t.Fatalf("first Create: %v", err)
	}
	if _, _, err := svc.Create(ctx, "alice@example.com", "dup"); !errors.Is(err, ErrNameTaken) {
		t.Fatalf("duplicate Create: err = %v, want ErrNameTaken", err)
	}
}

// R-3ES7-MMLM — an empty name generates a 26-char [A-Z2-7] name, and two empty
// creates yield different names.
func TestCreateEmptyNameGenerates(t *testing.T) {
	svc, _, _ := newTestService(t)
	ctx := context.Background()

	genRE := regexp.MustCompile(`^[A-Z2-7]{26}$`)

	w1, _, err := svc.Create(ctx, "alice@example.com", "")
	if err != nil {
		t.Fatalf("Create empty #1: %v", err)
	}
	w2, _, err := svc.Create(ctx, "alice@example.com", "")
	if err != nil {
		t.Fatalf("Create empty #2: %v", err)
	}
	for _, n := range []string{w1.Name, w2.Name} {
		if !genRE.MatchString(n) {
			t.Fatalf("generated name %q is not 26 chars over [A-Z2-7]", n)
		}
	}
	if w1.Name == w2.Name {
		t.Fatalf("two empty-name creates produced the same name %q", w1.Name)
	}
}
