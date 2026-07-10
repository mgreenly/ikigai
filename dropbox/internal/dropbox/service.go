package dropbox

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"
)

// Service is the dropbox domain seam (PLAN.md §8): it holds the SQL store, the
// local mirror, the Dropbox client, an EventSink, and the *sql.DB. It owns the
// TRANSACTION BOUNDARY — any change to a file's mirror state is committed as ONE
// transaction containing BOTH the files-index change AND the outbox event append
// (mirroring ledger's persist→Append→Ring seam: Append runs INSIDE the tx, Ring
// fires AFTER commit, so an event is emitted iff the mirror state changed).
//
// The Service is NOT a verb dispatcher; the MCP layer (internal/mcp/tools.go) is
// the sole dispatcher. The sync engine (sync.go) is the sole caller of the apply
// helpers below.
type Service struct {
	DB     *sql.DB
	Store  *Store
	Mirror *Mirror
	Client *Client
	// Outbox, when set, makes the service an event-plane producer. Nil disables
	// emission (unit tests run the engine with a recording fake or nil). Held as
	// an interface so the domain does not hard-depend on the event-plane library
	// when emission is off.
	Outbox EventSink
	Now    func() time.Time

	// contentBase is the scheme+host(+port) the loopback /content route lives at,
	// used only by Whoami/Health-adjacent code if ever needed; the event builders
	// get it through the outboxProducer. Kept for symmetry / future use.
	contentBase string
}

// NewService builds a Service over db with the real clock. The store, mirror,
// client, outbox, and contentBase are wired in by main (or by a test). A bare
// NewService (store only) keeps the Phase 0 identity probes working.
func NewService(db *sql.DB) *Service {
	return &Service{DB: db, Store: NewStore(), Now: time.Now}
}

// ── identity probes (MCP) ─────────────────────────────────────────────────

// Whoami returns the authenticated caller's identity verbatim. dropbox is a
// single-box, single-owner service: identity is consulted only by the two MCP
// tools, never to scope domain data (PLAN.md §6 — no owner column).
func (s *Service) Whoami(ownerEmail, clientID string) (HealthInfo, error) {
	return HealthInfo{OwnerEmail: ownerEmail, ClientID: clientID}, nil
}

// Health is implemented in health.go (PLAN.md §3 — identity + mirror/disk
// telemetry assembly).

// ── content resolution (Phase 5 wires the HTTP route; logic lives here) ──────

// Content resolves a literal Dropbox path through the index (case-insensitive,
// PLAN.md §2/§4) to the canonical stored display path. When rev is non-nil and does not match the indexed rev it
// returns ErrRevMismatch (the §4 exact-bytes contract → 409). ErrNotFound when
// the path is not in the index (e.g. after a delete → the route 404s).
func (s *Service) Content(path string, rev *string) (FileRow, error) {
	tx, err := s.DB.BeginTx(context.Background(), &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return FileRow{}, fmt.Errorf("content: begin tx: %w", err)
	}
	defer tx.Rollback()
	row, err := s.Store.GetFile(tx, path)
	if err != nil {
		return FileRow{}, err // ErrNotFound passes through
	}
	if rev != nil && *rev != "" && *rev != row.Rev {
		return row, ErrRevMismatch
	}
	return row, nil
}

// List returns index rows for the `list` MCP tool: a path_lower-ordered page of
// the files index, optionally scoped to a folder prefix and paginated by an
// opaque after-cursor. It runs the query on a read-only tx (same pattern as
// Content) and stays deliberately thin — the MCP layer owns limit clamping and
// next_cursor derivation.
//
// path is a display-path prefix; it is folded with foldPath (lowercases only —
// it does NOT normalize slashes), so the caller scopes case-insensitively. Both
// "" and "/" fold to a value the store treats as "no prefix" ("" stays "", and a
// bare "/" never bounds a real subtree), so either means "list everything".
func (s *Service) List(path, after string, limit int) ([]Entry, error) {
	prefix := foldPath(path)
	if prefix == "/" {
		prefix = ""
	}
	tx, err := s.DB.BeginTx(context.Background(), &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return nil, fmt.Errorf("list: begin tx: %w", err)
	}
	defer tx.Rollback()
	return s.Store.ListEntries(tx, prefix, after, limit)
}

// Stat resolves a path to either indexed entry kind.
func (s *Service) Stat(path string) (Entry, error) {
	tx, err := s.DB.BeginTx(context.Background(), &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return Entry{}, fmt.Errorf("stat: begin tx: %w", err)
	}
	defer tx.Rollback()
	if f, err := s.Store.GetFile(tx, path); err == nil {
		return Entry{Path: f.Path, Kind: KindFile, Rev: f.Rev, ContentHash: f.ContentHash, Size: f.Size, UpdatedAt: f.UpdatedAt, PathLower: f.PathLower}, nil
	} else if !errors.Is(err, ErrNotFound) {
		return Entry{}, err
	}
	d, err := s.Store.GetDir(tx, path)
	if err != nil {
		return Entry{}, err
	}
	return Entry{Path: d.Path, Kind: KindDir, UpdatedAt: d.UpdatedAt, PathLower: d.PathLower}, nil
}

func (s *Service) upsertDirParents(tx *sql.Tx, path string) error {
	parts := strings.Split(strings.Trim(path, "/"), "/")
	if len(parts) < 2 {
		return nil
	}
	for i := 1; i < len(parts); i++ {
		if err := s.Store.UpsertDir(tx, "/"+strings.Join(parts[:i], "/")); err != nil {
			return err
		}
	}
	return nil
}

// ── apply helpers (the engine's tx boundary) ────────────────────────────────
//
// These are the ONLY mutators of mirror state. Each owns the crash-ordering
// invariant (PLAN.md §6) for its direction; the engine (sync.go) calls them and
// owns the cursor advance + poison bound. Each rings the feed AFTER commit.

// applyUpsert applies a create/modify for one file (PLAN.md §6 create/modify
// ordering): the bytes are ALREADY written to the mirror by the caller (atomic,
// hash-verified) before this runs; here we commit {index upsert + event} on one
// tx, then Ring after commit. created is true when the path was absent from the
// index → file.created, else file.modified. Dedup by rev is the caller's job
// (a matching rev never reaches here). Returns the event type emitted.
func (s *Service) applyUpsert(ctx context.Context, path, rev, contentHash string, size int64, created bool) (string, error) {
	now := s.now()
	evType := EventFileModified
	if created {
		evType = EventFileCreated
	}
	ev := FileEvent{
		Type:        evType,
		Path:        path,
		Rev:         rev,
		ContentHash: contentHash,
		Size:        size,
		OccurredAt:  now,
	}
	err := s.inTx(ctx, func(tx *sql.Tx) error {
		if err := s.upsertDirParents(tx, path); err != nil {
			return err
		}
		if err := s.Store.UpsertFile(tx, path, rev, contentHash, size, now); err != nil {
			return err
		}
		return s.appendEvent(tx, ev)
	})
	if err != nil {
		return "", err
	}
	s.ring()
	return evType, nil
}

// applyMkdir indexes a structural directory after it exists on disk. It has no
// file lifecycle event and intentionally does not ring the event feed.
func (s *Service) applyMkdir(ctx context.Context, path string) error {
	if err := s.Mirror.Mkdir(path); err != nil {
		return err
	}
	return s.inTx(ctx, func(tx *sql.Tx) error {
		if err := s.upsertDirParents(tx, path); err != nil {
			return err
		}
		return s.Store.UpsertDir(tx, path)
	})
}

// applyRename applies a case-only rename (PLAN.md §2/§6): the on-disk file has
// ALREADY been renamed by the caller (mirror.Rename) before this runs. Here we
// commit {index upsert to the new display path + file.modified event} on one tx,
// then Ring. The old row is matched case-foldedly by UpsertFile's ON CONFLICT on
// the (unchanged) path_lower, so the display path is updated in place.
func (s *Service) applyRename(ctx context.Context, oldDisplay, newDisplay, rev, contentHash string, size int64) error {
	now := s.now()
	ev := FileEvent{
		Type:        EventFileModified,
		Path:        newDisplay,
		Rev:         rev,
		ContentHash: contentHash,
		Size:        size,
		OccurredAt:  now,
	}
	err := s.inTx(ctx, func(tx *sql.Tx) error {
		// Delete the old display-path row then insert the new one: a case-only
		// change keeps the same path_lower, and UpsertFile keys on the display
		// `path` PRIMARY KEY, so a plain upsert of the new display path would
		// leave the old display row behind. Remove it first (folded match).
		if err := s.Store.DeleteFile(tx, oldDisplay); err != nil && !errors.Is(err, ErrNotFound) {
			return err
		}
		if err := s.Store.UpsertFile(tx, newDisplay, rev, contentHash, size, now); err != nil {
			return err
		}
		return s.appendEvent(tx, ev)
	})
	if err != nil {
		return err
	}
	s.ring()
	return nil
}

// applyDelete applies a delete for one path or a whole subtree (PLAN.md §1/§2/§6
// — the load-bearing delete rules):
//
//   - A folder delete arrives as ONE entry; DeleteSubtree fans it out over the
//     index, returning every row at/under the prefix. We commit {all row-deletes
//   - one file.deleted event per row} on ONE tx, THEN unlink each mirror file
//     AFTER commit (delete crash-ordering: DB first, disk second).
//   - A delete delta on an ALREADY-ABSENT path (no rows under the prefix) is an
//     idempotent unlink that EMITS NOTHING — closing the crash-replay window:
//     replaying a delete whose row is already gone produces zero new events and
//     still removes any orphan file on disk.
//
// The last-known rev/content_hash/size for each event are read from the row
// BEFORE the in-tx delete removes it (DeleteSubtree returns the deleted rows).
func (s *Service) applyDelete(ctx context.Context, path string) (emitted int, err error) {
	var deleted []FileRow
	now := s.now()
	err = s.inTx(ctx, func(tx *sql.Tx) error {
		rows, derr := s.Store.ListFiles(tx, foldPath(path), "", int(^uint(0)>>1))
		if derr != nil {
			return derr
		}
		filePaths, _, derr := s.Store.DeleteDirSubtree(tx, path)
		if derr != nil {
			return derr
		}
		if len(filePaths) != len(rows) {
			return fmt.Errorf("delete directory subtree changed during transaction")
		}
		deleted = rows
		for _, r := range deleted {
			ev := FileEvent{
				Type:        EventFileDeleted,
				Path:        r.Path,
				Rev:         r.Rev,
				ContentHash: r.ContentHash,
				Size:        r.Size,
				OccurredAt:  now,
			}
			if err := s.appendEvent(tx, ev); err != nil {
				return err
			}
		}
		return nil
	})
	if err != nil {
		return 0, err
	}
	// AFTER commit: unlink each mirror file (idempotent). For an already-absent
	// path `deleted` is empty → nothing emitted; we still unlink the raw path to
	// remove any orphan from the crash window (PLAN.md §6).
	if err := s.Mirror.RemoveTree(path); err != nil {
		return 0, err
	}
	if len(deleted) > 0 {
		s.ring()
	}
	return len(deleted), nil
}

// ── tx / emission plumbing ──────────────────────────────────────────────────

// inTx runs fn inside a write transaction, committing on success and rolling
// back on error. The single transaction boundary for every mirror-state change.
func (s *Service) inTx(ctx context.Context, fn func(*sql.Tx) error) error {
	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("begin tx: %w", err)
	}
	if err := fn(tx); err != nil {
		_ = tx.Rollback()
		return err
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("commit: %w", err)
	}
	return nil
}

// appendEvent appends one event on tx when an EventSink is wired (no-op when
// emission is disabled).
func (s *Service) appendEvent(tx *sql.Tx, ev FileEvent) error {
	if s.Outbox == nil {
		return nil
	}
	return s.Outbox.AppendFileEvent(tx, ev)
}

// ring wakes parked feed connections after a successful commit (no-op when the
// outbox is not wired).
func (s *Service) ring() {
	if s.Outbox != nil {
		s.Outbox.Ring()
	}
}

// now renders the current time in the event timestamp format.
func (s *Service) now() string {
	clk := s.Now
	if clk == nil {
		clk = time.Now
	}
	return clk().UTC().Format(eventTimeFormat)
}
