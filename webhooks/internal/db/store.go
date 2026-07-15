package db

import (
	"context"
	"database/sql"
	"time"
)

// timeFormat is the single text encoding for every timestamp column. RFC3339Nano
// in UTC sorts lexicographically in calendar order and round-trips losslessly,
// so created_at / last_triggered_at compare correctly straight from SQLite TEXT.
const timeFormat = time.RFC3339Nano

// Webhook is the domain value object. It deliberately carries NO secret material:
// the signing secret's hash lives only in the secret_hash column and is handed
// back separately by GetByName, so a Webhook can be logged or listed freely (D2).
type Webhook struct {
	Name            string
	OwnerEmail      string
	Verification    string
	CreatedAt       time.Time
	LastTriggeredAt *time.Time
}

// Store is the concrete persistence layer over a single *sql.DB. Every method
// runs real SQL against real SQLite — owner scoping and uniqueness are enforced
// by the database, not by pre-checks in Go.
type Store struct{ db *sql.DB }

// NewStore wraps an open *sql.DB (already migrated) in a Store.
func NewStore(db *sql.DB) *Store { return &Store{db: db} }

// Insert writes a webhook, its secret fingerprint, and optional retained HMAC key. A second Insert with the
// same name fails on the real PRIMARY KEY constraint and returns a non-nil error
// (the existing row is left untouched) — there is no read-modify pre-check.
func (s *Store) Insert(ctx context.Context, w Webhook, secretHash string, retainedSecret ...string) error {
	var lastTriggered any
	if w.LastTriggeredAt != nil {
		lastTriggered = w.LastTriggeredAt.UTC().Format(timeFormat)
	}
	verification := w.Verification
	if verification == "" {
		verification = "bearer"
	}
	var secret any
	if len(retainedSecret) > 0 {
		secret = retainedSecret[0]
	}
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO webhooks (name, owner_email, secret_hash, created_at, last_triggered_at, verification, secret)
		 VALUES (?, ?, ?, ?, ?, ?, ?)`,
		w.Name, w.OwnerEmail, secretHash, w.CreatedAt.UTC().Format(timeFormat), lastTriggered, verification, secret)
	return err
}

// GetByName resolves a webhook by its name. ok is false (with nil error) when no
// such row exists. Secret material is returned separately from the Webhook so
// the value object remains safe to list or log.
func (s *Store) GetByName(ctx context.Context, name string) (w Webhook, secretHash, secret string, ok bool, err error) {
	row := s.db.QueryRowContext(ctx,
		`SELECT name, owner_email, secret_hash, created_at, last_triggered_at, verification, secret
		 FROM webhooks WHERE name = ?`, name)
	w, secretHash, secret, err = scanWebhook(row)
	if err == sql.ErrNoRows {
		return Webhook{}, "", "", false, nil
	}
	if err != nil {
		return Webhook{}, "", "", false, err
	}
	return w, secretHash, secret, true, nil
}

// ListByOwner returns exactly the webhooks owned by owner, ordered by name. It is
// owner-scoped: another owner's rows are never returned.
func (s *Store) ListByOwner(ctx context.Context, owner string) ([]Webhook, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT name, owner_email, secret_hash, created_at, last_triggered_at, verification, secret
		 FROM webhooks WHERE owner_email = ? ORDER BY name`, owner)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var out []Webhook
	for rows.Next() {
		w, _, _, scanErr := scanWebhook(rows)
		if scanErr != nil {
			return nil, scanErr
		}
		out = append(out, w)
	}
	return out, rows.Err()
}

// Delete removes a webhook only when it is owned by owner. deleted reports whether
// a row was actually removed; an Insert by another owner is left untouched and
// deleted is false.
func (s *Store) Delete(ctx context.Context, owner, name string) (deleted bool, err error) {
	res, err := s.db.ExecContext(ctx,
		`DELETE FROM webhooks WHERE owner_email = ? AND name = ?`, owner, name)
	if err != nil {
		return false, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return false, err
	}
	return n > 0, nil
}

// UpdateSecret rotates the fingerprint and, for github-hmac, retained key.
// The update is owner-scoped; updated reports whether a row matched.
func (s *Store) UpdateSecret(ctx context.Context, owner, name, secretHash string, plaintext ...string) (updated bool, err error) {
	var secret any
	if len(plaintext) > 0 {
		secret = plaintext[0]
	}
	res, err := s.db.ExecContext(ctx,
		`UPDATE webhooks SET secret_hash = ?, secret = CASE WHEN verification = 'github-hmac' THEN ? ELSE NULL END WHERE owner_email = ? AND name = ?`,
		secretHash, secret, owner, name)
	if err != nil {
		return false, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return false, err
	}
	return n > 0, nil
}

// TouchLastTriggered stamps last_triggered_at within an existing transaction so a
// trigger can record the touch atomically with its outbox append (D4/D5).
func (s *Store) TouchLastTriggered(tx *sql.Tx, name string, at time.Time) error {
	_, err := tx.Exec(
		`UPDATE webhooks SET last_triggered_at = ? WHERE name = ?`,
		at.UTC().Format(timeFormat), name)
	return err
}

// rowScanner is satisfied by both *sql.Row and *sql.Rows.
type rowScanner interface {
	Scan(dest ...any) error
}

// scanWebhook decodes one webhooks row, parsing the TEXT timestamps back into
// time.Time and leaving LastTriggeredAt nil when the column is NULL.
func scanWebhook(sc rowScanner) (Webhook, string, string, error) {
	var (
		w             Webhook
		secretHash    string
		createdAt     string
		lastTriggered sql.NullString
		secret        sql.NullString
	)
	if err := sc.Scan(&w.Name, &w.OwnerEmail, &secretHash, &createdAt, &lastTriggered, &w.Verification, &secret); err != nil {
		return Webhook{}, "", "", err
	}
	t, err := time.Parse(timeFormat, createdAt)
	if err != nil {
		return Webhook{}, "", "", err
	}
	w.CreatedAt = t
	if lastTriggered.Valid {
		lt, err := time.Parse(timeFormat, lastTriggered.String)
		if err != nil {
			return Webhook{}, "", "", err
		}
		w.LastTriggeredAt = &lt
	}
	return w, secretHash, secret.String, nil
}
