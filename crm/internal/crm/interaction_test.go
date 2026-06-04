package crm

import (
	"errors"
	"testing"
)

func TestInteraction_RoundTrip(t *testing.T) {
	s := newTestStore(t)

	var contactID, interactionID string

	// A contact to be the interaction's subject.
	withTx(t, s, func(tx *txAlias) {
		c, err := s.contacts.Save(tx, "", ContactInput{DisplayName: sp("Alice")}, s.Now())
		if err != nil {
			t.Fatalf("create contact: %v", err)
		}
		contactID = c.ID
	})

	// Append a call interaction with a contact subject. occurred_at is the
	// harness clock; the dispatcher would have resolved subject_id → contact_id.
	withTx(t, s, func(tx *txAlias) {
		occurred := s.Now()
		sum, err := s.interactions.Insert(tx, "call", "Discussed the renewal", occurred,
			sp(contactID), nil, nil, s.Now())
		if err != nil {
			t.Fatalf("insert interaction: %v", err)
		}
		if sum.ID == "" || sum.Type != "interaction" {
			t.Fatalf("bad summary: %+v", sum)
		}
		if sum.Label != "call: Discussed the renewal" {
			t.Fatalf("bad label: %q", sum.Label)
		}
		if sum.Fields["kind"] != "call" {
			t.Fatalf("kind missing: %+v", sum.Fields)
		}
		if !sum.isCreate {
			t.Errorf("insert should set isCreate")
		}
		if sum.sortKey != occurred {
			t.Errorf("sortKey should be occurred_at: got %v want %v", sum.sortKey, occurred)
		}
		interactionID = sum.ID
	})

	// Get card: self fields + subject ref resolves to the contact.
	withTx(t, s, func(tx *txAlias) {
		card, err := s.interactions.Get(tx, interactionID)
		if err != nil {
			t.Fatalf("get: %v", err)
		}
		if card["kind"] != "call" || card["body"] != "Discussed the renewal" {
			t.Fatalf("bad card: %+v", card)
		}
		if card["contact_id"] != contactID {
			t.Fatalf("bad contact_id: %+v", card)
		}
		subj, ok := card["subject"].(map[string]any)
		if !ok {
			t.Fatalf("missing subject: %+v", card["subject"])
		}
		if subj["type"] != "contact" || subj["id"] != contactID || subj["label"] != "Alice" {
			t.Fatalf("bad subject: %+v", subj)
		}
	})

	// A second interaction (note) on the same subject for filter coverage.
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.interactions.Insert(tx, "note", "Left a voicemail", s.Now(),
			sp(contactID), nil, nil, s.Now()); err != nil {
			t.Fatalf("insert note: %v", err)
		}
	})

	// Search by subject_id → both interactions, newest-occurred first.
	withTx(t, s, func(tx *txAlias) {
		got, err := s.interactions.Search(tx, SearchParams{Filters: map[string]any{"subject_id": contactID}})
		if err != nil {
			t.Fatalf("search subject_id: %v", err)
		}
		if len(got) != 2 {
			t.Fatalf("search subject_id want 2, got %+v", got)
		}
		if got[0].Fields["kind"] != "note" {
			t.Fatalf("expected newest-occurred (note) first: %+v", got)
		}
	})

	// Search by kind:"call" → only the call.
	withTx(t, s, func(tx *txAlias) {
		got, err := s.interactions.Search(tx, SearchParams{Filters: map[string]any{"kind": "call"}})
		if err != nil {
			t.Fatalf("search kind: %v", err)
		}
		if len(got) != 1 || got[0].ID != interactionID {
			t.Fatalf("search kind:call want 1 hit, got %+v", got)
		}
	})

	// Delete → Get is not_found (append-only: corrections are delete-and-relog).
	withTx(t, s, func(tx *txAlias) {
		if err := s.interactions.Delete(tx, interactionID, s.Now()); err != nil {
			t.Fatalf("delete: %v", err)
		}
	})
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.interactions.Get(tx, interactionID); !errors.Is(err, ErrNotFound) {
			t.Fatalf("get after delete: want ErrNotFound, got %v", err)
		}
	})
}

func TestInteraction_DeleteMissingIsNotFound(t *testing.T) {
	s := newTestStore(t)
	withTx(t, s, func(tx *txAlias) {
		if err := s.interactions.Delete(tx, "NOPE", s.Now()); !errors.Is(err, ErrNotFound) {
			t.Fatalf("want ErrNotFound, got %v", err)
		}
	})
}
