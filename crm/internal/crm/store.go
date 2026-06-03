package crm

import (
	"database/sql"
	"fmt"
	"strings"
	"time"
)

// rowScanner is satisfied by both *sql.Row and *sql.Rows.
type rowScanner interface {
	Scan(dest ...any) error
}

// ── time helpers ─────────────────────────────────────────────────────────────

func fmtTime(t time.Time) string { return t.UTC().Format(timeFormat) }

// parseTime parses a stored timestamp; a malformed value yields the zero time
// rather than an error (storage is always written by fmtTime, so this is
// defensive).
func parseTime(s string) time.Time {
	t, _ := time.Parse(timeFormat, s)
	return t
}

// ── null helpers ─────────────────────────────────────────────────────────────

// nullStr returns nil for a nil pointer and the dereferenced value otherwise,
// for binding into a nullable column.
func nullStr(s *string) any {
	if s == nil {
		return nil
	}
	return *s
}

// nullStrOrNil binds a string, treating "" as SQL NULL — used for nullable
// columns where a provided empty string means "clear".
func nullStrOrNil(s string) any {
	if s == "" {
		return nil
	}
	return s
}

func nullInt64(p *int64) any {
	if p == nil {
		return nil
	}
	return *p
}

func boolInt(b bool) int {
	if b {
		return 1
	}
	return 0
}

// ptr returns a pointer from a NullString (nil when not valid).
func strPtr(ns sql.NullString) *string {
	if !ns.Valid {
		return nil
	}
	v := ns.String
	return &v
}

// timePtr maps a NullString timestamp to *time.Time.
func timePtr(ns sql.NullString) *time.Time {
	if !ns.Valid {
		return nil
	}
	t := parseTime(ns.String)
	return &t
}

// mapUniqueErr translates a SQLite UNIQUE/CHECK constraint violation into the
// right sentinel: a duplicate live row is a conflict (an invariant race — the
// dedup probe handles the friendly "duplicate" path before insert), a CHECK
// failure is a validation error. Other errors are wrapped with context.
func mapUniqueErr(err error, what string) error {
	if err == nil {
		return nil
	}
	msg := err.Error()
	switch {
	case strings.Contains(msg, "UNIQUE constraint failed"):
		return fmt.Errorf("%w: duplicate %s", ErrConflict, what)
	case strings.Contains(msg, "CHECK constraint failed"):
		return fmt.Errorf("%w: %s violates an allowed-value constraint", ErrValidation, what)
	case strings.Contains(msg, "constraint failed"):
		return fmt.Errorf("%w: %s", ErrConflict, what)
	default:
		return fmt.Errorf("%s: %w", what, err)
	}
}

// ── card relation readers (shared across entity Get hooks, PLAN.md §4) ───────
//
// Each entity's Get composes its own card, but the relation sub-reads are shared
// here so every join consistently filters deleted_at IS NULL (the §8 rule that
// hides orphans). recentInteractionLimit is the §4 N=20.

const recentInteractionLimit = 20

// subjectCols is the closed allowlist of interaction/task subject columns. The
// column name is interpolated into SQL, so it must never come from user input —
// callers pass one of these constants.
var subjectCols = map[string]bool{"contact_id": true, "org_id": true, "deal_id": true}

// orgRefCard returns a compact {id,name,domain} reference for the contact/deal
// "works at" relation, or nil when the org id is empty or the org row is
// soft-deleted/absent (orphan tolerated, PLAN.md §8).
func orgRefCard(tx *sql.Tx, orgID *string) (map[string]any, error) {
	if orgID == nil || *orgID == "" {
		return nil, nil
	}
	var name string
	var domain sql.NullString
	err := tx.QueryRow(`SELECT name, domain FROM organizations WHERE id = ? AND deleted_at IS NULL`, *orgID).Scan(&name, &domain)
	switch err {
	case nil:
		m := map[string]any{"id": *orgID, "name": name}
		if domain.Valid {
			m["domain"] = domain.String
		}
		return m, nil
	case sql.ErrNoRows:
		return nil, nil
	default:
		return nil, fmt.Errorf("org ref: %w", err)
	}
}

// contactCardsByOrg lists an organization's live contacts (newest-updated first).
func contactCardsByOrg(tx *sql.Tx, orgID string) ([]map[string]any, error) {
	rows, err := tx.Query(`
		SELECT id, display_name, lifecycle, title
		FROM contacts WHERE org_id = ? AND deleted_at IS NULL
		ORDER BY updated_at DESC, id DESC`, orgID)
	if err != nil {
		return nil, fmt.Errorf("contacts by org: %w", err)
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var id, display string
		var lifecycle string
		var title sql.NullString
		if err := rows.Scan(&id, &display, &lifecycle, &title); err != nil {
			return nil, err
		}
		m := map[string]any{"id": id, "display_name": display, "lifecycle": lifecycle}
		if title.Valid {
			m["title"] = title.String
		}
		out = append(out, m)
	}
	return out, rows.Err()
}

// openDealCardsByOrg lists an organization's live, open deals (status open =
// stage not in won/lost).
func openDealCardsByOrg(tx *sql.Tx, orgID string) ([]map[string]any, error) {
	rows, err := tx.Query(`
		SELECT id, name, stage, amount_cents, currency
		FROM deals WHERE org_id = ? AND deleted_at IS NULL AND stage NOT IN ('won','lost')
		ORDER BY updated_at DESC, id DESC`, orgID)
	if err != nil {
		return nil, fmt.Errorf("open deals by org: %w", err)
	}
	defer rows.Close()
	return scanDealCards(rows)
}

func scanDealCards(rows *sql.Rows) ([]map[string]any, error) {
	out := []map[string]any{}
	for rows.Next() {
		var id, name, stage, currency string
		var amount sql.NullInt64
		if err := rows.Scan(&id, &name, &stage, &amount, &currency); err != nil {
			return nil, err
		}
		m := map[string]any{"id": id, "name": name, "stage": stage, "status": dealStatus(stage), "currency": currency}
		if amount.Valid {
			m["amount_cents"] = amount.Int64
		}
		out = append(out, m)
	}
	return out, rows.Err()
}

// openDealCardsByContact lists the live, open deals a contact participates in
// (via deal_contacts).
func openDealCardsByContact(tx *sql.Tx, contactID string) ([]map[string]any, error) {
	rows, err := tx.Query(`
		SELECT d.id, d.name, d.stage, d.amount_cents, d.currency
		FROM deals d
		JOIN deal_contacts dc ON dc.deal_id = d.id AND dc.deleted_at IS NULL
		WHERE dc.contact_id = ? AND d.deleted_at IS NULL AND d.stage NOT IN ('won','lost')
		ORDER BY d.updated_at DESC, d.id DESC`, contactID)
	if err != nil {
		return nil, fmt.Errorf("open deals by contact: %w", err)
	}
	defer rows.Close()
	return scanDealCards(rows)
}

// recentInteractionCards lists the newest N interactions for a subject column.
func recentInteractionCards(tx *sql.Tx, col, id string, limit int) ([]map[string]any, error) {
	if !subjectCols[col] {
		return nil, fmt.Errorf("invalid subject column %q", col)
	}
	rows, err := tx.Query(`
		SELECT id, kind, body, occurred_at
		FROM interactions WHERE `+col+` = ? AND deleted_at IS NULL
		ORDER BY occurred_at DESC, id DESC LIMIT ?`, id, limit)
	if err != nil {
		return nil, fmt.Errorf("recent interactions: %w", err)
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var iid, kind, body, occurred string
		if err := rows.Scan(&iid, &kind, &body, &occurred); err != nil {
			return nil, err
		}
		out = append(out, map[string]any{"id": iid, "kind": kind, "body": body, "occurred_at": occurred})
	}
	return out, rows.Err()
}

// openTaskCards lists a subject's live, open tasks (due soonest first).
func openTaskCards(tx *sql.Tx, col, id string) ([]map[string]any, error) {
	if !subjectCols[col] {
		return nil, fmt.Errorf("invalid subject column %q", col)
	}
	rows, err := tx.Query(`
		SELECT id, title, status, due_at
		FROM tasks WHERE `+col+` = ? AND deleted_at IS NULL AND status = 'open'
		ORDER BY (due_at IS NULL), due_at ASC, id ASC`, id)
	if err != nil {
		return nil, fmt.Errorf("open tasks: %w", err)
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var tid, title, status string
		var due sql.NullString
		if err := rows.Scan(&tid, &title, &status, &due); err != nil {
			return nil, err
		}
		m := map[string]any{"id": tid, "title": title, "status": status}
		if due.Valid {
			m["due_at"] = due.String
		}
		out = append(out, m)
	}
	return out, rows.Err()
}

// dealParticipantCards lists a deal's live participant contacts with their roles.
func dealParticipantCards(tx *sql.Tx, dealID string) ([]map[string]any, error) {
	rows, err := tx.Query(`
		SELECT c.id, c.display_name, dc.role
		FROM deal_contacts dc
		JOIN contacts c ON c.id = dc.contact_id AND c.deleted_at IS NULL
		WHERE dc.deal_id = ? AND dc.deleted_at IS NULL
		ORDER BY dc.created_at ASC, dc.id ASC`, dealID)
	if err != nil {
		return nil, fmt.Errorf("deal participants: %w", err)
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var id, display string
		var role sql.NullString
		if err := rows.Scan(&id, &display, &role); err != nil {
			return nil, err
		}
		m := map[string]any{"id": id, "display_name": display}
		if role.Valid {
			m["role"] = role.String
		}
		out = append(out, m)
	}
	return out, rows.Err()
}

// dealStatus derives the read-only status from a deal's stage (PLAN.md §3).
func dealStatus(stage string) string {
	switch stage {
	case "won":
		return "won"
	case "lost":
		return "lost"
	default:
		return "open"
	}
}

// filterString extracts a non-empty string filter from a SearchParams.Filters
// map, returning ok=false when absent, non-string, or empty.
func filterString(f map[string]any, key string) (string, bool) {
	if f == nil {
		return "", false
	}
	v, ok := f[key]
	if !ok {
		return "", false
	}
	s, ok := v.(string)
	if !ok || s == "" {
		return "", false
	}
	return s, true
}

// keysetAfter returns the SQL predicate and args for recency keyset pagination
// after the row identified by afterID in `table` (every entity Search orders
// updated_at DESC, id DESC). When afterID is empty or unknown it yields an
// always-true predicate. table is a trusted internal constant, never user input.
func keysetAfter(tx *sql.Tx, table, afterID string) (string, []any, error) {
	if afterID == "" {
		return "1=1", nil, nil
	}
	var updated string
	err := tx.QueryRow(`SELECT updated_at FROM `+table+` WHERE id = ?`, afterID).Scan(&updated)
	switch err {
	case nil:
		return "(updated_at < ? OR (updated_at = ? AND id < ?))", []any{updated, updated, afterID}, nil
	case sql.ErrNoRows:
		return "1=1", nil, nil
	default:
		return "", nil, fmt.Errorf("cursor lookup in %s: %w", table, err)
	}
}

// liveExists reports whether a live (non-soft-deleted) row with id exists in the
// named table. Used by the dispatcher to validate cross-entity FK targets and to
// resolve subject ids. The table name is a trusted internal constant, never user
// input.
func liveExists(tx *sql.Tx, table, id string) (bool, error) {
	if id == "" {
		return false, nil
	}
	var got string
	err := tx.QueryRow(`SELECT id FROM `+table+` WHERE id = ? AND deleted_at IS NULL`, id).Scan(&got)
	switch err {
	case nil:
		return true, nil
	case sql.ErrNoRows:
		return false, nil
	default:
		return false, fmt.Errorf("probe %s: %w", table, err)
	}
}
