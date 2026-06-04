package crm

import (
	"context"
	"encoding/json"
	"testing"

	"eventplane/outbox"
)

// withOutbox wires a real outbox.Outbox over the service's own SQLite handle —
// one file holds both the domain tables and the outbox table (003_outbox.sql is
// applied by newTestStore's migration), which is exactly what makes the atomic
// outbox legal (PLAN.md §5, §6). DBPath/GenerationPath are left empty: the
// startup probe is skipped and an ephemeral in-process generation token is
// minted (the test only asserts the committed outbox rows, not the feed/epoch).
func withOutbox(t *testing.T, s *Service) *Service {
	t.Helper()
	ob, err := outbox.New(s.DB, outbox.Options{Source: "crm"})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	s.Outbox = ob
	return s
}

type outboxRow struct {
	typ     string
	payload []byte
}

// readOutbox returns the committed outbox rows in seq order — proving the
// Append happened on the tx that committed (PLAN.md §6).
func readOutbox(t *testing.T, s *Service) []outboxRow {
	t.Helper()
	rows, err := s.DB.Query(`SELECT type, payload FROM outbox ORDER BY seq ASC`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	var out []outboxRow
	for rows.Next() {
		var r outboxRow
		if err := rows.Scan(&r.typ, &r.payload); err != nil {
			t.Fatalf("scan outbox: %v", err)
		}
		out = append(out, r)
	}
	if err := rows.Err(); err != nil {
		t.Fatalf("rows: %v", err)
	}
	return out
}

func eventTypes(rows []outboxRow) []string {
	types := make([]string, len(rows))
	for i, r := range rows {
		types[i] = r.typ
	}
	return types
}

// TestEventContactCreatedWithTags: creating a contact with tags emits exactly a
// contact.created snapshot plus one contact.tagged per tag, committed to the
// outbox table on the same tx as the domain write.
func TestEventContactCreatedWithTags(t *testing.T) {
	s := withOutbox(t, newTestStore(t))
	ctx := context.Background()

	sum, err := s.Save(ctx, "contact", "", []byte(`{
		"given_name": "Bob",
		"family_name": "Jones",
		"lifecycle": "subscriber",
		"emails": [{"email": "bob@example.com"}],
		"tags": ["newsletter", "vip"]
	}`), false)
	if err != nil {
		t.Fatalf("Save create: %v", err)
	}

	rows := readOutbox(t, s)
	if len(rows) != 3 {
		t.Fatalf("expected 3 events (created + 2 tagged), got %d: %v", len(rows), eventTypes(rows))
	}
	if rows[0].typ != "contact.created" {
		t.Fatalf("first event = %q, want contact.created", rows[0].typ)
	}
	tagged := map[string]bool{}
	for _, r := range rows[1:] {
		if r.typ != "contact.tagged" {
			t.Fatalf("tag event = %q, want contact.tagged", r.typ)
		}
		var p contactTagPayload
		if err := json.Unmarshal(r.payload, &p); err != nil {
			t.Fatalf("unmarshal tag payload: %v", err)
		}
		if p.ContactID != sum.ID {
			t.Fatalf("tag event contact_id = %q, want %q", p.ContactID, sum.ID)
		}
		tagged[p.Tag] = true
	}
	if !tagged["newsletter"] || !tagged["vip"] {
		t.Fatalf("tagged events = %v, want newsletter + vip", tagged)
	}

	// The created snapshot carries the funnel/identity fields.
	var snap contactSnapshotPayload
	if err := json.Unmarshal(rows[0].payload, &snap); err != nil {
		t.Fatalf("unmarshal created payload: %v", err)
	}
	if snap.ID != sum.ID {
		t.Fatalf("created id = %q, want %q", snap.ID, sum.ID)
	}
	if snap.Lifecycle != "subscriber" {
		t.Fatalf("created lifecycle = %q, want subscriber", snap.Lifecycle)
	}
	if len(snap.Emails) != 1 || snap.Emails[0].Email != "bob@example.com" || !snap.Emails[0].Primary {
		t.Fatalf("created emails = %+v, want one primary bob@example.com", snap.Emails)
	}
	if len(snap.Tags) != 2 {
		t.Fatalf("created snapshot tags = %v, want 2", snap.Tags)
	}
}

// TestEventContactUpdatedTagDelta: updating a contact's tag set emits
// contact.updated plus a tagged event for the added tag and an untagged event
// for the removed tag (the declarative-replace diff, PLAN.md §4/§6).
func TestEventContactUpdatedTagDelta(t *testing.T) {
	s := withOutbox(t, newTestStore(t))
	ctx := context.Background()

	created, err := s.Save(ctx, "contact", "", []byte(`{
		"display_name": "Carol",
		"tags": ["newsletter"]
	}`), false)
	if err != nil {
		t.Fatalf("Save create: %v", err)
	}

	// Drop "newsletter", add "vip": the declarative set is the complete desired set.
	if _, err := s.Save(ctx, "contact", created.ID, []byte(`{"tags": ["vip"]}`), false); err != nil {
		t.Fatalf("Save update: %v", err)
	}

	rows := readOutbox(t, s)
	// create: contact.created + contact.tagged(newsletter)
	// update: contact.updated + contact.tagged(vip) + contact.untagged(newsletter)
	got := eventTypes(rows)
	if len(rows) != 5 {
		t.Fatalf("expected 5 events, got %d: %v", len(rows), got)
	}
	if got[2] != "contact.updated" {
		t.Fatalf("first update event = %q, want contact.updated", got[2])
	}

	var taggedTags, untaggedTags []string
	for _, r := range rows[3:] {
		var p contactTagPayload
		if err := json.Unmarshal(r.payload, &p); err != nil {
			t.Fatalf("unmarshal: %v", err)
		}
		switch r.typ {
		case "contact.tagged":
			taggedTags = append(taggedTags, p.Tag)
		case "contact.untagged":
			untaggedTags = append(untaggedTags, p.Tag)
		default:
			t.Fatalf("unexpected update tag event %q", r.typ)
		}
	}
	if len(taggedTags) != 1 || taggedTags[0] != "vip" {
		t.Fatalf("update tagged = %v, want [vip]", taggedTags)
	}
	if len(untaggedTags) != 1 || untaggedTags[0] != "newsletter" {
		t.Fatalf("update untagged = %v, want [newsletter]", untaggedTags)
	}
}

// TestEventNilOutboxNoEmit: a Save with a nil Outbox writes the domain row and
// emits nothing (the guard keeps non-event-plane callers working).
func TestEventNilOutboxNoEmit(t *testing.T) {
	s := newTestStore(t) // no Outbox wired
	ctx := context.Background()
	if _, err := s.Save(ctx, "contact", "", []byte(`{"display_name": "Dave", "tags": ["x"]}`), false); err != nil {
		t.Fatalf("Save: %v", err)
	}
	var n int
	if err := s.DB.QueryRow(`SELECT COUNT(*) FROM outbox`).Scan(&n); err != nil {
		t.Fatalf("count outbox: %v", err)
	}
	if n != 0 {
		t.Fatalf("expected 0 outbox rows with nil Outbox, got %d", n)
	}
}
