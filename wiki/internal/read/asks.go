package read

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/base32"
	"encoding/json"
	"fmt"
	"time"
)

// asks status values (design §9.2, mirroring runs where honest).
const (
	StatusRunning   = "running"
	StatusSucceeded = "succeeded"
	StatusFailed    = "failed"
	StatusCrashed   = "crashed"
)

// AskStore owns the asks accounting table (design §9.2) — deliberately separate
// from runs (ask has no causing row, no in-flight claim, no retries). Lifecycle:
// insert `running`, finalize at end (succeeded/failed), and a boot sweep marks
// orphaned `running` rows `crashed`. Storing `question` (and the answer +
// citations in usage) is the free real-data golden source the eval harness names
// (eval obligation 4).
type AskStore struct {
	db  *sql.DB
	now func() time.Time
}

// NewAskStore wraps a migrated *sql.DB. now defaults to time.Now.
func NewAskStore(db *sql.DB) *AskStore {
	return &AskStore{db: db, now: time.Now}
}

// Begin inserts a new asks row in `running` and returns its id (design §9.2). The
// owner is attribution (the authenticated caller); the question is stored for the
// eval goldens.
func (s *AskStore) Begin(ctx context.Context, owner, question string) (string, error) {
	id := newULID()
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO asks (id, owner, question, status, started_at) VALUES (?, ?, ?, ?, ?)`,
		id, owner, question, StatusRunning, s.now().UnixMilli())
	if err != nil {
		return "", fmt.Errorf("read: begin ask: %w", err)
	}
	return id, nil
}

// Finish finalizes an asks row at the end of the call (design §9.2): succeeded
// with the answer + citations captured into usage (golden gold), or failed with
// the error. A finished_at is always stamped.
func (s *AskStore) Finish(ctx context.Context, id string, ans *Answer, callErr error) error {
	status := StatusSucceeded
	errStr := ""
	var usage any
	if callErr != nil {
		status = StatusFailed
		errStr = callErr.Error()
	} else if ans != nil {
		usage = map[string]any{
			"answer":    ans.Answer,
			"citations": ans.Citations,
			"sources":   ans.Sources,
			"found":     ans.Found,
		}
	}
	var usageJSON sql.NullString
	if usage != nil {
		if b, err := json.Marshal(usage); err == nil {
			usageJSON = sql.NullString{String: string(b), Valid: true}
		}
	}
	_, err := s.db.ExecContext(ctx,
		`UPDATE asks SET status=?, finished_at=?, usage=?, error=? WHERE id=?`,
		status, s.now().UnixMilli(), usageJSON, errStr, id)
	if err != nil {
		return fmt.Errorf("read: finish ask: %w", err)
	}
	return nil
}

// SweepOrphans flips every asks row still `running` to `crashed` (design §9.2 boot
// sweep): a synchronous ask that was in flight when the process died left a
// `running` row no retry will ever finish. Returns the number swept.
func (s *AskStore) SweepOrphans(ctx context.Context) (int, error) {
	res, err := s.db.ExecContext(ctx,
		`UPDATE asks SET status=?, finished_at=? WHERE status=?`,
		StatusCrashed, s.now().UnixMilli(), StatusRunning)
	if err != nil {
		return 0, fmt.Errorf("read: sweep orphan asks: %w", err)
	}
	n, _ := res.RowsAffected()
	return int(n), nil
}

var ulidEnc = base32.StdEncoding.WithPadding(base32.NoPadding)

// newULID returns the suite's standard 26-char time-ordered id for an asks row.
func newULID() string {
	var b [16]byte
	now := uint64(time.Now().UnixMilli())
	b[0] = byte(now >> 40)
	b[1] = byte(now >> 32)
	b[2] = byte(now >> 24)
	b[3] = byte(now >> 16)
	b[4] = byte(now >> 8)
	b[5] = byte(now)
	if _, err := rand.Read(b[6:]); err != nil {
		panic("crypto/rand failed: " + err.Error())
	}
	return ulidEnc.EncodeToString(b[:])
}
