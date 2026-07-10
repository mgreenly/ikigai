// Package dropbox is the domain for the dropbox service: a one-way folder mirror
// that ingests Dropbox app-folder files into the event plane (PLAN.md). This is
// the Phase 0 scaffold — a stub Service exposing only the identity probe used by
// the single MCP tool (health). The sync engine, client,
// store, mirror, events, and health subsystems are added in later phases (§8).
//
// The layering mirrors the ledger/crm chassis it was cloned from: a SQL-only
// data layer (store.go, later phases), a Service that owns the atomic
// {index change + outbox event} transaction, and event emission — with argument
// validation living at the MCP boundary (internal/mcp/tools.go).
package dropbox

import "errors"

// EntryKind identifies whether an indexed entry is a file or a directory.
type EntryKind string

const (
	KindFile EntryKind = "file"
	KindDir  EntryKind = "dir"
)

// Entry is the unified read shape for the index. Directory entries have no
// revision, hash, size, or error because those attributes belong to files.
type Entry struct {
	Path        string
	Kind        EntryKind
	Rev         string
	ContentHash string
	Size        int64
	UpdatedAt   string
	PathLower   string
}

// Error sentinels — the structured error vocabulary translated to wire shape in
// internal/mcp. The set is intentionally minimal in the scaffold and grows as
// the domain lands (download/integrity/poison-entry failures, §2).
var (
	// ErrNotFound: a requested path is absent from the mirror index.
	ErrNotFound = errors.New("dropbox: not found")
	// ErrValidation: malformed input at the domain boundary.
	ErrValidation = errors.New("dropbox: validation")
	// ErrContentHashMismatch: downloaded bytes do not hash to the content_hash
	// Dropbox reported in the file metadata. A retryable integrity failure (§2):
	// the download was truncated/corrupt and must be re-fetched before the
	// atomic rename into the mirror.
	ErrContentHashMismatch = errors.New("dropbox: content hash mismatch")
	// ErrRevMismatch: /content was asked for an exact rev (or hash) that does not
	// match the current indexed rev. The handler maps this to 409 — the §4
	// exact-bytes contract's "moved-on" signal.
	ErrRevMismatch = errors.New("dropbox: rev mismatch")
)

// Entry .tag discriminator values returned by list_folder / continue (§2). The
// engine branches on these to drive created/modified/deleted, including folder
// deletes (a folder delete arrives as a single deleted entry — §5).
const (
	// TagFile is a FileMetadata entry (download + index it).
	TagFile = "file"
	// TagFolder is a FolderMetadata entry (structural; mkdir on the mirror).
	TagFolder = "folder"
	// TagDeleted is a DeletedMetadata entry (a path — file or folder — removed).
	TagDeleted = "deleted"
)

// DeltaEntry is one normalized entry from a list_folder / continue page. Tag is
// the .tag discriminator (TagFile / TagFolder / TagDeleted). The metadata
// fields (Rev, Size, ContentHash) are populated only for TagFile; PathDisplay /
// PathLower are present on every tag (Dropbox returns them even for deletes).
type DeltaEntry struct {
	Tag         string // "file" | "folder" | "deleted"
	Name        string
	PathDisplay string // case-preserving display path within the app folder
	PathLower   string // case-folded path for case-insensitive index matching
	ID          string // Dropbox file id (file/folder only)
	Rev         string // content revision (file only)
	Size        uint64 // bytes (file only)
	ContentHash string // Dropbox block-SHA256 (file only)
}

// ListResult is the outcome of a list_folder / continue page: the entries on
// this page, the cursor to persist after applying them, and whether more pages
// remain (§2: loop continue while HasMore, persisting the cursor per page).
type ListResult struct {
	Entries []DeltaEntry
	Cursor  string
	HasMore bool
}

// FileMeta is the metadata Dropbox returns for a downloaded file (the parsed
// Dropbox-API-Result header on /files/download). The engine verifies the
// downloaded bytes against ContentHash before the atomic rename (§2).
type FileMeta struct {
	Name        string
	PathDisplay string
	PathLower   string
	ID          string
	Rev         string
	Size        uint64
	ContentHash string
}

// HealthInfo is the dropbox_health payload (PLAN.md §3): identity plus the
// mirror/disk telemetry. MirrorBytes is SUM(size) over the index (indexed
// logical size, not a directory walk); DiskFree/Total come from a statfs on the
// mirror; FailedFiles counts index rows the engine marked errored (the poison
// entries it advanced past, §2/§6).
type HealthInfo struct {
	OwnerEmail     string
	ClientID       string
	MirrorBytes    int64
	DiskFreeBytes  uint64
	DiskTotalBytes uint64
	FailedFiles    int
}
