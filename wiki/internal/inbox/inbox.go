// Package inbox is wiki's whole write path: the single Accept function every
// front door calls, the content-addressed payload store, and the size cap at the
// door. Per design §2.1/§2.2 it is the ONLY code that writes the inbox table —
// hash, size, decide inline-vs-spill, insert the row, nudge the workers. Accept
// is synchronous, transactional, and NEVER calls an LLM.
//
// Storage rule (§2.2): payloads of len(bytes) <= InlineMax go in the row's
// content column; larger ones spill to a content-addressed blob at
// blobs/<aa>/<sha256> (2-hex fan-out) with the row holding only the reference.
// Three invariants keep the threshold tunable with no migration: sha256 is
// computed and stored ALWAYS (both paths); the row records which path was taken
// (the blob flag) and readers dispatch on the row, never the current threshold;
// ReadPayload is the only accessor that knows two paths exist. Blob write order
// is write → fsync → insert row, so an orphan blob is legal (and sweepable) but a
// row pointing at a missing blob is a bug.
//
// Size cap (§2.2): Accept refuses input over MaxBytes loudly at the door with
// ErrTooLarge; there is no ingest-side chunking. The refusal is pre-accept, so
// the caller (a front door) emits wiki.ingest_refused itself — Accept never
// writes the outbox (it owns the inbox row, not the event plane).
package inbox

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"time"
)

// ErrTooLarge is returned by Accept when len(bytes) exceeds the configured
// MaxBytes door cap. It is a loud, pre-accept refusal — the row is never written
// — so the front door can map it to a wiki.ingest_refused outbox event (§8).
var ErrTooLarge = errors.New("inbox: payload exceeds size cap")

// Kind is the binary classification of what an arrival's content IS (routing),
// not how it arrived (§2.2): text/url/file collapse to KindDocument; domain
// events are KindEvent.
const (
	KindDocument = "document"
	KindEvent    = "event"
)

// Receipt is what Accept returns to a front door (the MCP receipt contract,
// §2.1): the inbox id, the content hash, and whether an identical-bytes arrival
// already existed. It is NOT a job id — integration runs later (§2.1).
type Receipt struct {
	ID     string // the ULID of THIS arrival row
	SHA256 string // content identity, always computed
	Dup    bool   // true if an inbox row with this sha256 already existed
}

// Store is the inbox writer/reader. It owns the inbox table on db and the blob
// directory under root. Constructed once at the composition root with the tuning
// knobs from config; a nil nudge is a no-op (the worker spine that consumes the
// nudge lands in P4).
type Store struct {
	db       *sql.DB
	blobRoot string // the directory that holds blobs/<aa>/<sha256>
	inlineMax int   // WIKI_INBOX_INLINE_MAX: <= goes inline, > spills
	maxBytes  int64 // WIKI_INGEST_MAX_BYTES: > is refused at the door

	newID func() string      // ULID minter (injectable for tests)
	now   func() time.Time   // clock (injectable for tests)
	nudge func()             // worker doorbell; nil = no-op (P4 wires it)
}

// Options configures a Store. db, BlobRoot, InlineMax and MaxBytes are required
// (a zero InlineMax/MaxBytes is a misconfiguration the constructor rejects);
// NewID/Now/Nudge default to production implementations / no-op.
type Options struct {
	DB        *sql.DB
	BlobRoot  string
	InlineMax int
	MaxBytes  int64

	NewID func() string
	Now   func() time.Time
	Nudge func()
}

// New validates options and returns a ready Store. It creates the blob root
// directory (idempotent) so the first spill never races a missing parent.
func New(opts Options) (*Store, error) {
	if opts.DB == nil {
		return nil, errors.New("inbox: DB is required")
	}
	if opts.BlobRoot == "" {
		return nil, errors.New("inbox: BlobRoot is required")
	}
	if opts.InlineMax < 0 {
		return nil, errors.New("inbox: InlineMax must be >= 0")
	}
	if opts.MaxBytes <= 0 {
		return nil, errors.New("inbox: MaxBytes must be > 0")
	}
	if err := os.MkdirAll(opts.BlobRoot, 0o755); err != nil {
		return nil, fmt.Errorf("inbox: create blob root: %w", err)
	}
	s := &Store{
		db:        opts.DB,
		blobRoot:  opts.BlobRoot,
		inlineMax: opts.InlineMax,
		maxBytes:  opts.MaxBytes,
		newID:     opts.NewID,
		now:       opts.Now,
		nudge:     opts.Nudge,
	}
	if s.newID == nil {
		s.newID = newULID
	}
	if s.now == nil {
		s.now = time.Now
	}
	return s, nil
}

// MaxBytes reports the configured door cap so a front door can populate a
// wiki.ingest_refused event's cap field without re-reading config.
func (s *Store) MaxBytes() int64 { return s.maxBytes }

// Accept is the single inbox write function (§2.1). It hashes the bytes, refuses
// oversized input, decides inline-vs-spill, inserts the row, and nudges the
// workers — synchronously, in one transaction, never calling an LLM.
//
// owner is attribution, never isolation (§2.2): a human's X-Owner-Email or the
// system identity stamped by a consumer door. kind is document|event. source is
// one prefixed string (url:… dropbox:… mcp:… crm:… cron:…). tags is a JSON array
// string (caller-supplied, defaulted to "[]" when empty).
//
// Duplicate detection is by sha256 only (§2.2): two arrivals of identical bytes
// are two rows, but the Receipt's Dup flag is true when a prior row already
// carried this hash — the receipt's "already recorded" signal. There is no
// UNIQUE on sha256; the insert always happens.
func (s *Store) Accept(ctx context.Context, owner, kind, source, mime, title, tags string, bytes []byte) (Receipt, error) {
	if int64(len(bytes)) > s.maxBytes {
		return Receipt{}, ErrTooLarge
	}
	sum := sha256.Sum256(bytes)
	hash := hex.EncodeToString(sum[:])

	if tags == "" {
		tags = "[]"
	} else if !json.Valid([]byte(tags)) {
		return Receipt{}, fmt.Errorf("inbox: tags is not valid JSON: %q", tags)
	}

	// Spill BEFORE the row insert (write → fsync → insert) so a row never points
	// at a missing blob; an orphan blob from a failed insert is legal and
	// sweepable (§2.2).
	spill := len(bytes) > s.inlineMax
	if spill {
		if err := s.writeBlob(hash, bytes); err != nil {
			return Receipt{}, err
		}
	}

	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return Receipt{}, fmt.Errorf("inbox: begin: %w", err)
	}
	defer tx.Rollback()

	// Dup is "an identical-bytes arrival already exists" — read before insert.
	var prior int
	if err := tx.QueryRowContext(ctx,
		`SELECT COUNT(1) FROM inbox WHERE sha256 = ?`, hash).Scan(&prior); err != nil {
		return Receipt{}, fmt.Errorf("inbox: dup probe: %w", err)
	}

	id := s.newID()
	var content any // NULL when spilled
	var blobFlag int
	if spill {
		blobFlag = 1
	} else {
		content = bytes
	}
	receivedAt := s.now().UTC().UnixMilli()

	if _, err := tx.ExecContext(ctx,
		`INSERT INTO inbox
		   (id, owner, kind, source, sha256, size, mime, content, blob, title, tags, received_at, integrated_by)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '')`,
		id, owner, kind, source, hash, len(bytes), mime, content, blobFlag, title, tags, receivedAt,
	); err != nil {
		return Receipt{}, fmt.Errorf("inbox: insert: %w", err)
	}
	if err := tx.Commit(); err != nil {
		return Receipt{}, fmt.Errorf("inbox: commit: %w", err)
	}

	// Nudge AFTER commit: the row is durable, so a missed nudge loses nothing (the
	// worker spine re-scans pending rows regardless). nil nudge = no-op (P4 wires
	// the real doorbell).
	if s.nudge != nil {
		s.nudge()
	}
	return Receipt{ID: id, SHA256: hash, Dup: prior > 0}, nil
}

// Row is the subset of an inbox row ReadPayload needs to locate the bytes — the
// path-discriminating fields, never the threshold. Readers fetch this from the
// inbox table and hand it to ReadPayload; they never inspect inlineMax.
type Row struct {
	SHA256  string
	Content []byte // inline bytes (valid only when Blob == 0)
	Blob    bool   // true = bytes live at blobs/<aa>/<sha256>
}

// ReadPayload is the lone accessor that knows two storage paths exist (§2.2). It
// dispatches on the row (the Blob flag), never on the current threshold, so a
// later threshold change cannot orphan an existing row. For an inline row it
// returns the stored content; for a spilled row it reads the content-addressed
// blob.
func (s *Store) ReadPayload(row Row) ([]byte, error) {
	if !row.Blob {
		return row.Content, nil
	}
	b, err := os.ReadFile(s.blobPath(row.SHA256))
	if err != nil {
		return nil, fmt.Errorf("inbox: read blob %s: %w", row.SHA256, err)
	}
	return b, nil
}

// writeBlob writes bytes to the content-addressed path with write → fsync →
// (caller inserts row) ordering. An already-present blob (identical content
// previously spilled) is left as-is — the path is content-addressed, so the
// bytes are guaranteed identical.
func (s *Store) writeBlob(hash string, bytes []byte) error {
	path := s.blobPath(hash)
	if _, err := os.Stat(path); err == nil {
		return nil // content-addressed: identical bytes already on disk
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return fmt.Errorf("inbox: create blob dir: %w", err)
	}
	f, err := os.OpenFile(path+".tmp", os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0o644)
	if err != nil {
		return fmt.Errorf("inbox: open blob tmp: %w", err)
	}
	if _, err := f.Write(bytes); err != nil {
		f.Close()
		return fmt.Errorf("inbox: write blob: %w", err)
	}
	if err := f.Sync(); err != nil {
		f.Close()
		return fmt.Errorf("inbox: fsync blob: %w", err)
	}
	if err := f.Close(); err != nil {
		return fmt.Errorf("inbox: close blob: %w", err)
	}
	if err := os.Rename(path+".tmp", path); err != nil {
		return fmt.Errorf("inbox: commit blob: %w", err)
	}
	return nil
}

// blobPath is the content-addressed location for a hash: blobs/<aa>/<sha256>,
// 2-hex fan-out (WIKI_BLOB_FANOUT = 2, §2.2).
func (s *Store) blobPath(hash string) string {
	return filepath.Join(s.blobRoot, "blobs", hash[:2], hash)
}
