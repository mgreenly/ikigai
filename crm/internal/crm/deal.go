package crm

import (
	"database/sql"
	"fmt"
	"strings"
	"time"

	"crm/internal/ids"
)

// dealStore is the SQL-only data layer for deals and their participants
// (deal_contacts). Participants are a declarative-replace set keyed by
// contact_id (PLAN.md §4), mirroring the contact tag/email/phone reconcile in
// contact.go. The deal's read-only `status` is never stored — it is derived from
// stage via dealStatus (PLAN.md §3).
//
// Normalization and rich validation happen at the dispatcher seam; Save trusts
// the typed input but enforces the create-required NOT NULL columns with a
// corrective message rather than surfacing a raw constraint error.
type dealStore struct{}

func (dealStore) Save(tx *sql.Tx, id string, in DealInput, now time.Time) (Summary, error) {
	if id == "" {
		return dealInsert(tx, in, now)
	}
	return dealUpdate(tx, id, in, now)
}

func dealInsert(tx *sql.Tx, in DealInput, now time.Time) (Summary, error) {
	if in.Name == nil || strings.TrimSpace(*in.Name) == "" {
		return Summary{}, invalid("name", "deal name is required")
	}
	id := ids.NewULID()
	ts := fmtTime(now)
	stage := "lead"
	if in.Stage != nil {
		stage = *in.Stage
	}
	currency := "USD"
	if in.Currency != nil {
		currency = *in.Currency
	}
	_, err := tx.Exec(`
		INSERT INTO deals (id, name, org_id, stage, amount_cents, currency, close_date, created_at, updated_at, deleted_at)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, NULL)`,
		id, *in.Name, orgIDVal(in.OrgID), stage, nullInt64(in.AmountCents), currency, closeDateVal(in.CloseDate), ts, ts)
	if err != nil {
		return Summary{}, mapUniqueErr(err, "deal")
	}
	if in.Contacts != nil {
		if err := reconcileParticipants(tx, id, *in.Contacts, now); err != nil {
			return Summary{}, err
		}
	}
	s, err := dealSummary(tx, id)
	if err != nil {
		return Summary{}, err
	}
	s.isCreate = true
	return s, nil
}

func dealUpdate(tx *sql.Tx, id string, in DealInput, now time.Time) (Summary, error) {
	sets := []string{"updated_at = ?"}
	args := []any{fmtTime(now)}
	if in.Name != nil {
		if strings.TrimSpace(*in.Name) == "" {
			return Summary{}, invalid("name", "deal name must not be empty")
		}
		sets = append(sets, "name = ?")
		args = append(args, *in.Name)
	}
	if in.OrgID != nil {
		sets = append(sets, "org_id = ?")
		args = append(args, nullStrOrNil(*in.OrgID))
	}
	if in.Stage != nil {
		sets = append(sets, "stage = ?")
		args = append(args, *in.Stage)
	}
	if in.AmountCents != nil {
		sets = append(sets, "amount_cents = ?")
		args = append(args, *in.AmountCents)
	}
	if in.Currency != nil {
		sets = append(sets, "currency = ?")
		args = append(args, *in.Currency)
	}
	if in.CloseDate != nil {
		sets = append(sets, "close_date = ?")
		args = append(args, nullStrOrNil(*in.CloseDate))
	}
	args = append(args, id)
	res, err := tx.Exec(`UPDATE deals SET `+strings.Join(sets, ", ")+` WHERE id = ? AND deleted_at IS NULL`, args...)
	if err != nil {
		return Summary{}, mapUniqueErr(err, "deal")
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return Summary{}, ErrNotFound
	}
	if in.Contacts != nil {
		if err := reconcileParticipants(tx, id, *in.Contacts, now); err != nil {
			return Summary{}, err
		}
	}
	return dealSummary(tx, id)
}

// closeDateVal binds close_date on insert: nil or "" is NULL.
func closeDateVal(d *string) any {
	if d == nil || *d == "" {
		return nil
	}
	return *d
}

// ── declarative participant reconcile (PLAN.md §4) ───────────────────────────
//
// The desired array is the complete set of participants keyed by contact_id
// (mirroring reconcileTags in contact.go): live rows whose contact_id is not
// desired are soft-deleted; desired contact_ids not live are inserted with their
// role; contact_ids present in both have their role updated to match (the set is
// declarative, so a restated role wins).
func reconcileParticipants(tx *sql.Tx, dealID string, desired []DealContactInput, now time.Time) error {
	live, err := liveParticipants(tx, dealID)
	if err != nil {
		return err
	}
	want := map[string]bool{}
	for _, c := range desired {
		want[c.ID] = true
	}
	for contactID, rowID := range live {
		if !want[contactID] {
			if _, err := tx.Exec(`UPDATE deal_contacts SET deleted_at = ? WHERE id = ?`, fmtTime(now), rowID); err != nil {
				return fmt.Errorf("drop participant: %w", err)
			}
		}
	}
	for _, c := range desired {
		if _, ok := live[c.ID]; ok {
			if _, err := tx.Exec(`UPDATE deal_contacts SET role = ? WHERE deal_id = ? AND contact_id = ? AND deleted_at IS NULL`,
				nullStr(c.Role), dealID, c.ID); err != nil {
				return fmt.Errorf("update participant role: %w", err)
			}
			continue
		}
		if _, err := tx.Exec(`
			INSERT INTO deal_contacts (id, deal_id, contact_id, role, created_at, deleted_at)
			VALUES (?, ?, ?, ?, ?, NULL)`,
			ids.NewULID(), dealID, c.ID, nullStr(c.Role), fmtTime(now)); err != nil {
			return mapUniqueErr(err, "participant")
		}
	}
	return nil
}

// liveParticipants returns contact_id→row-id for a deal's live participants.
func liveParticipants(tx *sql.Tx, dealID string) (map[string]string, error) {
	rows, err := tx.Query(`SELECT id, contact_id FROM deal_contacts WHERE deal_id = ? AND deleted_at IS NULL`, dealID)
	if err != nil {
		return nil, fmt.Errorf("list participants: %w", err)
	}
	defer rows.Close()
	out := map[string]string{}
	for rows.Next() {
		var rowID, contactID string
		if err := rows.Scan(&rowID, &contactID); err != nil {
			return nil, err
		}
		out[contactID] = rowID
	}
	return out, rows.Err()
}

// ── reads ────────────────────────────────────────────────────────────────────

// dealSummary re-reads the row and builds the trimmed search summary. status is
// derived from stage (PLAN.md §3), never read from a column.
func dealSummary(tx *sql.Tx, id string) (Summary, error) {
	var name, stage, currency, updated string
	var amount sql.NullInt64
	err := tx.QueryRow(`SELECT name, stage, amount_cents, currency, updated_at FROM deals WHERE id = ?`, id).
		Scan(&name, &stage, &amount, &currency, &updated)
	if err != nil {
		return Summary{}, fmt.Errorf("deal summary: %w", err)
	}
	s := Summary{ID: id, Type: "deal", Label: name, UpdatedAt: updated, sortKey: parseTime(updated),
		Fields: map[string]any{"stage": stage, "status": dealStatus(stage), "currency": currency}}
	if amount.Valid {
		s.Fields["amount_cents"] = amount.Int64
	}
	return s, nil
}

// Get composes the deal card: self fields + relations {organization, participant
// contacts+roles, recent interactions, open tasks} (PLAN.md §4). status is
// derived from stage, never stored.
func (dealStore) Get(tx *sql.Tx, id string) (Card, error) {
	var name, stage, currency, created, updated string
	var orgID, closeDate sql.NullString
	var amount sql.NullInt64
	err := tx.QueryRow(`
		SELECT name, org_id, stage, amount_cents, currency, close_date, created_at, updated_at
		FROM deals WHERE id = ? AND deleted_at IS NULL`, id).
		Scan(&name, &orgID, &stage, &amount, &currency, &closeDate, &created, &updated)
	if err == sql.ErrNoRows {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("get deal: %w", err)
	}
	card := Card{"id": id, "type": "deal", "name": name, "stage": stage, "status": dealStatus(stage),
		"currency": currency, "created_at": created, "updated_at": updated}
	if amount.Valid {
		card["amount_cents"] = amount.Int64
	}
	if closeDate.Valid {
		card["close_date"] = closeDate.String
	}
	if orgID.Valid {
		card["org_id"] = orgID.String
	}

	org, err := orgRefCard(tx, strPtr(orgID))
	if err != nil {
		return nil, err
	}
	if org != nil {
		card["organization"] = org
	}
	contacts, err := dealParticipantCards(tx, id)
	if err != nil {
		return nil, err
	}
	card["contacts"] = contacts
	ints, err := recentInteractionCards(tx, "deal_id", id, recentInteractionLimit)
	if err != nil {
		return nil, err
	}
	card["recent_interactions"] = ints
	tasks, err := openTaskCards(tx, "deal_id", id)
	if err != nil {
		return nil, err
	}
	card["open_tasks"] = tasks
	return card, nil
}

// Search matches live deals by name substring, with optional stage / org_id /
// status filters (status maps onto the derived stage predicate), recency-ordered.
func (dealStore) Search(tx *sql.Tx, p SearchParams) ([]Summary, error) {
	where := []string{"deleted_at IS NULL"}
	var args []any
	if q := strings.TrimSpace(p.Query); q != "" {
		where = append(where, "name LIKE ? COLLATE NOCASE")
		args = append(args, "%"+q+"%")
	}
	if stage, ok := filterString(p.Filters, "stage"); ok {
		where = append(where, "stage = ?")
		args = append(args, stage)
	}
	if org, ok := filterString(p.Filters, "org_id"); ok {
		where = append(where, "org_id = ?")
		args = append(args, org)
	}
	if status, ok := filterString(p.Filters, "status"); ok {
		switch status {
		case "open":
			where = append(where, "stage NOT IN ('won','lost')")
		case "won":
			where = append(where, "stage = 'won'")
		case "lost":
			where = append(where, "stage = 'lost'")
		}
	}
	pred, pArgs, err := keysetAfter(tx, "deals", p.AfterID)
	if err != nil {
		return nil, err
	}
	where = append(where, pred)
	args = append(args, pArgs...)
	args = append(args, p.limit())
	rows, err := tx.Query(
		`SELECT id, name, stage, amount_cents, currency, updated_at FROM deals WHERE `+strings.Join(where, " AND ")+
			` ORDER BY updated_at DESC, id DESC LIMIT ?`, args...)
	if err != nil {
		return nil, fmt.Errorf("search deals: %w", err)
	}
	defer rows.Close()
	var out []Summary
	for rows.Next() {
		var id, name, stage, currency, updated string
		var amount sql.NullInt64
		if err := rows.Scan(&id, &name, &stage, &amount, &currency, &updated); err != nil {
			return nil, err
		}
		s := Summary{ID: id, Type: "deal", Label: name, UpdatedAt: updated, sortKey: parseTime(updated),
			Fields: map[string]any{"stage": stage, "status": dealStatus(stage), "currency": currency}}
		if amount.Valid {
			s.Fields["amount_cents"] = amount.Int64
		}
		out = append(out, s)
	}
	return out, rows.Err()
}

// Delete soft-deletes the deal and its owned children (deal_contacts
// participants). Shallow (PLAN.md §8): interactions/tasks that reference the deal
// are left intact and simply hidden from reads while it is deleted.
func (dealStore) Delete(tx *sql.Tx, id string, at time.Time) error {
	ts := fmtTime(at)
	res, err := tx.Exec(`UPDATE deals SET deleted_at = ?, updated_at = ? WHERE id = ? AND deleted_at IS NULL`, ts, ts, id)
	if err != nil {
		return fmt.Errorf("delete deal: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return ErrNotFound
	}
	if _, err := tx.Exec(`UPDATE deal_contacts SET deleted_at = ? WHERE deal_id = ? AND deleted_at IS NULL`, ts, id); err != nil {
		return fmt.Errorf("delete deal participants: %w", err)
	}
	return nil
}
