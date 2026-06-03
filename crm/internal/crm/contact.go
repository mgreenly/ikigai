package crm

import (
	"database/sql"
	"fmt"
	"strings"
	"time"

	"crm/internal/ids"
)

// contactStore is the SQL-only data layer for contacts and their owned children
// (emails, phones, tags). Emails/phones/tags are declarative-replace sets
// (PLAN.md §4): the array is the complete desired set, the first email/phone is
// primary, and the tag diff is captured into the returned Summary so the
// dispatcher can emit contact.tagged/untagged (PLAN.md §6).
//
// Normalization (email lowercase, phone E.164, display_name derivation, label
// validation) happens at the dispatcher seam; Save trusts the typed input but
// enforces the create-required NOT NULL columns with corrective messages.
type contactStore struct{}

func (contactStore) Save(tx *sql.Tx, id string, in ContactInput, now time.Time) (Summary, error) {
	if id == "" {
		return contactInsert(tx, in, now)
	}
	return contactUpdate(tx, id, in, now)
}

func contactInsert(tx *sql.Tx, in ContactInput, now time.Time) (Summary, error) {
	if in.DisplayName == nil || strings.TrimSpace(*in.DisplayName) == "" {
		return Summary{}, invalid("display_name", "a contact needs at least one of display_name, given_name/family_name, or a primary email")
	}
	id := ids.NewULID()
	ts := fmtTime(now)
	lifecycle := "lead"
	if in.Lifecycle != nil {
		lifecycle = *in.Lifecycle
	}
	_, err := tx.Exec(`
		INSERT INTO contacts (id, given_name, family_name, display_name, org_id, title, lifecycle, created_at, updated_at, deleted_at)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, NULL)`,
		id, nullStr(in.GivenName), nullStr(in.FamilyName), *in.DisplayName,
		orgIDVal(in.OrgID), nullStr(in.Title), lifecycle, ts, ts)
	if err != nil {
		return Summary{}, mapUniqueErr(err, "contact")
	}
	if in.Emails != nil {
		if err := reconcileEmails(tx, id, *in.Emails, now); err != nil {
			return Summary{}, err
		}
	}
	if in.Phones != nil {
		if err := reconcilePhones(tx, id, *in.Phones, now); err != nil {
			return Summary{}, err
		}
	}
	var added, removed []string
	if in.Tags != nil {
		added, removed, err = reconcileTags(tx, id, *in.Tags, now)
		if err != nil {
			return Summary{}, err
		}
	}
	s, err := contactSummary(tx, id)
	if err != nil {
		return Summary{}, err
	}
	s.isCreate = true
	s.tagsAdded, s.tagsRemoved = added, removed
	return s, nil
}

func contactUpdate(tx *sql.Tx, id string, in ContactInput, now time.Time) (Summary, error) {
	sets := []string{"updated_at = ?"}
	args := []any{fmtTime(now)}
	if in.GivenName != nil {
		sets = append(sets, "given_name = ?")
		args = append(args, nullStrOrNil(*in.GivenName))
	}
	if in.FamilyName != nil {
		sets = append(sets, "family_name = ?")
		args = append(args, nullStrOrNil(*in.FamilyName))
	}
	if in.DisplayName != nil {
		if strings.TrimSpace(*in.DisplayName) == "" {
			return Summary{}, invalid("display_name", "display_name must not be empty")
		}
		sets = append(sets, "display_name = ?")
		args = append(args, *in.DisplayName)
	}
	if in.OrgID != nil {
		sets = append(sets, "org_id = ?")
		args = append(args, nullStrOrNil(*in.OrgID))
	}
	if in.Title != nil {
		sets = append(sets, "title = ?")
		args = append(args, nullStrOrNil(*in.Title))
	}
	if in.Lifecycle != nil {
		sets = append(sets, "lifecycle = ?")
		args = append(args, *in.Lifecycle)
	}
	args = append(args, id)
	res, err := tx.Exec(`UPDATE contacts SET `+strings.Join(sets, ", ")+` WHERE id = ? AND deleted_at IS NULL`, args...)
	if err != nil {
		return Summary{}, mapUniqueErr(err, "contact")
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return Summary{}, ErrNotFound
	}
	if in.Emails != nil {
		if err := reconcileEmails(tx, id, *in.Emails, now); err != nil {
			return Summary{}, err
		}
	}
	if in.Phones != nil {
		if err := reconcilePhones(tx, id, *in.Phones, now); err != nil {
			return Summary{}, err
		}
	}
	var added, removed []string
	if in.Tags != nil {
		added, removed, err = reconcileTags(tx, id, *in.Tags, now)
		if err != nil {
			return Summary{}, err
		}
	}
	s, err := contactSummary(tx, id)
	if err != nil {
		return Summary{}, err
	}
	s.tagsAdded, s.tagsRemoved = added, removed
	return s, nil
}

// orgIDVal binds org_id on insert: nil or "" is NULL.
func orgIDVal(o *string) any {
	if o == nil || *o == "" {
		return nil
	}
	return *o
}

// ── declarative set reconcile (PLAN.md §4) ───────────────────────────────────

func reconcileEmails(tx *sql.Tx, contactID string, desired []EmailInput, now time.Time) error {
	live, err := liveChildValues(tx, "contact_emails", "email", contactID)
	if err != nil {
		return err
	}
	want := map[string]bool{}
	for _, e := range desired {
		want[e.Email] = true
	}
	for email := range live {
		if !want[email] {
			if _, err := tx.Exec(`UPDATE contact_emails SET deleted_at = ? WHERE id = ?`, fmtTime(now), live[email]); err != nil {
				return fmt.Errorf("soft-delete email: %w", err)
			}
		}
	}
	for _, e := range desired {
		if _, ok := live[e.Email]; ok {
			if _, err := tx.Exec(`UPDATE contact_emails SET label = ? WHERE contact_id = ? AND email = ? AND deleted_at IS NULL`,
				nullStr(e.Label), contactID, e.Email); err != nil {
				return fmt.Errorf("update email label: %w", err)
			}
			continue
		}
		if _, err := tx.Exec(`
			INSERT INTO contact_emails (id, contact_id, email, label, is_primary, created_at, deleted_at)
			VALUES (?, ?, ?, ?, 0, ?, NULL)`,
			ids.NewULID(), contactID, e.Email, nullStr(e.Label), fmtTime(now)); err != nil {
			return mapUniqueErr(err, "email")
		}
	}
	return setPrimaryChild(tx, "contact_emails", "email", contactID, firstEmail(desired))
}

func reconcilePhones(tx *sql.Tx, contactID string, desired []PhoneInput, now time.Time) error {
	live, err := liveChildValues(tx, "contact_phones", "phone", contactID)
	if err != nil {
		return err
	}
	want := map[string]bool{}
	for _, p := range desired {
		want[p.Phone] = true
	}
	for phone := range live {
		if !want[phone] {
			if _, err := tx.Exec(`UPDATE contact_phones SET deleted_at = ? WHERE id = ?`, fmtTime(now), live[phone]); err != nil {
				return fmt.Errorf("soft-delete phone: %w", err)
			}
		}
	}
	for _, p := range desired {
		if _, ok := live[p.Phone]; ok {
			if _, err := tx.Exec(`UPDATE contact_phones SET label = ? WHERE contact_id = ? AND phone = ? AND deleted_at IS NULL`,
				nullStr(p.Label), contactID, p.Phone); err != nil {
				return fmt.Errorf("update phone label: %w", err)
			}
			continue
		}
		if _, err := tx.Exec(`
			INSERT INTO contact_phones (id, contact_id, phone, label, is_primary, created_at, deleted_at)
			VALUES (?, ?, ?, ?, 0, ?, NULL)`,
			ids.NewULID(), contactID, p.Phone, nullStr(p.Label), fmtTime(now)); err != nil {
			return mapUniqueErr(err, "phone")
		}
	}
	return setPrimaryChild(tx, "contact_phones", "phone", contactID, firstPhone(desired))
}

// reconcileTags diffs the live tag set against the desired set, returning the
// tags added and removed (for contact.tagged/untagged events, PLAN.md §6).
func reconcileTags(tx *sql.Tx, contactID string, desired []string, now time.Time) (added, removed []string, err error) {
	live, err := liveChildValues(tx, "contact_tags", "tag", contactID)
	if err != nil {
		return nil, nil, err
	}
	want := map[string]bool{}
	for _, t := range desired {
		want[t] = true
	}
	for tag := range live {
		if !want[tag] {
			if _, err := tx.Exec(`UPDATE contact_tags SET deleted_at = ? WHERE id = ?`, fmtTime(now), live[tag]); err != nil {
				return nil, nil, fmt.Errorf("untag: %w", err)
			}
			removed = append(removed, tag)
		}
	}
	for _, tag := range desired {
		if _, ok := live[tag]; ok {
			continue
		}
		if _, err := tx.Exec(`
			INSERT INTO contact_tags (id, contact_id, tag, created_at, deleted_at) VALUES (?, ?, ?, ?, NULL)`,
			ids.NewULID(), contactID, tag, fmtTime(now)); err != nil {
			return nil, nil, mapUniqueErr(err, "tag")
		}
		added = append(added, tag)
	}
	return added, removed, nil
}

// liveChildValues returns value→id for the live rows of an owned child table.
func liveChildValues(tx *sql.Tx, table, col, contactID string) (map[string]string, error) {
	rows, err := tx.Query(`SELECT id, `+col+` FROM `+table+` WHERE contact_id = ? AND deleted_at IS NULL`, contactID)
	if err != nil {
		return nil, fmt.Errorf("list %s: %w", table, err)
	}
	defer rows.Close()
	out := map[string]string{}
	for rows.Next() {
		var id, val string
		if err := rows.Scan(&id, &val); err != nil {
			return nil, err
		}
		out[val] = id
	}
	return out, rows.Err()
}

// setPrimaryChild demotes every live row and promotes the one matching primary
// (the first in the desired set). Demote-then-promote keeps the partial unique
// "one live primary" index satisfied at every step.
func setPrimaryChild(tx *sql.Tx, table, col, contactID, primary string) error {
	if _, err := tx.Exec(`UPDATE `+table+` SET is_primary = 0 WHERE contact_id = ? AND deleted_at IS NULL`, contactID); err != nil {
		return fmt.Errorf("demote %s: %w", table, err)
	}
	if primary == "" {
		return nil
	}
	if _, err := tx.Exec(`UPDATE `+table+` SET is_primary = 1 WHERE contact_id = ? AND `+col+` = ? AND deleted_at IS NULL`, contactID, primary); err != nil {
		return fmt.Errorf("promote %s: %w", table, err)
	}
	return nil
}

func firstEmail(es []EmailInput) string {
	if len(es) == 0 {
		return ""
	}
	return es[0].Email
}

func firstPhone(ps []PhoneInput) string {
	if len(ps) == 0 {
		return ""
	}
	return ps[0].Phone
}

// ── reads ────────────────────────────────────────────────────────────────────

func contactSummary(tx *sql.Tx, id string) (Summary, error) {
	var display, lifecycle, updated string
	err := tx.QueryRow(`SELECT display_name, lifecycle, updated_at FROM contacts WHERE id = ?`, id).Scan(&display, &lifecycle, &updated)
	if err != nil {
		return Summary{}, fmt.Errorf("contact summary: %w", err)
	}
	s := Summary{ID: id, Type: "contact", Label: display, UpdatedAt: updated, sortKey: parseTime(updated),
		Fields: map[string]any{"lifecycle": lifecycle}}
	if email, ok := primaryEmail(tx, id); ok {
		s.Fields["primary_email"] = email
	}
	return s, nil
}

func primaryEmail(tx *sql.Tx, contactID string) (string, bool) {
	var email string
	err := tx.QueryRow(`SELECT email FROM contact_emails WHERE contact_id = ? AND is_primary = 1 AND deleted_at IS NULL`, contactID).Scan(&email)
	if err != nil {
		return "", false
	}
	return email, true
}

// Get composes the contact card: identity + emails/phones/tags + relations
// {org, open deals, recent interactions, open tasks} (PLAN.md §4).
func (contactStore) Get(tx *sql.Tx, id string) (Card, error) {
	var given, family, title sql.NullString
	var orgID sql.NullString
	var display, lifecycle, created, updated string
	err := tx.QueryRow(`
		SELECT given_name, family_name, display_name, org_id, title, lifecycle, created_at, updated_at
		FROM contacts WHERE id = ? AND deleted_at IS NULL`, id).
		Scan(&given, &family, &display, &orgID, &title, &lifecycle, &created, &updated)
	if err == sql.ErrNoRows {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("get contact: %w", err)
	}
	card := Card{"id": id, "type": "contact", "display_name": display, "lifecycle": lifecycle,
		"created_at": created, "updated_at": updated}
	if given.Valid {
		card["given_name"] = given.String
	}
	if family.Valid {
		card["family_name"] = family.String
	}
	if title.Valid {
		card["title"] = title.String
	}
	if orgID.Valid {
		card["org_id"] = orgID.String
	}

	emails, err := emailCards(tx, id)
	if err != nil {
		return nil, err
	}
	card["emails"] = emails
	phones, err := phoneCards(tx, id)
	if err != nil {
		return nil, err
	}
	card["phones"] = phones
	tags, err := contactTags(tx, id)
	if err != nil {
		return nil, err
	}
	card["tags"] = tags

	op := strPtr(orgID)
	org, err := orgRefCard(tx, op)
	if err != nil {
		return nil, err
	}
	if org != nil {
		card["organization"] = org
	}
	deals, err := openDealCardsByContact(tx, id)
	if err != nil {
		return nil, err
	}
	card["open_deals"] = deals
	ints, err := recentInteractionCards(tx, "contact_id", id, recentInteractionLimit)
	if err != nil {
		return nil, err
	}
	card["recent_interactions"] = ints
	tasks, err := openTaskCards(tx, "contact_id", id)
	if err != nil {
		return nil, err
	}
	card["open_tasks"] = tasks
	return card, nil
}

func emailCards(tx *sql.Tx, contactID string) ([]map[string]any, error) {
	rows, err := tx.Query(`SELECT id, email, label, is_primary FROM contact_emails WHERE contact_id = ? AND deleted_at IS NULL ORDER BY is_primary DESC, created_at ASC, id ASC`, contactID)
	if err != nil {
		return nil, fmt.Errorf("emails: %w", err)
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var eid, email string
		var label sql.NullString
		var primary int
		if err := rows.Scan(&eid, &email, &label, &primary); err != nil {
			return nil, err
		}
		m := map[string]any{"id": eid, "email": email, "primary": primary != 0}
		if label.Valid {
			m["label"] = label.String
		}
		out = append(out, m)
	}
	return out, rows.Err()
}

func phoneCards(tx *sql.Tx, contactID string) ([]map[string]any, error) {
	rows, err := tx.Query(`SELECT id, phone, label, is_primary FROM contact_phones WHERE contact_id = ? AND deleted_at IS NULL ORDER BY is_primary DESC, created_at ASC, id ASC`, contactID)
	if err != nil {
		return nil, fmt.Errorf("phones: %w", err)
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var pid, phone string
		var label sql.NullString
		var primary int
		if err := rows.Scan(&pid, &phone, &label, &primary); err != nil {
			return nil, err
		}
		m := map[string]any{"id": pid, "phone": phone, "primary": primary != 0}
		if label.Valid {
			m["label"] = label.String
		}
		out = append(out, m)
	}
	return out, rows.Err()
}

func contactTags(tx *sql.Tx, contactID string) ([]string, error) {
	rows, err := tx.Query(`SELECT tag FROM contact_tags WHERE contact_id = ? AND deleted_at IS NULL ORDER BY tag ASC`, contactID)
	if err != nil {
		return nil, fmt.Errorf("tags: %w", err)
	}
	defer rows.Close()
	out := []string{}
	for rows.Next() {
		var tag string
		if err := rows.Scan(&tag); err != nil {
			return nil, err
		}
		out = append(out, tag)
	}
	return out, rows.Err()
}

// Search matches live contacts by name/email/phone substring, with optional
// lifecycle / org_id / tag filters, recency-ordered.
func (contactStore) Search(tx *sql.Tx, p SearchParams) ([]Summary, error) {
	where := []string{"c.deleted_at IS NULL"}
	var args []any
	if q := strings.TrimSpace(p.Query); q != "" {
		like := "%" + q + "%"
		where = append(where, `(
			c.display_name LIKE ? COLLATE NOCASE
			OR c.given_name LIKE ? COLLATE NOCASE
			OR c.family_name LIKE ? COLLATE NOCASE
			OR EXISTS (SELECT 1 FROM contact_emails e WHERE e.contact_id = c.id AND e.deleted_at IS NULL AND e.email LIKE ? COLLATE NOCASE)
			OR EXISTS (SELECT 1 FROM contact_phones ph WHERE ph.contact_id = c.id AND ph.deleted_at IS NULL AND ph.phone LIKE ? COLLATE NOCASE)
		)`)
		args = append(args, like, like, like, like, like)
	}
	if lc, ok := filterString(p.Filters, "lifecycle"); ok {
		where = append(where, "c.lifecycle = ?")
		args = append(args, lc)
	}
	if org, ok := filterString(p.Filters, "org_id"); ok {
		where = append(where, "c.org_id = ?")
		args = append(args, org)
	}
	if tag, ok := filterString(p.Filters, "tag"); ok {
		where = append(where, "EXISTS (SELECT 1 FROM contact_tags t WHERE t.contact_id = c.id AND t.deleted_at IS NULL AND t.tag = ?)")
		args = append(args, tag)
	}
	pred, pArgs, err := keysetAfter(tx, "contacts", p.AfterID)
	if err != nil {
		return nil, err
	}
	where = append(where, strings.ReplaceAll(pred, "updated_at", "c.updated_at"))
	args = append(args, pArgs...)
	args = append(args, p.limit())
	rows, err := tx.Query(`
		SELECT c.id, c.display_name, c.lifecycle, c.updated_at
		FROM contacts c WHERE `+strings.Join(where, " AND ")+`
		ORDER BY c.updated_at DESC, c.id DESC LIMIT ?`, args...)
	if err != nil {
		return nil, fmt.Errorf("search contacts: %w", err)
	}
	defer rows.Close()
	var out []Summary
	var ids []string
	for rows.Next() {
		var id, display, lifecycle, updated string
		if err := rows.Scan(&id, &display, &lifecycle, &updated); err != nil {
			return nil, err
		}
		out = append(out, Summary{ID: id, Type: "contact", Label: display, UpdatedAt: updated,
			sortKey: parseTime(updated), Fields: map[string]any{"lifecycle": lifecycle}})
		ids = append(ids, id)
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	// Attach primary email to each summary (best-effort disambiguation).
	for i, id := range ids {
		if email, ok := primaryEmail(tx, id); ok {
			out[i].Fields["primary_email"] = email
		}
	}
	return out, nil
}

// Delete soft-deletes the contact and its owned children (emails/phones/tags).
// Shallow (PLAN.md §8): deal_contacts rows referencing this contact are left
// intact and simply hidden from reads (the participant join filters on the
// contact's deleted_at).
func (contactStore) Delete(tx *sql.Tx, id string, at time.Time) error {
	ts := fmtTime(at)
	res, err := tx.Exec(`UPDATE contacts SET deleted_at = ?, updated_at = ? WHERE id = ? AND deleted_at IS NULL`, ts, ts, id)
	if err != nil {
		return fmt.Errorf("delete contact: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return ErrNotFound
	}
	for _, table := range []string{"contact_emails", "contact_phones", "contact_tags"} {
		if _, err := tx.Exec(`UPDATE `+table+` SET deleted_at = ? WHERE contact_id = ? AND deleted_at IS NULL`, ts, id); err != nil {
			return fmt.Errorf("delete %s: %w", table, err)
		}
	}
	return nil
}
