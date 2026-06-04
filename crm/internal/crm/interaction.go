package crm

import (
	"database/sql"
	"fmt"
	"strings"
	"time"

	"crm/internal/ids"
)

// interactionStore is the SQL-only data layer for the append-only interactions
// timeline. It satisfies the entity interface (Get/Search/Delete) and adds
// Insert, the append used by crm_log. Interactions are created via Log, never
// crm_save (PLAN.md §3): there is no edit-in-place — corrections are
// delete-and-relog. Insert trusts the typed input; the dispatcher validates the
// kind vocabulary and resolves the subject_id to exactly one FK, and the schema
// CHECKs are the backstop.
type interactionStore struct{}

// Insert appends one interaction. The dispatcher resolves the subject_id to
// exactly one of contactID/orgID/dealID (by probe-by-id) before calling.
func (interactionStore) Insert(tx *sql.Tx, kind, body string, occurredAt time.Time, contactID, orgID, dealID *string, now time.Time) (Summary, error) {
	id := ids.NewULID()
	ts := fmtTime(now)
	occurred := fmtTime(occurredAt)
	_, err := tx.Exec(`
		INSERT INTO interactions (id, kind, body, occurred_at, contact_id, org_id, deal_id, created_at, updated_at, deleted_at)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, NULL)`,
		id, kind, body, occurred,
		nullStrPtrOrNil(contactID), nullStrPtrOrNil(orgID), nullStrPtrOrNil(dealID),
		ts, ts)
	if err != nil {
		return Summary{}, mapUniqueErr(err, "interaction")
	}
	s := Summary{ID: id, Type: "interaction", Label: interactionLabel(kind, body),
		UpdatedAt: occurred, sortKey: occurredAt,
		Fields: map[string]any{"kind": kind, "occurred_at": occurred}}
	s.isCreate = true
	return s, nil
}

// interactionLabel builds a short display label from kind + body: the kind
// prefix plus the first line of the body (trimmed), so a timeline row is legible
// without the full body.
func interactionLabel(kind, body string) string {
	snippet := strings.TrimSpace(body)
	if i := strings.IndexByte(snippet, '\n'); i >= 0 {
		snippet = strings.TrimSpace(snippet[:i])
	}
	const max = 80
	if len(snippet) > max {
		snippet = snippet[:max]
	}
	if snippet == "" {
		return kind
	}
	return kind + ": " + snippet
}

// ── reads ────────────────────────────────────────────────────────────────────

// Get composes the interaction card: self fields + the resolved subject
// reference, when one is set and still live (orphan tolerated — a soft-deleted/
// absent subject is simply omitted, PLAN.md §8). A soft-deleted interaction is
// ErrNotFound, matching the other entities.
func (interactionStore) Get(tx *sql.Tx, id string) (Card, error) {
	var kind, body, occurred, created, updated string
	var contactID, orgID, dealID sql.NullString
	err := tx.QueryRow(`
		SELECT kind, body, occurred_at, contact_id, org_id, deal_id, created_at, updated_at
		FROM interactions WHERE id = ? AND deleted_at IS NULL`, id).
		Scan(&kind, &body, &occurred, &contactID, &orgID, &dealID, &created, &updated)
	if err == sql.ErrNoRows {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("get interaction: %w", err)
	}
	card := Card{"id": id, "type": "interaction", "kind": kind, "body": body,
		"occurred_at": occurred, "created_at": created, "updated_at": updated}
	if contactID.Valid {
		card["contact_id"] = contactID.String
	}
	if orgID.Valid {
		card["org_id"] = orgID.String
	}
	if dealID.Valid {
		card["deal_id"] = dealID.String
	}

	subject, err := taskSubjectRef(tx, contactID, orgID, dealID)
	if err != nil {
		return nil, err
	}
	if subject != nil {
		card["subject"] = subject
	}
	return card, nil
}

// Search matches live interactions by body substring, with optional subject_id
// (matching ANY of the three FK columns) and kind filters. Ordered by the
// timeline key occurred_at DESC, id DESC (differs from the recency ordering the
// other entities use).
func (interactionStore) Search(tx *sql.Tx, p SearchParams) ([]Summary, error) {
	where := []string{"deleted_at IS NULL"}
	var args []any
	if q := strings.TrimSpace(p.Query); q != "" {
		where = append(where, "body LIKE ? COLLATE NOCASE")
		args = append(args, "%"+q+"%")
	}
	if subject, ok := filterString(p.Filters, "subject_id"); ok {
		where = append(where, "(contact_id = ? OR org_id = ? OR deal_id = ?)")
		args = append(args, subject, subject, subject)
	}
	if kind, ok := filterString(p.Filters, "kind"); ok {
		where = append(where, "kind = ?")
		args = append(args, kind)
	}
	pred, pArgs, err := interactionKeysetAfter(tx, p.AfterID)
	if err != nil {
		return nil, err
	}
	where = append(where, pred)
	args = append(args, pArgs...)
	args = append(args, p.limit())
	rows, err := tx.Query(
		`SELECT id, kind, body, occurred_at FROM interactions WHERE `+strings.Join(where, " AND ")+
			` ORDER BY occurred_at DESC, id DESC LIMIT ?`, args...)
	if err != nil {
		return nil, fmt.Errorf("search interactions: %w", err)
	}
	defer rows.Close()
	var out []Summary
	for rows.Next() {
		var iid, kind, body, occurred string
		if err := rows.Scan(&iid, &kind, &body, &occurred); err != nil {
			return nil, err
		}
		out = append(out, Summary{ID: iid, Type: "interaction", Label: interactionLabel(kind, body),
			UpdatedAt: occurred, sortKey: parseTime(occurred),
			Fields: map[string]any{"kind": kind, "occurred_at": occurred}})
	}
	return out, rows.Err()
}

// interactionKeysetAfter is the timeline analog of store.go's keysetAfter, keyed
// on the occurred_at column (the interaction Search orders occurred_at DESC, id
// DESC, not updated_at). afterID is empty/unknown → an always-true predicate.
func interactionKeysetAfter(tx *sql.Tx, afterID string) (string, []any, error) {
	if afterID == "" {
		return "1=1", nil, nil
	}
	var occurred string
	err := tx.QueryRow(`SELECT occurred_at FROM interactions WHERE id = ?`, afterID).Scan(&occurred)
	switch err {
	case nil:
		return "(occurred_at < ? OR (occurred_at = ? AND id < ?))", []any{occurred, occurred, afterID}, nil
	case sql.ErrNoRows:
		return "1=1", nil, nil
	default:
		return "", nil, fmt.Errorf("interaction cursor lookup: %w", err)
	}
}

// Delete soft-deletes the interaction. Shallow (PLAN.md §8): interactions own no
// children. Corrections are delete-and-relog.
func (interactionStore) Delete(tx *sql.Tx, id string, at time.Time) error {
	ts := fmtTime(at)
	res, err := tx.Exec(`UPDATE interactions SET deleted_at = ?, updated_at = ? WHERE id = ? AND deleted_at IS NULL`, ts, ts, id)
	if err != nil {
		return fmt.Errorf("delete interaction: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return ErrNotFound
	}
	return nil
}
