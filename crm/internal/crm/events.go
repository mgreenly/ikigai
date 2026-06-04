package crm

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

	"eventplane/outbox"
)

// First-wave domain events (PLAN.md §6). Only contacts are wired in code:
// contact.created / contact.updated carry a self-describing contact snapshot
// (including lifecycle so consumers can react to funnel moves), and
// contact.tagged / contact.untagged carry the segment-membership deltas the
// `notify` consumer turns into newsletter-audience changes. The deal/task/
// interaction "second wave" (deal.stage_changed, deal.won, deal.lost,
// interaction.logged) is documented intent only and is NOT built here — no
// dormant payload structs (PLAN.md §6 "reject speculative futures").
//
// The outbox library wraps each opaque payload in the uniform envelope (source,
// id, emit time, generation) at serialize time; crm only owns the payload shape.

// contactSnapshotPayload is the contact.created / contact.updated payload. Field
// names are the wire contract. emails/phones use "primary" (not the storage
// layer's is_primary), matching the prior contacts/events.go template, redone
// for the new contact shape (PLAN.md §6). Optional columns are emitted as null
// when absent so the snapshot is self-describing.
type contactSnapshotPayload struct {
	ID          string                `json:"id"`
	DisplayName string                `json:"display_name"`
	GivenName   *string               `json:"given_name"`
	FamilyName  *string               `json:"family_name"`
	Title       *string               `json:"title"`
	OrgID       *string               `json:"org_id"`
	Lifecycle   string                `json:"lifecycle"`
	Emails      []contactEmailPayload `json:"emails"`
	Phones      []contactPhonePayload `json:"phones"`
	Tags        []string              `json:"tags"`
	CreatedAt   string                `json:"created_at"`
	UpdatedAt   string                `json:"updated_at"`
}

type contactEmailPayload struct {
	Email   string  `json:"email"`
	Label   *string `json:"label"`
	Primary bool    `json:"primary"`
}

type contactPhonePayload struct {
	Phone   string  `json:"phone"`
	Label   *string `json:"label"`
	Primary bool    `json:"primary"`
}

// contactTagPayload is the contact.tagged / contact.untagged payload: the
// contact id plus the single tag whose membership changed (PLAN.md §6 — segment
// membership). One event per added/removed tag.
type contactTagPayload struct {
	ContactID string `json:"contact_id"`
	Tag       string `json:"tag"`
}

// contactEvents builds the first-wave events for a contact Save: the
// created-or-updated snapshot followed by one tagged event per added tag and one
// untagged event per removed tag (PLAN.md §6). The diff is read off the Summary
// side-band populated by contactStore.Save — never recomputed here.
func contactEvents(tx *sql.Tx, s Summary) ([]outbox.Event, error) {
	snap, err := loadContactSnapshot(tx, s.ID)
	if err != nil {
		return nil, err
	}
	raw, err := json.Marshal(snap)
	if err != nil {
		return nil, fmt.Errorf("marshal contact snapshot payload: %w", err)
	}
	typ := "contact.updated"
	if s.isCreate {
		typ = "contact.created"
	}
	events := []outbox.Event{{Type: typ, Payload: raw}}

	for _, tag := range s.tagsAdded {
		ev, err := contactTagEvent("contact.tagged", s.ID, tag)
		if err != nil {
			return nil, err
		}
		events = append(events, ev)
	}
	for _, tag := range s.tagsRemoved {
		ev, err := contactTagEvent("contact.untagged", s.ID, tag)
		if err != nil {
			return nil, err
		}
		events = append(events, ev)
	}
	return events, nil
}

func contactTagEvent(typ, contactID, tag string) (outbox.Event, error) {
	raw, err := json.Marshal(contactTagPayload{ContactID: contactID, Tag: tag})
	if err != nil {
		return outbox.Event{}, fmt.Errorf("marshal %s payload: %w", typ, err)
	}
	return outbox.Event{Type: typ, Payload: raw}, nil
}

// loadContactSnapshot reads the just-written contact (on the same tx, before
// commit) into the event payload shape. Timestamps are re-rendered in the
// canonical wire format so consumers see a stable shape.
func loadContactSnapshot(tx *sql.Tx, id string) (contactSnapshotPayload, error) {
	var given, family, title, orgID sql.NullString
	var display, lifecycle, created, updated string
	err := tx.QueryRow(`
		SELECT given_name, family_name, display_name, title, org_id, lifecycle, created_at, updated_at
		FROM contacts WHERE id = ?`, id).
		Scan(&given, &family, &display, &title, &orgID, &lifecycle, &created, &updated)
	if err != nil {
		return contactSnapshotPayload{}, fmt.Errorf("load contact snapshot: %w", err)
	}
	p := contactSnapshotPayload{
		ID:          id,
		DisplayName: display,
		GivenName:   strPtr(given),
		FamilyName:  strPtr(family),
		Title:       strPtr(title),
		OrgID:       strPtr(orgID),
		Lifecycle:   lifecycle,
		Emails:      []contactEmailPayload{},
		Phones:      []contactPhonePayload{},
		Tags:        []string{},
		CreatedAt:   reformatTime(created),
		UpdatedAt:   reformatTime(updated),
	}

	erows, err := tx.Query(`SELECT email, label, is_primary FROM contact_emails WHERE contact_id = ? AND deleted_at IS NULL ORDER BY is_primary DESC, created_at ASC, id ASC`, id)
	if err != nil {
		return contactSnapshotPayload{}, fmt.Errorf("snapshot emails: %w", err)
	}
	defer erows.Close()
	for erows.Next() {
		var email string
		var label sql.NullString
		var primary int
		if err := erows.Scan(&email, &label, &primary); err != nil {
			return contactSnapshotPayload{}, err
		}
		p.Emails = append(p.Emails, contactEmailPayload{Email: email, Label: strPtr(label), Primary: primary != 0})
	}
	if err := erows.Err(); err != nil {
		return contactSnapshotPayload{}, err
	}

	prows, err := tx.Query(`SELECT phone, label, is_primary FROM contact_phones WHERE contact_id = ? AND deleted_at IS NULL ORDER BY is_primary DESC, created_at ASC, id ASC`, id)
	if err != nil {
		return contactSnapshotPayload{}, fmt.Errorf("snapshot phones: %w", err)
	}
	defer prows.Close()
	for prows.Next() {
		var phone string
		var label sql.NullString
		var primary int
		if err := prows.Scan(&phone, &label, &primary); err != nil {
			return contactSnapshotPayload{}, err
		}
		p.Phones = append(p.Phones, contactPhonePayload{Phone: phone, Label: strPtr(label), Primary: primary != 0})
	}
	if err := prows.Err(); err != nil {
		return contactSnapshotPayload{}, err
	}

	tags, err := contactTags(tx, id)
	if err != nil {
		return contactSnapshotPayload{}, err
	}
	p.Tags = tags
	return p, nil
}

// reformatTime re-renders a stored timestamp in the canonical wire format. A
// malformed value (never written by fmtTime) passes through unchanged.
func reformatTime(s string) string {
	if t, err := time.Parse(timeFormat, s); err == nil {
		return t.UTC().Format(timeFormat)
	}
	return s
}
