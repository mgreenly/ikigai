package crm

import (
	"errors"
	"testing"
)

func TestContact_RoundTrip(t *testing.T) {
	s := newTestStore(t)

	var cid string
	withTx(t, s, func(tx *txAlias) {
		sum, err := s.contacts.Save(tx, "", ContactInput{
			DisplayName: sp("Bob Jones"),
			GivenName:   sp("Bob"),
			FamilyName:  sp("Jones"),
			Lifecycle:   sp("subscriber"),
			Emails:      &[]EmailInput{{Email: "bob@acme.com"}, {Email: "b@home.net", Label: sp("home")}},
			Phones:      &[]PhoneInput{{Phone: "+14155550123"}},
			Tags:        &[]string{"newsletter", "vip"},
		}, s.Now())
		if err != nil {
			t.Fatalf("create: %v", err)
		}
		if sum.Type != "contact" || sum.Label != "Bob Jones" || !sum.isCreate {
			t.Fatalf("bad summary: %+v", sum)
		}
		if len(sum.tagsAdded) != 2 {
			t.Fatalf("want 2 tags added, got %v", sum.tagsAdded)
		}
		cid = sum.ID
	})

	withTx(t, s, func(tx *txAlias) {
		card, err := s.contacts.Get(tx, cid)
		if err != nil {
			t.Fatalf("get: %v", err)
		}
		if card["lifecycle"] != "subscriber" {
			t.Errorf("lifecycle: %v", card["lifecycle"])
		}
		emails := card["emails"].([]map[string]any)
		if len(emails) != 2 {
			t.Fatalf("want 2 emails, got %d", len(emails))
		}
		if emails[0]["email"] != "bob@acme.com" || emails[0]["primary"] != true {
			t.Errorf("first email should be primary bob@acme.com: %+v", emails[0])
		}
		tags := card["tags"].([]string)
		if len(tags) != 2 {
			t.Errorf("want 2 tags, got %v", tags)
		}
	})

	// Declarative tag edit: drop vip, add lead-magnet → diff is +lead-magnet -vip.
	withTx(t, s, func(tx *txAlias) {
		sum, err := s.contacts.Save(tx, cid, ContactInput{Tags: &[]string{"newsletter", "lead-magnet"}}, s.Now())
		if err != nil {
			t.Fatalf("retag: %v", err)
		}
		if len(sum.tagsAdded) != 1 || sum.tagsAdded[0] != "lead-magnet" {
			t.Errorf("tagsAdded: %v", sum.tagsAdded)
		}
		if len(sum.tagsRemoved) != 1 || sum.tagsRemoved[0] != "vip" {
			t.Errorf("tagsRemoved: %v", sum.tagsRemoved)
		}
	})

	// Omitting tags leaves them untouched; clearing emails to a single primary.
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.contacts.Save(tx, cid, ContactInput{Emails: &[]EmailInput{{Email: "b@home.net"}}}, s.Now()); err != nil {
			t.Fatalf("email replace: %v", err)
		}
	})
	withTx(t, s, func(tx *txAlias) {
		card, _ := s.contacts.Get(tx, cid)
		emails := card["emails"].([]map[string]any)
		if len(emails) != 1 || emails[0]["email"] != "b@home.net" || emails[0]["primary"] != true {
			t.Fatalf("email set not reconciled: %+v", emails)
		}
		if tags := card["tags"].([]string); len(tags) != 2 {
			t.Errorf("tags should be untouched (2), got %v", tags)
		}
	})

	// Filter searches.
	withTx(t, s, func(tx *txAlias) {
		got, err := s.contacts.Search(tx, SearchParams{Filters: map[string]any{"tag": "newsletter"}})
		if err != nil {
			t.Fatalf("search tag: %v", err)
		}
		if len(got) != 1 || got[0].ID != cid {
			t.Fatalf("tag search: %+v", got)
		}
		got, _ = s.contacts.Search(tx, SearchParams{Filters: map[string]any{"lifecycle": "customer"}})
		if len(got) != 0 {
			t.Errorf("lifecycle filter should exclude: %+v", got)
		}
		got, _ = s.contacts.Search(tx, SearchParams{Query: "jones"})
		if len(got) != 1 {
			t.Errorf("name search: %+v", got)
		}
	})

	// Delete cascades to owned children.
	withTx(t, s, func(tx *txAlias) {
		if err := s.contacts.Delete(tx, cid, s.Now()); err != nil {
			t.Fatalf("delete: %v", err)
		}
	})
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.contacts.Get(tx, cid); !errors.Is(err, ErrNotFound) {
			t.Fatalf("get after delete: %v", err)
		}
		var liveChildren int
		tx.QueryRow(`SELECT
			(SELECT COUNT(*) FROM contact_emails WHERE contact_id=? AND deleted_at IS NULL) +
			(SELECT COUNT(*) FROM contact_tags   WHERE contact_id=? AND deleted_at IS NULL)`, cid, cid).Scan(&liveChildren)
		if liveChildren != 0 {
			t.Errorf("owned children not soft-deleted: %d live", liveChildren)
		}
	})
}

func TestContact_CreateRequiresIdentity(t *testing.T) {
	s := newTestStore(t)
	withTx(t, s, func(tx *txAlias) {
		if _, err := s.contacts.Save(tx, "", ContactInput{}, s.Now()); !errors.Is(err, ErrValidation) {
			t.Fatalf("want ErrValidation, got %v", err)
		}
	})
}
