package crm

import (
	"errors"
	"testing"
)

func TestDeal_RoundTrip(t *testing.T) {
	s := newTestStore(t)

	var orgID, alice, bob, carol, dealID string

	// Set up an org and three participant contacts.
	withTx(t, s, func(tx *txAlias) {
		org, err := s.orgs.Save(tx, "", OrganizationInput{Name: sp("Acme Inc"), Domain: sp("acme.com")}, s.Now())
		if err != nil {
			t.Fatalf("create org: %v", err)
		}
		orgID = org.ID
		for _, c := range []struct {
			name string
			id   *string
		}{{"Alice", &alice}, {"Bob", &bob}, {"Carol", &carol}} {
			sum, err := s.contacts.Save(tx, "", ContactInput{DisplayName: sp(c.name)}, s.Now())
			if err != nil {
				t.Fatalf("create contact %s: %v", c.name, err)
			}
			*c.id = sum.ID
		}
	})

	// Create the deal with org + two participants.
	withTx(t, s, func(tx *txAlias) {
		sum, err := s.deals.Save(tx, "", DealInput{
			Name:        sp("Big Deal"),
			OrgID:       sp(orgID),
			AmountCents: i64(500000),
			Contacts: &[]DealContactInput{
				{ID: alice, Role: sp("champion")},
				{ID: bob, Role: sp("decision_maker")},
			},
		}, s.Now())
		if err != nil {
			t.Fatalf("create deal: %v", err)
		}
		if sum.ID == "" || sum.Type != "deal" || sum.Label != "Big Deal" {
			t.Fatalf("bad summary: %+v", sum)
		}
		// Defaults + derived status in the summary.
		if sum.Fields["stage"] != "lead" || sum.Fields["status"] != "open" || sum.Fields["currency"] != "USD" {
			t.Fatalf("bad summary fields: %+v", sum.Fields)
		}
		if sum.Fields["amount_cents"] != int64(500000) {
			t.Fatalf("bad amount: %v", sum.Fields["amount_cents"])
		}
		if !sum.isCreate {
			t.Errorf("create should set isCreate")
		}
		dealID = sum.ID
	})

	// Get card: derived status "open", org attached, two participants.
	withTx(t, s, func(tx *txAlias) {
		card, err := s.deals.Get(tx, dealID)
		if err != nil {
			t.Fatalf("get: %v", err)
		}
		if card["name"] != "Big Deal" || card["stage"] != "lead" || card["status"] != "open" {
			t.Fatalf("bad card: %+v", card)
		}
		if card["currency"] != "USD" || card["amount_cents"] != int64(500000) || card["org_id"] != orgID {
			t.Fatalf("bad card scalars: %+v", card)
		}
		if org, ok := card["organization"].(map[string]any); !ok || org["name"] != "Acme Inc" {
			t.Fatalf("bad organization relation: %+v", card["organization"])
		}
		parts, ok := card["contacts"].([]map[string]any)
		if !ok || len(parts) != 2 {
			t.Fatalf("want 2 participants, got %+v", card["contacts"])
		}
		for _, k := range []string{"recent_interactions", "open_tasks"} {
			if _, ok := card[k]; !ok {
				t.Errorf("card missing relation %q", k)
			}
		}
	})

	// Stage update flips derived status to "won".
	withTx(t, s, func(tx *txAlias) {
		sum, err := s.deals.Save(tx, dealID, DealInput{Stage: sp("won")}, s.Now())
		if err != nil {
			t.Fatalf("update stage: %v", err)
		}
		if sum.Fields["status"] != "won" {
			t.Fatalf("status not derived as won: %+v", sum.Fields)
		}
	})
	withTx(t, s, func(tx *txAlias) {
		card, err := s.deals.Get(tx, dealID)
		if err != nil {
			t.Fatalf("get after stage update: %v", err)
		}
		if card["stage"] != "won" || card["status"] != "won" {
			t.Fatalf("stage/status not won: %+v", card)
		}
	})

	// Declarative participant replace: drop alice, keep+re-role bob, add carol.
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.deals.Save(tx, dealID, DealInput{Contacts: &[]DealContactInput{
			{ID: bob, Role: sp("blocker")},
			{ID: carol, Role: sp("user")},
		}}, s.Now()); err != nil {
			t.Fatalf("reconcile participants: %v", err)
		}
	})
	withTx(t, s, func(tx *txAlias) {
		card, err := s.deals.Get(tx, dealID)
		if err != nil {
			t.Fatalf("get after reconcile: %v", err)
		}
		parts := card["contacts"].([]map[string]any)
		if len(parts) != 2 {
			t.Fatalf("want 2 participants after replace, got %+v", parts)
		}
		roles := map[string]any{}
		for _, p := range parts {
			roles[p["id"].(string)] = p["role"]
		}
		if _, ok := roles[alice]; ok {
			t.Errorf("alice should have been dropped")
		}
		if roles[bob] != "blocker" {
			t.Errorf("bob role not updated, got %v", roles[bob])
		}
		if roles[carol] != "user" {
			t.Errorf("carol not added, got %v", roles[carol])
		}
	})

	// Search by stage filter.
	withTx(t, s, func(tx *txAlias) {
		got, err := s.deals.Search(tx, SearchParams{Filters: map[string]any{"stage": "won"}})
		if err != nil {
			t.Fatalf("search by stage: %v", err)
		}
		if len(got) != 1 || got[0].ID != dealID {
			t.Fatalf("search by stage want 1 hit, got %+v", got)
		}
	})

	// Search by status:"open" → no hit (deal is won).
	withTx(t, s, func(tx *txAlias) {
		got, err := s.deals.Search(tx, SearchParams{Filters: map[string]any{"status": "open"}})
		if err != nil {
			t.Fatalf("search by status: %v", err)
		}
		if len(got) != 0 {
			t.Fatalf("search status:open want 0 (deal won), got %+v", got)
		}
	})

	// Delete → Get is not_found; participants soft-deleted.
	withTx(t, s, func(tx *txAlias) {
		if err := s.deals.Delete(tx, dealID, s.Now()); err != nil {
			t.Fatalf("delete: %v", err)
		}
	})
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.deals.Get(tx, dealID); !errors.Is(err, ErrNotFound) {
			t.Fatalf("get after delete: want ErrNotFound, got %v", err)
		}
		var live int
		if err := tx.QueryRow(`SELECT COUNT(*) FROM deal_contacts WHERE deal_id = ? AND deleted_at IS NULL`, dealID).Scan(&live); err != nil {
			t.Fatalf("count participants: %v", err)
		}
		if live != 0 {
			t.Fatalf("participants not soft-deleted, %d live", live)
		}
	})
}

func TestDeal_SearchOpenFilter(t *testing.T) {
	s := newTestStore(t)
	var openID string
	withTx(t, s, func(tx *txAlias) {
		o, err := s.deals.Save(tx, "", DealInput{Name: sp("Open One"), Stage: sp("qualified")}, s.Now())
		if err != nil {
			t.Fatalf("create open: %v", err)
		}
		openID = o.ID
		if _, err := s.deals.Save(tx, "", DealInput{Name: sp("Lost One"), Stage: sp("lost")}, s.Now()); err != nil {
			t.Fatalf("create lost: %v", err)
		}
	})
	withTx(t, s, func(tx *txAlias) {
		got, err := s.deals.Search(tx, SearchParams{Filters: map[string]any{"status": "open"}})
		if err != nil {
			t.Fatalf("search: %v", err)
		}
		if len(got) != 1 || got[0].ID != openID {
			t.Fatalf("status:open want only the open deal, got %+v", got)
		}
	})
}

func TestDeal_CreateRequiresName(t *testing.T) {
	s := newTestStore(t)
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.deals.Save(tx, "", DealInput{}, s.Now()); !errors.Is(err, ErrValidation) {
			t.Fatalf("want ErrValidation, got %v", err)
		}
	})
}

func TestDeal_UpdateMissingIsNotFound(t *testing.T) {
	s := newTestStore(t)
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.deals.Save(tx, "NOPE", DealInput{Name: sp("x")}, s.Now()); !errors.Is(err, ErrNotFound) {
			t.Fatalf("want ErrNotFound, got %v", err)
		}
	})
}
