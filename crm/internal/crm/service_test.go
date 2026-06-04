package crm

import (
	"context"
	"encoding/json"
	"errors"
	"testing"
)

// jsonFields marshals a map into the loose `fields` blob the dispatcher decodes.
func jsonFields(t *testing.T, m map[string]any) []byte {
	t.Helper()
	b, err := json.Marshal(m)
	if err != nil {
		t.Fatalf("marshal fields: %v", err)
	}
	return b
}

// mustSave runs Save and fails on error, returning the summary.
func mustSave(t *testing.T, s *Service, typ, id string, fields map[string]any, force bool) Summary {
	t.Helper()
	sum, err := s.Save(context.Background(), typ, id, jsonFields(t, fields), force)
	if err != nil {
		t.Fatalf("save %s: %v", typ, err)
	}
	return sum
}

// TestServiceOrganizationRoundTrip exercises Save → Get → Search → Delete.
func TestServiceOrganizationRoundTrip(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	sum := mustSave(t, s, "organization", "", map[string]any{"name": "Acme", "domain": "Acme.com"}, false)
	if sum.Type != "organization" || sum.Label != "Acme" {
		t.Fatalf("unexpected summary: %+v", sum)
	}

	card, err := s.Get(ctx, sum.ID)
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if card["name"] != "Acme" {
		t.Fatalf("get name: %v", card["name"])
	}
	// domain normalized to lowercase.
	if card["domain"] != "acme.com" {
		t.Fatalf("domain not lowercased: %v", card["domain"])
	}

	got, err := s.Search(ctx, SearchParams{Query: "acme", Type: "organization"})
	if err != nil {
		t.Fatalf("search: %v", err)
	}
	if len(got) != 1 || got[0].ID != sum.ID {
		t.Fatalf("search miss: %+v", got)
	}

	if err := s.Delete(ctx, "organization", sum.ID); err != nil {
		t.Fatalf("delete: %v", err)
	}
	if _, err := s.Get(ctx, sum.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected not found after delete, got %v", err)
	}
}

// TestServiceContactRoundTripAndNormalization covers email lowercasing, phone
// E.164 normalization, display_name derivation, and the round trip.
func TestServiceContactRoundTripAndNormalization(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	sum := mustSave(t, s, "contact", "", map[string]any{
		"emails": []map[string]any{{"email": "  Bob@Example.COM "}},
		"phones": []map[string]any{{"phone": "+1 415-555-0123"}},
	}, false)

	card, err := s.Get(ctx, sum.ID)
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	// display_name derived from the primary email.
	if card["display_name"] != "bob@example.com" {
		t.Fatalf("display_name derivation: %v", card["display_name"])
	}
	emails := card["emails"].([]map[string]any)
	if len(emails) != 1 || emails[0]["email"] != "bob@example.com" {
		t.Fatalf("email not normalized: %+v", emails)
	}
	phones := card["phones"].([]map[string]any)
	if len(phones) != 1 || phones[0]["phone"] != "+14155550123" {
		t.Fatalf("phone not E.164: %+v", phones)
	}

	got, err := s.Search(ctx, SearchParams{Query: "bob@example", Type: "contact"})
	if err != nil {
		t.Fatalf("search: %v", err)
	}
	if len(got) != 1 || got[0].ID != sum.ID {
		t.Fatalf("search miss: %+v", got)
	}

	if err := s.Delete(ctx, "contact", sum.ID); err != nil {
		t.Fatalf("delete: %v", err)
	}
	if _, err := s.Get(ctx, sum.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected not found after delete")
	}
}

// TestServiceContactDuplicateAndForce covers the dedup probe by primary email.
func TestServiceContactDuplicateAndForce(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	first := mustSave(t, s, "contact", "", map[string]any{
		"display_name": "Bob",
		"emails":       []map[string]any{{"email": "bob@example.com"}},
	}, false)

	// Second contact with the same primary email → DuplicateError carrying the
	// existing id, no force.
	_, err := s.Save(ctx, "contact", "", jsonFields(t, map[string]any{
		"display_name": "Robert",
		"emails":       []map[string]any{{"email": "Bob@Example.com"}},
	}), false)
	var dup *DuplicateError
	if !errors.As(err, &dup) {
		t.Fatalf("expected DuplicateError, got %v", err)
	}
	if dup.ExistingID != first.ID {
		t.Fatalf("duplicate existing_id mismatch: %s vs %s", dup.ExistingID, first.ID)
	}

	// Same call with force:true succeeds.
	forced := mustSave(t, s, "contact", "", map[string]any{
		"display_name": "Robert",
		"emails":       []map[string]any{{"email": "bob@example.com"}},
	}, true)
	if forced.ID == first.ID {
		t.Fatalf("force should create a new contact")
	}
}

// TestServiceDealRejectsClientStatus asserts a client-supplied derived status is
// rejected with a validation error.
func TestServiceDealRejectsClientStatus(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	_, err := s.Save(ctx, "deal", "", jsonFields(t, map[string]any{
		"name":   "Big Deal",
		"status": "won",
	}), false)
	if !errors.Is(err, ErrValidation) {
		t.Fatalf("expected validation error for client status, got %v", err)
	}
	var ve *ValidationError
	if errors.As(err, &ve) && ve.Field != "status" {
		t.Fatalf("expected field=status, got %q", ve.Field)
	}
}

// TestServiceDealRoundTripAndStage covers stage validation, derived status, and
// participant FK liveness.
func TestServiceDealRoundTripAndStage(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	org := mustSave(t, s, "organization", "", map[string]any{"name": "Globex"}, false)
	contact := mustSave(t, s, "contact", "", map[string]any{"display_name": "Carol"}, false)

	deal := mustSave(t, s, "deal", "", map[string]any{
		"name":     "Globex Renewal",
		"org_id":   org.ID,
		"stage":    "proposal",
		"contacts": []map[string]any{{"id": contact.ID, "role": "champion"}},
	}, false)
	if deal.Fields["status"] != "open" {
		t.Fatalf("expected derived status open, got %v", deal.Fields["status"])
	}

	card, err := s.Get(ctx, deal.ID)
	if err != nil {
		t.Fatalf("get deal: %v", err)
	}
	participants := card["contacts"].([]map[string]any)
	if len(participants) != 1 || participants[0]["id"] != contact.ID {
		t.Fatalf("participant missing: %+v", participants)
	}

	// Move to won → derived status won.
	won := mustSave(t, s, "deal", deal.ID, map[string]any{"stage": "won"}, false)
	if won.Fields["status"] != "won" {
		t.Fatalf("expected status won, got %v", won.Fields["status"])
	}

	// Bad stage rejected.
	if _, err := s.Save(ctx, "deal", "", jsonFields(t, map[string]any{"name": "X", "stage": "bogus"}), false); !errors.Is(err, ErrValidation) {
		t.Fatalf("expected validation for bad stage, got %v", err)
	}

	// Participant referencing a non-live contact rejected.
	if _, err := s.Save(ctx, "deal", "", jsonFields(t, map[string]any{
		"name":     "Y",
		"contacts": []map[string]any{{"id": "01NOTREAL"}},
	}), false); !errors.Is(err, ErrValidation) {
		t.Fatalf("expected validation for dead participant, got %v", err)
	}
}

// TestServiceTaskRoundTrip covers task save/get/delete and the done_at
// convenience via status:done.
func TestServiceTaskRoundTrip(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	contact := mustSave(t, s, "contact", "", map[string]any{"display_name": "Dave"}, false)
	task := mustSave(t, s, "task", "", map[string]any{
		"title":      "Follow up",
		"contact_id": contact.ID,
	}, false)
	if task.Fields["status"] != "open" {
		t.Fatalf("expected open task, got %v", task.Fields["status"])
	}

	done := mustSave(t, s, "task", task.ID, map[string]any{"status": "done"}, false)
	if done.Fields["status"] != "done" {
		t.Fatalf("expected done, got %v", done.Fields["status"])
	}
	card, err := s.Get(ctx, task.ID)
	if err != nil {
		t.Fatalf("get task: %v", err)
	}
	if card["done_at"] == nil || card["done_at"] == "" {
		t.Fatalf("expected done_at stamped, card: %+v", card)
	}

	// FK liveness: a task pointing at a non-live org is rejected.
	if _, err := s.Save(ctx, "task", "", jsonFields(t, map[string]any{
		"title":  "bad",
		"org_id": "01NOPE",
	}), false); !errors.Is(err, ErrValidation) {
		t.Fatalf("expected validation for dead org_id, got %v", err)
	}

	if err := s.Delete(ctx, "task", task.ID); err != nil {
		t.Fatalf("delete: %v", err)
	}
}

// TestServiceLog covers interaction creation via Log, subject probe-by-id, kind
// validation, and the not_found path.
func TestServiceLog(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	contact := mustSave(t, s, "contact", "", map[string]any{"display_name": "Erin"}, false)

	logged, err := s.Log(ctx, LogInput{SubjectID: contact.ID, Kind: "call", Body: "Discussed renewal."})
	if err != nil {
		t.Fatalf("log: %v", err)
	}
	if logged.Type != "interaction" {
		t.Fatalf("unexpected type: %+v", logged)
	}

	// It shows up on the contact card's recent interactions.
	card, err := s.Get(ctx, contact.ID)
	if err != nil {
		t.Fatalf("get contact: %v", err)
	}
	ints := card["recent_interactions"].([]map[string]any)
	if len(ints) != 1 || ints[0]["kind"] != "call" {
		t.Fatalf("interaction not on card: %+v", ints)
	}

	// Reachable via Get/Search/Delete.
	if _, err := s.Get(ctx, logged.ID); err != nil {
		t.Fatalf("get interaction: %v", err)
	}
	if err := s.Delete(ctx, "interaction", logged.ID); err != nil {
		t.Fatalf("delete interaction: %v", err)
	}

	// Bad kind rejected.
	if _, err := s.Log(ctx, LogInput{SubjectID: contact.ID, Kind: "tweet", Body: "x"}); !errors.Is(err, ErrValidation) {
		t.Fatalf("expected validation for bad kind, got %v", err)
	}
	// Unknown subject → not found.
	if _, err := s.Log(ctx, LogInput{SubjectID: "01GHOST", Kind: "note", Body: "x"}); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected not found for unknown subject, got %v", err)
	}
	// Empty body rejected.
	if _, err := s.Log(ctx, LogInput{SubjectID: contact.ID, Kind: "note", Body: "  "}); !errors.Is(err, ErrValidation) {
		t.Fatalf("expected validation for empty body, got %v", err)
	}
}

// TestServiceSoftDeleteOrphanFiltering asserts the §8 rule: a deleted FK target
// no longer appears in a referring entity's card (orphan hidden, not blocked).
func TestServiceSoftDeleteOrphanFiltering(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()

	org := mustSave(t, s, "organization", "", map[string]any{"name": "Initech"}, false)
	contact := mustSave(t, s, "contact", "", map[string]any{
		"display_name": "Frank",
		"org_id":       org.ID,
	}, false)

	// Before delete, the contact card shows the organization relation.
	card, err := s.Get(ctx, contact.ID)
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if card["organization"] == nil {
		t.Fatalf("expected organization relation before delete")
	}

	// Soft-delete the org (shallow): the contact's org_id FK stays, but the
	// relation must vanish from the card because joins filter deleted_at.
	if err := s.Delete(ctx, "organization", org.ID); err != nil {
		t.Fatalf("delete org: %v", err)
	}
	card, err = s.Get(ctx, contact.ID)
	if err != nil {
		t.Fatalf("get after org delete: %v", err)
	}
	if card["organization"] != nil {
		t.Fatalf("orphaned organization relation should be hidden, got %v", card["organization"])
	}
	// The contact itself is still live and the raw org_id FK is preserved.
	if card["org_id"] != org.ID {
		t.Fatalf("org_id FK should be preserved for undelete, got %v", card["org_id"])
	}

	// Deleted org no longer appears in search.
	got, err := s.Search(ctx, SearchParams{Query: "Initech", Type: "organization"})
	if err != nil {
		t.Fatalf("search: %v", err)
	}
	if len(got) != 0 {
		t.Fatalf("deleted org should not appear in search, got %+v", got)
	}
}
