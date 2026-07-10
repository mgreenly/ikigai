package dropbox

import (
	"bytes"
	"context"
	"database/sql"
	"errors"
	"fmt"
	"log/slog"
	"time"
)

// sync.go is the sync engine (PLAN.md §2, the heart): a single background
// goroutine running the longpoll → continue → apply loop. Its lifecycle is
// modeled on agent/internal/runner — started on boot, honoring context
// cancellation for a clean shutdown.
//
// The engine owns the cursor advance (per continue-page), the created/modified/
// deleted decision (incl. folder-delete subtree fan-out and absent-path
// no-op-but-unlink), and the poison-entry bound. The Service (service.go) owns
// the transaction that commits {files index change + outbox event} atomically;
// the engine calls the Service's apply helpers.

// dropboxAPI is the slice of the Dropbox client the engine needs. It is an
// interface so a unit test can inject a fake returning canned deltas (with a
// real temp store/mirror) and exercise the load-bearing rules deterministically
// without live Dropbox (PLAN.md §10). *Client satisfies it.
type dropboxAPI interface {
	ListFolder(ctx context.Context) (ListResult, error)
	ListFolderContinue(ctx context.Context, cursor string) (ListResult, error)
	Longpoll(ctx context.Context, cursor string) (LongpollResult, error)
	Download(ctx context.Context, path, rev string) ([]byte, FileMeta, error)
}

// Engine drives the sync loop. It composes the injected client with the
// Service's store/mirror/apply-helpers.
type Engine struct {
	svc    *Service
	client dropboxAPI
	log    *slog.Logger

	// maxEntryRetries is the poison-entry bound (DROPBOX_MAX_ENTRY_RETRIES,
	// default 5): after this many failed passes on a single entry the engine marks
	// the row errored and advances past it so one bad entry cannot wedge all sync
	// (PLAN.md §2). retries is keyed by folded path across continue pages within a
	// single drain.
	maxEntryRetries int

	// backoff is the wait after a transient longpoll/continue failure (network/
	// 5xx). Small in tests via the constructor.
	backoff time.Duration
}

// EngineOptions configures NewEngine.
type EngineOptions struct {
	Client          dropboxAPI
	Logger          *slog.Logger
	MaxEntryRetries int
	Backoff         time.Duration
}

const defaultMaxEntryRetries = 5

// NewEngine builds an Engine over svc with the given client. A zero
// MaxEntryRetries defaults to 5 (PLAN.md §2 poison bound).
func NewEngine(svc *Service, opts EngineOptions) *Engine {
	log := opts.Logger
	if log == nil {
		log = slog.Default()
	}
	mer := opts.MaxEntryRetries
	if mer <= 0 {
		mer = defaultMaxEntryRetries
	}
	bo := opts.Backoff
	if bo <= 0 {
		bo = 5 * time.Second
	}
	return &Engine{svc: svc, client: opts.Client, log: log, maxEntryRetries: mer, backoff: bo}
}

// Run blocks running the sync loop until ctx is cancelled, then returns nil.
// main launches it on a goroutine with the server's lifecycle context. It first
// bootstraps (enumerate-or-resume), then enters the steady-state longpoll loop.
func (e *Engine) Run(ctx context.Context) error {
	e.log.Info("dropbox sync engine starting")
	if err := e.bootstrap(ctx); err != nil {
		if ctx.Err() != nil {
			return nil
		}
		// A bootstrap failure (e.g. network) is logged; the steady loop will retry
		// from the (possibly still-absent) cursor on the next pass.
		e.log.Error("dropbox sync bootstrap failed", "err", err.Error())
	}
	e.steadyState(ctx)
	e.log.Info("dropbox sync engine stopped")
	return nil
}

// bootstrap performs first-boot enumeration when no cursor is persisted (PLAN.md
// §2): ListFolder (recursive) then drain ListFolderContinue while HasMore,
// applying every entry and persisting the cursor per page. On first boot every
// existing file emits file.created (no silent baseline, §5). When a cursor is
// already persisted, bootstrap is a no-op — the steady loop resumes from it.
func (e *Engine) bootstrap(ctx context.Context) error {
	cursor, ok, err := e.readCursor(ctx)
	if err != nil {
		return err
	}
	if ok && cursor != "" {
		e.log.Info("dropbox sync resuming from persisted cursor")
		return nil
	}
	e.log.Info("dropbox sync first boot: enumerating app folder")
	res, err := e.client.ListFolder(ctx)
	if err != nil {
		return fmt.Errorf("list_folder: %w", err)
	}
	retries := map[string]int{}
	if err := e.applyPage(ctx, res, retries); err != nil {
		return err
	}
	for res.HasMore {
		if ctx.Err() != nil {
			return ctx.Err()
		}
		res, err = e.client.ListFolderContinue(ctx, res.Cursor)
		if err != nil {
			return fmt.Errorf("list_folder/continue: %w", err)
		}
		if err := e.applyPage(ctx, res, retries); err != nil {
			return err
		}
	}
	e.log.Info("dropbox sync first-boot enumeration complete")
	return nil
}

// steadyState runs the longpoll → continue → apply loop until ctx is cancelled
// (PLAN.md §2). Longpoll parks on the cursor; on changes it drains continue
// pages, persisting the cursor per page.
func (e *Engine) steadyState(ctx context.Context) {
	for {
		if ctx.Err() != nil {
			return
		}
		cursor, ok, err := e.readCursor(ctx)
		if err != nil {
			e.log.Warn("dropbox sync read cursor failed", "err", err.Error())
			if !e.sleep(ctx, e.backoff) {
				return
			}
			continue
		}
		if !ok || cursor == "" {
			// No cursor yet (bootstrap failed earlier). Retry the bootstrap.
			if err := e.bootstrap(ctx); err != nil {
				if ctx.Err() != nil {
					return
				}
				e.log.Warn("dropbox sync bootstrap retry failed", "err", err.Error())
				if !e.sleep(ctx, e.backoff) {
					return
				}
			}
			continue
		}

		lr, err := e.client.Longpoll(ctx, cursor)
		if err != nil {
			if ctx.Err() != nil {
				return
			}
			e.log.Warn("dropbox sync longpoll failed", "err", err.Error())
			if !e.sleep(ctx, e.backoff) {
				return
			}
			continue
		}
		if lr.Backoff > 0 {
			if !e.sleep(ctx, time.Duration(lr.Backoff)*time.Second) {
				return
			}
		}
		if !lr.Changes {
			continue
		}
		if err := e.drain(ctx, cursor); err != nil {
			if ctx.Err() != nil {
				return
			}
			e.log.Warn("dropbox sync drain failed", "err", err.Error())
			if !e.sleep(ctx, e.backoff) {
				return
			}
		}
	}
}

// drain loops ListFolderContinue from cursor while HasMore, applying each page
// and persisting the cursor PER PAGE (PLAN.md §2/§3: per-continue-page advance,
// so a crash mid-drain resumes from the last applied page). retries spans the
// whole drain so a poison entry's count survives across pages.
func (e *Engine) drain(ctx context.Context, cursor string) error {
	retries := map[string]int{}
	for {
		if ctx.Err() != nil {
			return ctx.Err()
		}
		res, err := e.client.ListFolderContinue(ctx, cursor)
		if err != nil {
			return fmt.Errorf("list_folder/continue: %w", err)
		}
		if err := e.applyPage(ctx, res, retries); err != nil {
			return err
		}
		cursor = res.Cursor
		if !res.HasMore {
			return nil
		}
	}
}

// applyPage applies every entry on a page, then persists the page's cursor in
// its own tx AFTER all entries are applied (PLAN.md §2 per-page advance). A
// single entry that keeps failing is bounded by maxEntryRetries: after the bound
// the row is marked errored and the entry is skipped so the rest of the feed
// keeps flowing (poison-entry bound, §2).
func (e *Engine) applyPage(ctx context.Context, res ListResult, retries map[string]int) error {
	for _, entry := range res.Entries {
		if ctx.Err() != nil {
			return ctx.Err()
		}
		if err := e.applyEntry(ctx, entry); err != nil {
			key := foldPath(entry.PathDisplay)
			retries[key]++
			n := retries[key]
			if n < e.maxEntryRetries {
				// Don't advance the cursor: return the error so the whole page
				// (and cursor) replays. The other already-applied entries dedup by
				// rev/absent-path on replay.
				return fmt.Errorf("apply entry %q (attempt %d/%d): %w", entry.PathDisplay, n, e.maxEntryRetries, err)
			}
			// Poison bound reached: mark the row errored and advance past it.
			e.log.Error("dropbox sync poison entry: advancing past failed entry",
				"path", entry.PathDisplay, "attempts", n, "err", err.Error())
			if merr := e.markError(ctx, entry.PathDisplay, err.Error()); merr != nil {
				e.log.Warn("dropbox sync mark-error failed", "path", entry.PathDisplay, "err", merr.Error())
			}
			delete(retries, key)
			continue
		}
		delete(retries, foldPath(entry.PathDisplay))
	}
	// Persist the cursor only after every entry on this page is applied.
	return e.setCursor(ctx, res.Cursor)
}

// applyEntry routes one delta entry to the right apply action (PLAN.md §2):
//
//	deleted → subtree fan-out (folder) / single delete / absent-path no-op
//	folder  → structural mkdir on the mirror (no event)
//	file    → download + hash-verify + write, then created|modified (rev-dedup),
//	          or a case-only rename → on-disk rename + file.modified
func (e *Engine) applyEntry(ctx context.Context, entry DeltaEntry) error {
	switch entry.Tag {
	case TagDeleted:
		_, err := e.svc.applyDelete(ctx, entry.PathDisplay)
		return err
	case TagFolder:
		// Structural only — mkdir, no event (PLAN.md §5).
		return e.svc.Mirror.Mkdir(entry.PathDisplay)
	case TagFile:
		return e.applyFile(ctx, entry)
	default:
		// Unknown tag: skip rather than wedge (forward-compat).
		e.log.Warn("dropbox sync unknown entry tag", "tag", entry.Tag, "path", entry.PathDisplay)
		return nil
	}
}

// applyFile applies a file delta (PLAN.md §2/§5/§6 create/modify rules):
//
//   - Look the path up case-insensitively. If a row exists whose rev already
//     matches, it's a no-op (rev dedup — no re-download, no event).
//   - If a row exists whose path_lower matches but the DISPLAY path differs AND
//     the rev is unchanged, it's a CASE-ONLY RENAME: on-disk rename +
//     file.modified (never delete+create) (§2 case-only rename).
//   - Otherwise download the bytes (client hash-verifies vs content_hash), write
//     atomically to the mirror, then commit {index upsert + event}: file.created
//     if the path was absent, file.modified if it existed with a different rev.
func (e *Engine) applyFile(ctx context.Context, entry DeltaEntry) error {
	existing, err := e.getFile(ctx, entry.PathDisplay)
	hadRow := err == nil
	if err != nil && !errors.Is(err, ErrNotFound) {
		return err
	}

	if hadRow {
		// Rev dedup: identical rev AND identical display path → nothing changed.
		if existing.Rev == entry.Rev && existing.Path == entry.PathDisplay {
			return nil
		}
		// Case-only rename: same rev, display path differs (path_lower matched).
		if existing.Rev == entry.Rev && existing.Path != entry.PathDisplay {
			if err := e.svc.Mirror.Rename(existing.Path, entry.PathDisplay); err != nil {
				return err
			}
			return e.svc.applyRename(ctx, existing.Path, entry.PathDisplay,
				entry.Rev, entry.ContentHash, int64(entry.Size))
		}
	}

	// Download + hash-verify (the client verifies vs content_hash), then atomic
	// write, then commit {index + event}. Write BEFORE commit so a crash before
	// commit replays as an idempotent re-download (PLAN.md §6).
	data, meta, err := e.client.Download(ctx, entry.PathDisplay, entry.Rev)
	if err != nil {
		return err
	}
	if _, _, err := e.svc.Mirror.WriteFrom(entry.PathDisplay, bytes.NewReader(data)); err != nil {
		return err
	}
	rev := meta.Rev
	if rev == "" {
		rev = entry.Rev
	}
	hash := meta.ContentHash
	if hash == "" {
		hash = entry.ContentHash
	}
	size := int64(meta.Size)
	if meta.Size == 0 && entry.Size != 0 {
		size = int64(entry.Size)
	}
	_, err = e.svc.applyUpsert(ctx, entry.PathDisplay, rev, hash, size, !hadRow)
	return err
}

// ── small tx-scoped store helpers (engine-owned reads/cursor writes) ─────────

func (e *Engine) readCursor(ctx context.Context) (string, bool, error) {
	tx, err := e.svc.DB.BeginTx(ctx, &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return "", false, err
	}
	defer tx.Rollback()
	return e.svc.Store.GetCursor(tx)
}

func (e *Engine) setCursor(ctx context.Context, cursor string) error {
	return e.svc.inTx(ctx, func(tx *sql.Tx) error {
		return e.svc.Store.SetCursor(tx, cursor, e.svc.now())
	})
}

func (e *Engine) getFile(ctx context.Context, path string) (FileRow, error) {
	tx, err := e.svc.DB.BeginTx(ctx, &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return FileRow{}, err
	}
	defer tx.Rollback()
	return e.svc.Store.GetFile(tx, path)
}

func (e *Engine) markError(ctx context.Context, path, errText string) error {
	return e.svc.inTx(ctx, func(tx *sql.Tx) error {
		if err := e.svc.Store.MarkError(tx, path, errText); err != nil {
			// The row may not exist (e.g. a create that never indexed). Insert a
			// placeholder errored row so health surfaces it.
			if errors.Is(err, ErrNotFound) {
				if uerr := e.svc.Store.UpsertFile(tx, path, "", "", 0, e.svc.now()); uerr != nil {
					return uerr
				}
				return e.svc.Store.MarkError(tx, path, errText)
			}
			return err
		}
		return nil
	})
}

// sleep waits d or returns false if ctx is cancelled first.
func (e *Engine) sleep(ctx context.Context, d time.Duration) bool {
	t := time.NewTimer(d)
	defer t.Stop()
	select {
	case <-ctx.Done():
		return false
	case <-t.C:
		return true
	}
}
