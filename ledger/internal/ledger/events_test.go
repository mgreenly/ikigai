package ledger

import (
	"bufio"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"eventplane/outbox"
)

// mkSvcWithOutbox wires a real event-plane producer over the test DB. DBPath is
// empty so the startup probe is skipped (independent :memory: handles cannot
// exercise it) and the generation token is ephemeral.
func mkSvcWithOutbox(t *testing.T) *Service {
	t.Helper()
	conn := openDB(t)
	ob, err := outbox.New(conn, outbox.Options{Source: "ledger", Registry: Events})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	s := NewService(conn)
	s.Outbox = NewOutboxProducer(ob)
	return s
}

func outboxRows(t *testing.T, s *Service) []struct {
	Kind    string
	Subject string
	Payload string
} {
	t.Helper()
	rows, err := s.DB.Query(`SELECT kind, subject, payload FROM outbox ORDER BY seq`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	var out []struct {
		Kind    string
		Subject string
		Payload string
	}
	for rows.Next() {
		var r struct {
			Kind    string
			Subject string
			Payload string
		}
		if err := rows.Scan(&r.Kind, &r.Subject, &r.Payload); err != nil {
			t.Fatal(err)
		}
		out = append(out, r)
	}
	return out
}

func TestRecord_EmitsRecorded(t *testing.T) {
	s := mkSvcWithOutbox(t)
	tx := record(t, s, "2026-06-01", "Acme — June hosting",
		leg("Assets:Receivable:Acme", i64(5000)),
		leg("Income:Hosting", nil),
	)
	rows := outboxRows(t, s)
	if len(rows) != 1 {
		t.Fatalf("outbox rows = %d, want 1", len(rows))
	}
	if rows[0].Kind != kindRecorded || rows[0].Subject != "" {
		t.Errorf("address = (%q, %q), want (recorded, empty)", rows[0].Kind, rows[0].Subject)
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

func TestRecordAndReverse_UseSubjectlessRecordedAddressAtomically(t *testing.T) {
	// R-FXKF-JD3L
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
	// Every committed transaction emits exactly one recorded event — the
	// original and the reversal mirror both.
	if len(rows) != 2 {
		t.Fatalf("outbox rows = %d, want 2", len(rows))
	}
	for i, row := range rows {
		if row.Kind != kindRecorded || row.Subject != "" {
			t.Errorf("row %d address = (%q, %q), want (recorded, empty)", i, row.Kind, row.Subject)
		}
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
	tx, err := s.DB.Begin()
	if err != nil {
		t.Fatal(err)
	}
	if err := s.Outbox.AppendRecorded(tx, orig); err != nil {
		t.Fatalf("append in rollback tx: %v", err)
	}
	if err := tx.Rollback(); err != nil {
		t.Fatal(err)
	}
	if got := len(outboxRows(t, s)); got != 2 {
		t.Errorf("outbox rows after rollback = %d, want 2", got)
	}
}

func TestRecordedEventsCarryExternalRef(t *testing.T) {
	// R-FV4M-RTM7
	s := mkSvcWithOutbox(t)
	ref := "gmail:message-id"
	orig, err := s.Record(context.Background(), RecordInput{Date: "2026-06-01", Description: "x", ExternalRef: &ref, Postings: []PostingInput{leg("Assets:Bank", i64(100)), leg("Income:Hosting", i64(-100))}})
	if err != nil {
		t.Fatal(err)
	}
	mirror, err := s.Reverse(context.Background(), orig.ID, nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	rows := outboxRows(t, s)
	var first, second struct {
		ExternalRef *string `json:"external_ref"`
	}
	if err := json.Unmarshal([]byte(rows[0].Payload), &first); err != nil {
		t.Fatal(err)
	}
	if err := json.Unmarshal([]byte(rows[1].Payload), &second); err != nil {
		t.Fatal(err)
	}
	if first.ExternalRef == nil || *first.ExternalRef != ref {
		t.Errorf("original event ref = %v", first.ExternalRef)
	}
	if second.ExternalRef != nil {
		t.Errorf("mirror %s inherited ref %v", mirror.ID, second.ExternalRef)
	}
}

func TestFeedHandlerFramesSubjectlessRecordedEnvelope(t *testing.T) {
	// R-G2G1-2G2D
	s := mkSvcWithOutbox(t)
	record(t, s, "2026-06-01", "Acme — June hosting",
		leg("Assets:Receivable:Acme", i64(5000)),
		leg("Income:Hosting", nil),
	)
	producer := s.Outbox.(*outboxProducer)
	server := httptest.NewServer(producer.ob.FeedHandler())
	defer server.Close()

	res, err := http.Get(server.URL)
	if err != nil {
		t.Fatalf("GET feed: %v", err)
	}
	defer res.Body.Close()
	if res.StatusCode != http.StatusOK {
		t.Fatalf("feed status = %d", res.StatusCode)
	}

	scanner := bufio.NewScanner(res.Body)
	var event, data string
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "event: ") {
			event = strings.TrimPrefix(line, "event: ")
		}
		if strings.HasPrefix(line, "data: ") && event == "ledger:recorded" {
			data = strings.TrimPrefix(line, "data: ")
			break
		}
	}
	if err := scanner.Err(); err != nil {
		t.Fatalf("read feed: %v", err)
	}
	if event != "ledger:recorded" || data == "" {
		t.Fatalf("SSE frame = event %q data %q", event, data)
	}
	var envelope map[string]any
	if err := json.Unmarshal([]byte(data), &envelope); err != nil {
		t.Fatalf("decode envelope: %v", err)
	}
	if envelope["source"] != "ledger" || envelope["kind"] != kindRecorded || envelope["subject"] != "" {
		t.Errorf("envelope address = %v", envelope)
	}
	if _, ok := envelope["type"]; ok {
		t.Errorf("envelope unexpectedly has type: %v", envelope)
	}
}
