package ledger

import (
	"context"
	"encoding/json"
	"testing"

	"eventplane/outbox"
)

// mkSvcWithOutbox wires a real event-plane producer over the test DB. DBPath is
// empty so the startup probe is skipped (independent :memory: handles cannot
// exercise it) and the generation token is ephemeral.
func mkSvcWithOutbox(t *testing.T) *Service {
	t.Helper()
	conn := openDB(t)
	ob, err := outbox.New(conn, outbox.Options{Source: "ledger"})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	s := NewService(conn)
	s.Outbox = NewOutboxProducer(ob)
	return s
}

func outboxRows(t *testing.T, s *Service) []struct {
	Type    string
	Payload string
} {
	t.Helper()
	rows, err := s.DB.Query(`SELECT type, payload FROM outbox ORDER BY seq`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	var out []struct {
		Type    string
		Payload string
	}
	for rows.Next() {
		var r struct {
			Type    string
			Payload string
		}
		if err := rows.Scan(&r.Type, &r.Payload); err != nil {
			t.Fatal(err)
		}
		out = append(out, r)
	}
	return out
}

func TestRecord_EmitsTransactionRecorded(t *testing.T) {
	s := mkSvcWithOutbox(t)
	tx := record(t, s, "2026-06-01", "Acme — June hosting",
		leg("Assets:Receivable:Acme", i64(5000)),
		leg("Income:Hosting", nil),
	)
	rows := outboxRows(t, s)
	if len(rows) != 1 {
		t.Fatalf("outbox rows = %d, want 1", len(rows))
	}
	if rows[0].Type != "transaction.recorded" {
		t.Errorf("type = %q, want transaction.recorded", rows[0].Type)
	}
	var p struct {
		ID         string  `json:"id"`
		ReversesID *string `json:"reverses_id"`
		Postings   []struct {
			Account     string `json:"account"`
			AmountCents int64  `json:"amount_cents"`
		} `json:"postings"`
	}
	if err := json.Unmarshal([]byte(rows[0].Payload), &p); err != nil {
		t.Fatalf("payload: %v", err)
	}
	if p.ID != tx.ID || len(p.Postings) != 2 || p.ReversesID != nil {
		t.Errorf("payload mismatch: %+v", p)
	}
	if p.Postings[1].AmountCents != -5000 {
		t.Errorf("payload residual = %d, want -5000", p.Postings[1].AmountCents)
	}
}

func TestReverse_AlsoEmitsRecorded_WithReversesID(t *testing.T) {
	s := mkSvcWithOutbox(t)
	orig := record(t, s, "2026-06-01", "x",
		leg("Assets:Bank:Checking", i64(100)),
		leg("Income:Hosting", i64(-100)),
	)
	mirror, err := s.Reverse(context.Background(), orig.ID, nil, nil)
	if err != nil {
		t.Fatalf("reverse: %v", err)
	}
	rows := outboxRows(t, s)
	// Every committed transaction emits exactly one transaction.recorded — the
	// original and the reversal mirror both.
	if len(rows) != 2 {
		t.Fatalf("outbox rows = %d, want 2", len(rows))
	}
	var p struct {
		ID         string  `json:"id"`
		ReversesID *string `json:"reverses_id"`
	}
	if err := json.Unmarshal([]byte(rows[1].Payload), &p); err != nil {
		t.Fatal(err)
	}
	if p.ID != mirror.ID {
		t.Errorf("second event id = %s, want mirror %s", p.ID, mirror.ID)
	}
	if p.ReversesID == nil || *p.ReversesID != orig.ID {
		t.Errorf("mirror event reverses_id = %v, want %s", p.ReversesID, orig.ID)
	}
}
