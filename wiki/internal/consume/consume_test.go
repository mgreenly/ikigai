package consume

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"eventplane/consumer"

	"wiki/internal/events"
	"wiki/internal/inbox"
)

// fakeAccepter records the last Accept call and can be told to refuse oversized.
type fakeAccepter struct {
	calls    []acceptCall
	tooLarge bool
	maxBytes int64
}

type acceptCall struct {
	owner, kind, source, mime, title, tags string
	body                                   []byte
}

func (f *fakeAccepter) Accept(_ context.Context, owner, kind, source, mime, title, tags string, body []byte) (inbox.Receipt, error) {
	if f.tooLarge {
		return inbox.Receipt{}, inbox.ErrTooLarge
	}
	f.calls = append(f.calls, acceptCall{owner, kind, source, mime, title, tags, body})
	return inbox.Receipt{ID: "01ARR", SHA256: "h"}, nil
}

func (f *fakeAccepter) MaxBytes() int64 {
	if f.maxBytes == 0 {
		return 131072
	}
	return f.maxBytes
}

type fakeRefuser struct{ refused []events.IngestRefused }

func (r *fakeRefuser) IngestRefused(_ context.Context, ev events.IngestRefused) error {
	r.refused = append(r.refused, ev)
	return nil
}

// TestDomainHandlerAcceptsEvent: a domain event becomes an Accept(kind=event)
// under the system identity, with the envelope JSON as the bytes.
func TestDomainHandlerAcceptsEvent(t *testing.T) {
	in := &fakeAccepter{}
	d := New(in, nil, nil, nil)
	ev := consumer.Event{Type: "contact.created", ID: "01E", Source: "crm", Time: "t", Payload: json.RawMessage(`{"name":"Acme"}`)}
	if err := d.DomainHandler()(context.Background(), ev); err != nil {
		t.Fatalf("handler: %v", err)
	}
	if len(in.calls) != 1 {
		t.Fatalf("want 1 accept, got %d", len(in.calls))
	}
	c := in.calls[0]
	if c.owner != SystemOwner {
		t.Errorf("owner = %q, want system identity", c.owner)
	}
	if c.kind != inbox.KindEvent {
		t.Errorf("kind = %q, want event", c.kind)
	}
	if c.source != "crm:contact.created" {
		t.Errorf("source = %q", c.source)
	}
	// The bytes are the full envelope, including the payload.
	var env map[string]any
	if err := json.Unmarshal(c.body, &env); err != nil {
		t.Fatalf("envelope: %v", err)
	}
	if env["source"] != "crm" || env["type"] != "contact.created" {
		t.Errorf("envelope missing fields: %v", env)
	}
}

// TestDropboxHandlerFetchesContent: a file.created event is fetched over
// content_url and Accepted as a document.
func TestDropboxHandlerFetchesContent(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("file bytes"))
	}))
	defer srv.Close()

	in := &fakeAccepter{}
	d := New(in, nil, srv.Client(), nil)
	payload, _ := json.Marshal(map[string]any{"path": "/notes/x.md", "content_url": srv.URL, "size": 10})
	ev := consumer.Event{Type: "file.created", ID: "01F", Source: "dropbox", Payload: payload}
	if err := d.DropboxHandler()(context.Background(), ev); err != nil {
		t.Fatalf("handler: %v", err)
	}
	if len(in.calls) != 1 {
		t.Fatalf("want 1 accept, got %d", len(in.calls))
	}
	c := in.calls[0]
	if c.kind != inbox.KindDocument {
		t.Errorf("kind = %q, want document", c.kind)
	}
	if string(c.body) != "file bytes" {
		t.Errorf("body = %q, want fetched content", c.body)
	}
	if c.source != "dropbox:/notes/x.md" {
		t.Errorf("source = %q", c.source)
	}
}

// TestDropboxHandlerSkipsDelete: a file.deleted event carries no knowledge — the
// handler advances the cursor (nil) without an Accept.
func TestDropboxHandlerSkipsDelete(t *testing.T) {
	in := &fakeAccepter{}
	d := New(in, nil, nil, nil)
	ev := consumer.Event{Type: "file.deleted", Source: "dropbox", Payload: json.RawMessage(`{}`)}
	if err := d.DropboxHandler()(context.Background(), ev); err != nil {
		t.Errorf("delete should advance (nil), got %v", err)
	}
	if len(in.calls) != 0 {
		t.Errorf("delete should not Accept")
	}
}

// TestCronHandler: a cron tick is Accepted as an event with source cron:<name>.
func TestCronHandler(t *testing.T) {
	in := &fakeAccepter{}
	d := New(in, nil, nil, nil)
	ev := consumer.Event{Type: "cron.daily", ID: "01C", Source: "cron"}
	if err := d.CronHandler("daily")(context.Background(), ev); err != nil {
		t.Fatalf("handler: %v", err)
	}
	if in.calls[0].source != "cron:daily" || in.calls[0].kind != inbox.KindEvent {
		t.Errorf("cron accept = %+v", in.calls[0])
	}
}

// TestOversizedRefusalNotifiesAndSkips: a non-interactive door that hits the cap
// emits wiki.ingest_refused AND returns ErrSkip (advance, never stall forever).
func TestOversizedRefusalNotifiesAndSkips(t *testing.T) {
	in := &fakeAccepter{tooLarge: true}
	refuser := &fakeRefuser{}
	d := New(in, refuser, nil, nil)
	ev := consumer.Event{Type: "contact.created", Source: "crm", Payload: json.RawMessage(`{}`)}
	err := d.DomainHandler()(context.Background(), ev)
	if !errors.Is(err, consumer.ErrSkip) {
		t.Errorf("oversized should ErrSkip, got %v", err)
	}
	if len(refuser.refused) != 1 {
		t.Fatalf("want 1 ingest_refused emit, got %d", len(refuser.refused))
	}
	if refuser.refused[0].Door != "domain" {
		t.Errorf("refused.Door = %q", refuser.refused[0].Door)
	}
}
