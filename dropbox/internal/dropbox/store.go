package dropbox

import (
	"database/sql"
	"errors"
	"fmt"
	"strings"
)

// Store is the SQL-only data layer (PLAN.md §8). Every method takes *sql.Tx so
// the Service composes the {files index change + outbox event} transaction
// atomically — no method opens its own DB handle or commits. The chassis db
// package enforces single-writer/WAL on the connection (internal/db/db.go).
//
// Case folding (PLAN.md §2): Dropbox paths are case-insensitive and
// case-preserving while the mirror FS is case-sensitive. The `path` column
// stores the verbatim display path (for serving and eventing); `path_lower`
// holds the folded form and is the sole key for lookups and prefix matches.
// Callers pass display paths throughout; the Store folds them with foldPath.
type Store struct{}

// NewStore builds a Store.
func NewStore() *Store { return &Store{} }

// foldPath returns the case-folded form of a Dropbox display path, the value
// stored in and matched against the path_lower column. Folding lives in one
// place so writes and lookups agree.
func foldPath(displayPath string) string {
	return strings.ToLower(displayPath)
}

// FileRow is one row of the files index (the per-path mirror index). Path is the
// verbatim display path; the engine reads Rev/ContentHash/Size to decide
// created-vs-modified and to populate the file.deleted payload's last-known
// fields before the in-tx delete removes the row (PLAN.md §5).
type FileRow struct {
	Path        string
	Rev         string
	ContentHash string
	Size        int64
	UpdatedAt   string
	PathLower   string
	Error       sql.NullString
}

// DirRow is one row of the first-class directory index.
type DirRow struct {
	Path      string
	PathLower string
	UpdatedAt string
}

// ── sync_state (singleton cursor) ───────────────────────────────────────────

// GetCursor returns the persisted Dropbox list_folder cursor and whether a
// cursor row exists. On first boot the row is absent (ok == false): the engine
// enumerates from scratch and persists the cursor via SetCursor.
func (Store) GetCursor(tx *sql.Tx) (cursor string, ok bool, err error) {
	row := tx.QueryRow(`SELECT cursor FROM sync_state WHERE id = 1`)
	var c sql.NullString
	switch err := row.Scan(&c); {
	case errors.Is(err, sql.ErrNoRows):
		return "", false, nil
	case err != nil:
		return "", false, fmt.Errorf("get cursor: %w", err)
	}
	if !c.Valid {
		return "", false, nil
	}
	return c.String, true, nil
}

// SetCursor upserts the singleton cursor row (CHECK(id=1)). Called per
// continue-page after that page's entries are applied (PLAN.md §2).
func (Store) SetCursor(tx *sql.Tx, cursor, updatedAt string) error {
	_, err := tx.Exec(`
		INSERT INTO sync_state (id, cursor, updated_at) VALUES (1, ?, ?)
		ON CONFLICT(id) DO UPDATE SET cursor = excluded.cursor, updated_at = excluded.updated_at
	`, cursor, updatedAt)
	if err != nil {
		return fmt.Errorf("set cursor: %w", err)
	}
	return nil
}

// ── files index ─────────────────────────────────────────────────────────────

// UpsertFile inserts or replaces the index row for a display path. path_lower is
// derived from the display path so all lookups/prefix-matches stay consistent.
// An upsert clears any prior poison-entry error: a successful (re)index means the
// path is no longer failed.
func (Store) UpsertFile(tx *sql.Tx, path, rev, contentHash string, size int64, updatedAt string) error {
	var dir string
	err := tx.QueryRow(`SELECT path FROM directories WHERE path_lower = ?`, foldPath(path)).Scan(&dir)
	if err == nil {
		return fmt.Errorf("upsert file %q conflicts with directory %q: %w", path, dir, ErrValidation)
	}
	if !errors.Is(err, sql.ErrNoRows) {
		return fmt.Errorf("check file directory collision: %w", err)
	}
	_, err = tx.Exec(`
		INSERT INTO files (path, rev, content_hash, size, updated_at, path_lower, error)
		VALUES (?, ?, ?, ?, ?, ?, NULL)
		ON CONFLICT(path) DO UPDATE SET
			rev          = excluded.rev,
			content_hash = excluded.content_hash,
			size         = excluded.size,
			updated_at   = excluded.updated_at,
			path_lower   = excluded.path_lower,
			error        = NULL
	`, path, rev, contentHash, size, updatedAt, foldPath(path))
	if err != nil {
		return fmt.Errorf("upsert file: %w", err)
	}
	return nil
}

// UpsertDir records a directory by its display path. Files and directories
// share Dropbox's case-insensitive namespace, so a folded collision is rejected.
func (Store) UpsertDir(tx *sql.Tx, path string) error {
	var file string
	err := tx.QueryRow(`SELECT path FROM files WHERE path_lower = ?`, foldPath(path)).Scan(&file)
	if err == nil {
		return fmt.Errorf("upsert directory %q conflicts with file %q: %w", path, file, ErrValidation)
	}
	if !errors.Is(err, sql.ErrNoRows) {
		return fmt.Errorf("check directory file collision: %w", err)
	}
	_, err = tx.Exec(`
		INSERT INTO directories (path, path_lower, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP)
		ON CONFLICT(path_lower) DO UPDATE SET path = excluded.path, updated_at = excluded.updated_at
	`, path, foldPath(path))
	if err != nil {
		return fmt.Errorf("upsert directory: %w", err)
	}
	return nil
}

// GetDir resolves a directory by its case-folded display path.
func (Store) GetDir(tx *sql.Tx, displayPath string) (DirRow, error) {
	var row DirRow
	err := tx.QueryRow(`SELECT path, path_lower, updated_at FROM directories WHERE path_lower = ?`, foldPath(displayPath)).Scan(&row.Path, &row.PathLower, &row.UpdatedAt)
	if errors.Is(err, sql.ErrNoRows) {
		return DirRow{}, ErrNotFound
	}
	if err != nil {
		return DirRow{}, fmt.Errorf("get directory: %w", err)
	}
	return row, nil
}

// DeleteDirSubtree removes both index kinds under a directory path using the
// same folded, slash-boundary prefix semantics as DeleteSubtree.
func (Store) DeleteDirSubtree(tx *sql.Tx, path string) (files, dirs []string, err error) {
	prefix := foldPath(path)
	like := escapeLike(prefix) + "/%"
	fileRows, err := tx.Query(`SELECT path FROM files WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\' ORDER BY path_lower`, prefix, like)
	if err != nil {
		return nil, nil, fmt.Errorf("select directory subtree files: %w", err)
	}
	for fileRows.Next() {
		var p string
		if err := fileRows.Scan(&p); err != nil {
			fileRows.Close()
			return nil, nil, err
		}
		files = append(files, p)
	}
	if err := fileRows.Close(); err != nil {
		return nil, nil, err
	}
	dirRows, err := tx.Query(`SELECT path FROM directories WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\' ORDER BY path_lower`, prefix, like)
	if err != nil {
		return nil, nil, fmt.Errorf("select directory subtree directories: %w", err)
	}
	for dirRows.Next() {
		var p string
		if err := dirRows.Scan(&p); err != nil {
			dirRows.Close()
			return nil, nil, err
		}
		dirs = append(dirs, p)
	}
	if err := dirRows.Close(); err != nil {
		return nil, nil, err
	}
	if _, err := tx.Exec(`DELETE FROM files WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\'`, prefix, like); err != nil {
		return nil, nil, fmt.Errorf("delete directory subtree files: %w", err)
	}
	if _, err := tx.Exec(`DELETE FROM directories WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\'`, prefix, like); err != nil {
		return nil, nil, fmt.Errorf("delete directory subtree directories: %w", err)
	}
	return files, dirs, nil
}

// RenameDirSubtree reparents a directory and all indexed descendants.
func (Store) RenameDirSubtree(tx *sql.Tx, old, new string) error {
	prefix, like := foldPath(old), escapeLike(foldPath(old))+"/%"
	type renameRow struct{ path, lower string }
	var rows []renameRow
	for _, table := range []string{"directories", "files"} {
		r, err := tx.Query(`SELECT path, path_lower FROM `+table+` WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\' ORDER BY LENGTH(path_lower) ASC`, prefix, like)
		if err != nil {
			return fmt.Errorf("select rename %s: %w", table, err)
		}
		for r.Next() {
			var v renameRow
			if err := r.Scan(&v.path, &v.lower); err != nil {
				r.Close()
				return err
			}
			rows = append(rows, v)
		}
		if err := r.Close(); err != nil {
			return err
		}
	}
	if len(rows) == 0 {
		return ErrNotFound
	}
	replace := func(p string) string { return new + p[len(old):] }
	for _, table := range []string{"directories", "files"} {
		var updates []renameRow
		r, err := tx.Query(`SELECT path, path_lower FROM `+table+` WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\' ORDER BY LENGTH(path_lower) DESC`, prefix, like)
		if err != nil {
			return fmt.Errorf("select rename %s: %w", table, err)
		}
		for r.Next() {
			var v renameRow
			if err := r.Scan(&v.path, &v.lower); err != nil {
				r.Close()
				return err
			}
			updates = append(updates, v)
		}
		if err := r.Close(); err != nil {
			return err
		}
		for _, v := range updates {
			np := replace(v.path)
			if _, err := tx.Exec(`UPDATE `+table+` SET path = ?, path_lower = ? WHERE path_lower = ?`, np, foldPath(np), v.lower); err != nil {
				return fmt.Errorf("rename %s: %w", table, err)
			}
		}
	}
	return nil
}

// GetFile looks a row up case-insensitively: the caller passes a display path,
// which is folded and matched against path_lower (PLAN.md §2). ErrNotFound if
// no row matches.
func (Store) GetFile(tx *sql.Tx, displayPath string) (FileRow, error) {
	row := tx.QueryRow(`
		SELECT path, rev, content_hash, size, updated_at, path_lower, error
		FROM files WHERE path_lower = ?
	`, foldPath(displayPath))
	return scanFileRow(row)
}

// DeleteFile removes the row for a display path (folded match on path_lower).
// Returns ErrNotFound if no row matched — the engine treats a delete of an
// already-absent path as an idempotent no-op for events (PLAN.md §6).
func (Store) DeleteFile(tx *sql.Tx, displayPath string) error {
	res, err := tx.Exec(`DELETE FROM files WHERE path_lower = ?`, foldPath(displayPath))
	if err != nil {
		return fmt.Errorf("delete file: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return ErrNotFound
	}
	return nil
}

// DeleteSubtree deletes the row at displayPath AND every row beneath it, then
// returns the deleted rows so the engine can unlink each mirror file and emit one
// file.deleted per row (PLAN.md §5: a folder delete arrives as a single entry and
// is fanned out over the index subtree).
//
// The prefix match folds displayPath and matches
//
//	path_lower = prefix  OR  path_lower LIKE prefix || '/%'
//
// so a prefix of `/foo` matches `/foo` and `/foo/bar` but NOT `/foobar`: the
// second clause requires a literal `/` boundary after the prefix, which `/foobar`
// lacks, and the first clause requires exact equality. The LIKE pattern is built
// with ESCAPE so any `%`/`_` in the path is treated literally.
func (Store) DeleteSubtree(tx *sql.Tx, displayPath string) ([]FileRow, error) {
	prefix := foldPath(displayPath)
	likePattern := escapeLike(prefix) + "/%"

	rows, err := tx.Query(`
		SELECT path, rev, content_hash, size, updated_at, path_lower, error
		FROM files
		WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\'
		ORDER BY path_lower ASC
	`, prefix, likePattern)
	if err != nil {
		return nil, fmt.Errorf("select subtree: %w", err)
	}
	var deleted []FileRow
	for rows.Next() {
		fr, err := scanFileRow(rows)
		if err != nil {
			rows.Close()
			return nil, err
		}
		deleted = append(deleted, fr)
	}
	if err := rows.Err(); err != nil {
		rows.Close()
		return nil, fmt.Errorf("scan subtree: %w", err)
	}
	rows.Close()

	if _, err := tx.Exec(`
		DELETE FROM files WHERE path_lower = ? OR path_lower LIKE ? ESCAPE '\'
	`, prefix, likePattern); err != nil {
		return nil, fmt.Errorf("delete subtree: %w", err)
	}
	return deleted, nil
}

// ListFiles returns index rows in path_lower order, optionally scoped to a folded
// folder prefix and paginated by an opaque after-cursor. It is the read backing
// the `list` MCP tool — SQL-only, *sql.Tx, opens no own connection.
//
// prefix (already folded by the caller, or "" for no scope): when non-empty it
// matches the same `/`-boundary subtree semantics as DeleteSubtree,
//
//	path_lower = prefix  OR  path_lower LIKE prefix || '/%'
//
// so a prefix of `/foo` matches `/foo` and `/foo/bar` but NOT `/foobar` (the
// LIKE clause requires a literal `/` after the prefix; the equality clause covers
// the folder path itself). The pattern is built with escapeLike + ESCAPE '\' so
// any `%`/`_` in the path is matched literally.
//
// after (the cursor): when non-empty, only rows with path_lower > after are
// returned — the previous page's last path_lower, so pages stitch without
// overlap. Rows come back ordered by path_lower ASC, capped at limit.
func (Store) ListFiles(tx *sql.Tx, prefix, after string, limit int) ([]FileRow, error) {
	var (
		where []string
		args  []any
	)
	if prefix != "" {
		where = append(where, "(path_lower = ? OR path_lower LIKE ? ESCAPE '\\')")
		args = append(args, prefix, escapeLike(prefix)+"/%")
	}
	if after != "" {
		where = append(where, "path_lower > ?")
		args = append(args, after)
	}
	q := `SELECT path, rev, content_hash, size, updated_at, path_lower, error FROM files`
	if len(where) > 0 {
		q += " WHERE " + strings.Join(where, " AND ")
	}
	q += " ORDER BY path_lower ASC LIMIT ?"
	args = append(args, limit)

	rows, err := tx.Query(q, args...)
	if err != nil {
		return nil, fmt.Errorf("list files: %w", err)
	}
	defer rows.Close()
	var out []FileRow
	for rows.Next() {
		fr, err := scanFileRow(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, fr)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("scan files: %w", err)
	}
	return out, nil
}

// ListEntries returns files and directories interleaved in path order.
func (Store) ListEntries(tx *sql.Tx, prefix, after string, limit int) ([]Entry, error) {
	where, args := []string{}, []any{}
	if prefix != "" {
		where = append(where, "(path_lower = ? OR path_lower LIKE ? ESCAPE '\\')")
		args = append(args, prefix, escapeLike(prefix)+"/%")
	}
	if after != "" {
		where = append(where, "path_lower > ?")
		args = append(args, after)
	}
	clause := ""
	if len(where) > 0 {
		clause = " WHERE " + strings.Join(where, " AND ")
	}
	q := `SELECT path, 'file', rev, content_hash, size, updated_at, path_lower FROM files` + clause + ` UNION ALL SELECT path, 'dir', '', '', 0, updated_at, path_lower FROM directories` + clause + ` ORDER BY 7 ASC LIMIT ?`
	args = append(args, args...)
	args = append(args, limit)
	rows, err := tx.Query(q, args...)
	if err != nil {
		return nil, fmt.Errorf("list entries: %w", err)
	}
	defer rows.Close()
	var out []Entry
	for rows.Next() {
		var e Entry
		if err := rows.Scan(&e.Path, &e.Kind, &e.Rev, &e.ContentHash, &e.Size, &e.UpdatedAt, &e.PathLower); err != nil {
			return nil, err
		}
		out = append(out, e)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("scan entries: %w", err)
	}
	return out, nil
}

// escapeLike escapes the LIKE metacharacters (%, _, and the escape char itself)
// so a path is matched literally as a prefix. Paired with ESCAPE '\' on the query.
func escapeLike(s string) string {
	r := strings.NewReplacer(`\`, `\\`, `%`, `\%`, `_`, `\_`)
	return r.Replace(s)
}

// ── aggregates / health ─────────────────────────────────────────────────────

// TotalSize returns SUM(size) over the index — mirror_bytes for health (the
// indexed logical size, PLAN.md §6). 0 on an empty index.
func (Store) TotalSize(tx *sql.Tx) (int64, error) {
	var total int64
	row := tx.QueryRow(`SELECT COALESCE(SUM(size), 0) FROM files`)
	if err := row.Scan(&total); err != nil {
		return 0, fmt.Errorf("total size: %w", err)
	}
	return total, nil
}

// MarkError stamps the error column on a path (folded match), recording the last
// failure for a poison entry the engine advanced past (PLAN.md §2 poison-entry
// bound). ErrNotFound if no row matched.
func (Store) MarkError(tx *sql.Tx, displayPath, errText string) error {
	res, err := tx.Exec(`UPDATE files SET error = ? WHERE path_lower = ?`, errText, foldPath(displayPath))
	if err != nil {
		return fmt.Errorf("mark error: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return ErrNotFound
	}
	return nil
}

// FailedFiles returns the count of rows carrying a non-null error — failed_files
// for health (PLAN.md §3).
func (Store) FailedFiles(tx *sql.Tx) (int, error) {
	var n int
	row := tx.QueryRow(`SELECT COUNT(*) FROM files WHERE error IS NOT NULL`)
	if err := row.Scan(&n); err != nil {
		return 0, fmt.Errorf("failed files: %w", err)
	}
	return n, nil
}

// ── scan helper ──────────────────────────────────────────────────────────────

type rowScanner interface {
	Scan(dest ...any) error
}

// scanFileRow scans one files row in the canonical column order shared by every
// read. Maps sql.ErrNoRows to ErrNotFound for single-row QueryRow callers.
func scanFileRow(r rowScanner) (FileRow, error) {
	var fr FileRow
	if err := r.Scan(&fr.Path, &fr.Rev, &fr.ContentHash, &fr.Size, &fr.UpdatedAt, &fr.PathLower, &fr.Error); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return FileRow{}, ErrNotFound
		}
		return FileRow{}, fmt.Errorf("scan file row: %w", err)
	}
	return fr, nil
}
