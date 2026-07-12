package outbox

import (
	"bufio"
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

// sseConn is a minimal test client for the SSE feed: it reads complete frames
// (text between blank lines) off the wire and delivers them on a channel.
type sseConn struct {
	resp   *http.Response
	cancel context.CancelFunc
	frames chan string
}

func dialFeed(t *testing.T, url string, header http.Header) *sseConn {
	t.Helper()
	ctx, cancel := context.WithCancel(context.Background())
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		cancel()
		t.Fatalf("new request: %v", err)
	}
	req.Header = header
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		cancel()
		t.Fatalf("dial: %v", err)
	}
	c := &sseConn{resp: resp, cancel: cancel, frames: make(chan string, 64)}
	go c.read()
	t.Cleanup(c.close)
	return c
}

func (c *sseConn) read() {
	defer close(c.frames)
	sc := bufio.NewScanner(c.resp.Body)
	sc.Buffer(make([]byte, 64*1024), 1<<20)
	var cur []string
	for sc.Scan() {
		line := sc.Text()
		if line == "" {
			if len(cur) > 0 {
				c.frames <- strings.Join(cur, "\n")
				cur = nil
			}
			continue
		}
		cur = append(cur, line)
	}
}

// next returns the next frame, skipping keepalive comments, or fails on timeout.
func (c *sseConn) next(t *testing.T) string {
	t.Helper()
	for {
		select {
		case f, ok := <-c.frames:
			if !ok {
				t.Fatal("stream closed before expected frame")
			}
			if strings.HasPrefix(f, ":") { // keepalive comment
				continue
			}
			return f
		case <-time.After(2 * time.Second):
			t.Fatal("timed out waiting for a frame")
		}
	}
}

func (c *sseConn) close() {
	c.cancel()
	c.resp.Body.Close()
}

func eventOf(frame string) string { return field(frame, "event: ") }
func dataOf(frame string) string  { return field(frame, "data: ") }
func idOf(frame string) string    { return field(frame, "id: ") }
func field(frame, prefix string) string {
	for _, line := range strings.Split(frame, "\n") {
		if strings.HasPrefix(line, prefix) {
			return strings.TrimPrefix(line, prefix)
		}
	}
	return ""
}

func feedServer(t *testing.T, o *Outbox) string {
	t.Helper()
	srv := httptest.NewServer(o.FeedHandler())
	t.Cleanup(srv.Close)
	return srv.URL
}

func appendAddress(t *testing.T, o *Outbox, db interface {
	Begin() (*sql.Tx, error)
}, kind, subject string) {
	t.Helper()
	tx, err := db.Begin()
	if err != nil {
		t.Fatal(err)
	}
	if err := o.Append(tx, Event{Kind: kind, Subject: subject, Payload: json.RawMessage(`{"n":1}`)}); err != nil {
		t.Fatal(err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatal(err)
	}
	o.Ring()
}

func TestFeedEnvelopeCanonicalKeyAndStableReplay(t *testing.T) {
	o, db := newMemOutbox(t, func(opts *Options) { opts.Source = "dropbox" })
	appendAddress(t, o, db, "create", "/bills/a.pdf")
	url := feedServer(t, o)

	firstConn := dialFeed(t, url, http.Header{})
	if f := firstConn.next(t); eventOf(f) != "status" {
		t.Fatalf("want status, got %q", f)
	}
	first := firstConn.next(t)
	var env map[string]json.RawMessage
	if err := json.Unmarshal([]byte(dataOf(first)), &env); err != nil {
		t.Fatal(err)
	}
	wantKeys := []string{"id", "source", "time", "kind", "subject", "payload"}
	// R-3D34-SZYT
	if len(env) != len(wantKeys) {
		t.Fatalf("envelope keys = %v", env)
	}
	for _, key := range wantKeys {
		if _, ok := env[key]; !ok {
			t.Fatalf("envelope missing %q: %v", key, env)
		}
	}
	// R-3EB1-6RPI
	if eventOf(first) != "dropbox:create/bills/a.pdf" {
		t.Fatalf("event key = %q", eventOf(first))
	}

	secondConn := dialFeed(t, url, http.Header{})
	_ = secondConn.next(t) // status
	second := secondConn.next(t)
	// R-42P0-U6JE
	if second != first {
		t.Fatalf("replay changed:\nfirst:  %s\nsecond: %s", first, second)
	}
}

func TestFeedCanonicalKeyWithoutSubject(t *testing.T) {
	o, db := newMemOutbox(t, func(opts *Options) { opts.Source = "ledger" })
	appendAddress(t, o, db, "recorded", "")
	c := dialFeed(t, feedServer(t, o), http.Header{})
	_ = c.next(t) // status
	// R-3EB1-6RPI
	if got := eventOf(c.next(t)); got != "ledger:recorded" {
		t.Fatalf("event key = %q", got)
	}
}

func TestFeed_FromBeginning(t *testing.T) {
	o, db := newMemOutbox(t)
	appendOne(t, o, db, "contact.created")
	appendOne(t, o, db, "contact.created")
	url := feedServer(t, o)

	c := dialFeed(t, url, http.Header{})
	// behind=2 status, then 2 events, then caught-up.
	if f := c.next(t); eventOf(f) != "status" {
		t.Fatalf("want status frame, got %q", f)
	}
	for i := 0; i < 2; i++ {
		f := c.next(t)
		if eventOf(f) != "crm:contact.created" {
			t.Fatalf("event %d: want contact.created, got %q", i, f)
		}
		if idOf(f) == "" {
			t.Fatalf("event %d: missing id: cursor line", i)
		}
	}
	if f := c.next(t); eventOf(f) != "caught-up" || dataOf(f) != "{}" {
		t.Fatalf("want caught-up {}, got %q", f)
	}
}

func TestFeed_FromTail_DeliversLiveEvent(t *testing.T) {
	o, db := newMemOutbox(t)
	appendOne(t, o, db, "contact.created") // pre-existing; tail must skip it
	url := feedServer(t, o)

	c := dialFeed(t, url+"?from=tail", http.Header{})
	if f := c.next(t); eventOf(f) != "caught-up" {
		t.Fatalf("tail should start caught-up, got %q", f)
	}
	// A live event after caught-up must arrive via the doorbell.
	appendOne(t, o, db, "contact.created")
	f := c.next(t)
	if eventOf(f) != "crm:contact.created" {
		t.Fatalf("want live event, got %q", f)
	}
}

func TestFeed_ResumeAfterCursor(t *testing.T) {
	o, db := newMemOutbox(t)
	appendOne(t, o, db, "contact.created") // seq 1
	appendOne(t, o, db, "contact.created") // seq 2
	url := feedServer(t, o)

	hdr := http.Header{"Last-Event-ID": {makeCursor(o.Generation(), 1)}}
	c := dialFeed(t, url, hdr)
	// behind=1 status, then only seq 2, then caught-up.
	if f := c.next(t); eventOf(f) != "status" {
		t.Fatalf("want status, got %q", f)
	}
	f := c.next(t)
	if eventOf(f) != "crm:contact.created" || idOf(f) != makeCursor(o.Generation(), 2) {
		t.Fatalf("want seq-2 event, got %q", f)
	}
	if f := c.next(t); eventOf(f) != "caught-up" {
		t.Fatalf("want caught-up, got %q", f)
	}
}

func TestFeed_ResyncStaleEpoch(t *testing.T) {
	o, db := newMemOutbox(t)
	appendOne(t, o, db, "contact.created")
	url := feedServer(t, o)
	hdr := http.Header{"Last-Event-ID": {makeCursor("SOMEOTHEREPOCH000000000000", 1)}}
	c := dialFeed(t, url, hdr)
	assertResync(t, c.next(t), reasonStaleEpoch)
}

func TestFeed_ResyncUnintelligible(t *testing.T) {
	o, db := newMemOutbox(t)
	appendOne(t, o, db, "contact.created")
	url := feedServer(t, o)
	hdr := http.Header{"Last-Event-ID": {"this-is-not-a-valid-cursor"}}
	c := dialFeed(t, url, hdr)
	assertResync(t, c.next(t), reasonUnintelligible)
}

func TestFeed_ResyncDiverged(t *testing.T) {
	o, db := newMemOutbox(t)
	appendOne(t, o, db, "contact.created") // head = 1
	url := feedServer(t, o)
	hdr := http.Header{"Last-Event-ID": {makeCursor(o.Generation(), 99)}} // ahead of head
	c := dialFeed(t, url, hdr)
	assertResync(t, c.next(t), reasonDiverged)
}

func TestFeed_ResyncPastHorizon(t *testing.T) {
	base := time.Date(2026, 1, 1, 0, 0, 0, 0, time.UTC)
	o, db := newMemOutbox(t, func(opt *Options) {
		opt.RetentionMaxRows = 3
		opt.RetentionDays = 7
		opt.Now = func() time.Time { return base }
	})
	for i := 0; i < 10; i++ {
		appendOne(t, o, db, "contact.created")
	}
	o.now = func() time.Time { return base.Add(30 * 24 * time.Hour) }
	if err := o.Trim(context.Background()); err != nil {
		t.Fatalf("trim: %v", err)
	}
	// Newest 3 kept -> min seq is 8. A cursor at seq 1 lost events 2..7.
	url := feedServer(t, o)
	hdr := http.Header{"Last-Event-ID": {makeCursor(o.Generation(), 1)}}
	c := dialFeed(t, url, hdr)
	assertResync(t, c.next(t), reasonPastHorizon)
}

func TestFeed_RejectsIdentityHeaders(t *testing.T) {
	o, _ := newMemOutbox(t)
	url := feedServer(t, o)

	for _, h := range []http.Header{
		{"X-Owner-Email": {"owner@example.com"}},
		{"X-Forwarded-Proto": {"https"}},
	} {
		req, _ := http.NewRequest(http.MethodGet, url, nil)
		req.Header = h
		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatalf("request: %v", err)
		}
		resp.Body.Close()
		if resp.StatusCode != http.StatusNotFound {
			t.Fatalf("identity-header request should 404, got %d for %v", resp.StatusCode, h)
		}
	}
}

func assertResync(t *testing.T, frame, wantReason string) {
	t.Helper()
	if eventOf(frame) != "resync" {
		t.Fatalf("want resync frame, got %q", frame)
	}
	if !strings.Contains(dataOf(frame), `"reason":"`+wantReason+`"`) {
		t.Fatalf("want reason %q, got data %q", wantReason, dataOf(frame))
	}
}
