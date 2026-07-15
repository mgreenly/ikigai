package webhooks

import (
	"context"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/hex"
	"errors"
	"regexp"

	"webhooks/internal/db"
	"webhooks/internal/ids"
)

// secretPrefix marks a webhook signing secret so it is recognizable in logs and
// configuration. The plaintext is shown exactly once (at Create/Rotate); bearer
// stores only its hash, while github-hmac retains the key required to verify HMACs.
const secretPrefix = "ms_wh_"

// nameRE constrains user-supplied webhook names. Generated names (from ids.New)
// are always valid by construction and skip this check.
var nameRE = regexp.MustCompile(`^[A-Za-z0-9_-]{1,64}$`)

// Sentinel errors callers can match with errors.Is.
var (
	ErrNameTaken           = errors.New("webhook name already in use")
	ErrInvalidName         = errors.New("invalid webhook name")
	ErrNotFound            = errors.New("webhook not found")
	ErrInvalidVerification = errors.New("invalid verification scheme")
)

// newName mints a fresh opaque webhook name (26 chars over [A-Z2-7]).
func newName() string { return ids.New() }

// newSecret mints a fresh prefixed signing secret. The random tail is a full
// 128-bit ids.New value.
func newSecret() string { return secretPrefix + ids.New() }

// hashSecret returns the lowercase hex sha256 of plaintext — exactly what is
// stored in secret_hash.
func hashSecret(plaintext string) string {
	sum := sha256.Sum256([]byte(plaintext))
	return hex.EncodeToString(sum[:])
}

// verifySecret reports whether presented hashes to storedHash, using a
// constant-time comparison over the hex digests so it leaks no timing signal.
func verifySecret(presented, storedHash string) bool {
	return subtle.ConstantTimeCompare([]byte(hashSecret(presented)), []byte(storedHash)) == 1
}

// validateName enforces ^[A-Za-z0-9_-]{1,64}$ on user-supplied names.
func validateName(name string) error {
	if !nameRE.MatchString(name) {
		return ErrInvalidName
	}
	return nil
}

// Create provisions a new webhook owned by owner. An empty name is replaced by a
// freshly-generated opaque name; a non-empty name must match
// ^[A-Za-z0-9_-]{1,64}$ or Create returns ErrInvalidName with no row written.
// Bearer persists only hashSecret(secret); github-hmac additionally retains the
// key needed for verification. The plaintext is returned for the caller to show
// once. A duplicate name maps the PRIMARY KEY violation to ErrNameTaken.
func (s *Service) Create(ctx context.Context, owner, name string, requested ...string) (w db.Webhook, secret string, err error) {
	verification := "bearer"
	if len(requested) > 0 && requested[0] != "" {
		verification = requested[0]
	}
	if verification != "bearer" && verification != "github-hmac" {
		return db.Webhook{}, "", ErrInvalidVerification
	}
	if name == "" {
		name = newName()
	} else if err := validateName(name); err != nil {
		return db.Webhook{}, "", err
	}

	secret = newSecret()
	w = db.Webhook{
		Name:         name,
		OwnerEmail:   owner,
		Verification: verification,
		CreatedAt:    s.clock.Now(),
	}
	var insertErr error
	if verification == "github-hmac" {
		insertErr = s.store.Insert(ctx, w, hashSecret(secret), secret)
	} else {
		insertErr = s.store.Insert(ctx, w, hashSecret(secret))
	}
	if insertErr != nil {
		if isDuplicate(ctx, s, name) {
			return db.Webhook{}, "", ErrNameTaken
		}
		return db.Webhook{}, "", insertErr
	}
	return w, secret, nil
}

// Rotate issues a fresh secret for owner's webhook, invalidating the old one.
// name, owner_email, and created_at are untouched. A missing or not-owned
// webhook (Store.UpdateSecret reporting updated==false) maps to ErrNotFound.
func (s *Service) Rotate(ctx context.Context, owner, name string) (secret string, err error) {
	secret = newSecret()
	updated, err := s.store.UpdateSecret(ctx, owner, name, hashSecret(secret), secret)
	if err != nil {
		return "", err
	}
	if !updated {
		return "", ErrNotFound
	}
	return secret, nil
}

// isDuplicate distinguishes a PRIMARY KEY collision from other Insert failures:
// a row already exists under this name.
func isDuplicate(ctx context.Context, s *Service, name string) bool {
	_, _, _, ok, err := s.store.GetByName(ctx, name)
	return err == nil && ok
}
