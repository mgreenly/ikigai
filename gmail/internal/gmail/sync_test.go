package gmail

import (
	"bufio"
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	appkitdatabase "appkit/db"
	"gmail/internal/db"

	"eventplane/outbox"
)

// ── test harness ─────────────────────────────────────────────────────────────

// fakeClient is an injected gmailAPI returning canned profile/history/message
// responses, so the engine's derivation + atomic-advance rules are exercised
// deterministically without live Gmail.
type fakeClient struct {
	profileHistoryID string
	profileErr       error

	// pages keyed by pageToken ("" is the first page). Each call to HistoryList
	// returns the page for the given pageToken.
	pages      map[string]HistoryListResult
	historyErr error // when set, HistoryList returns it (e.g. ErrNotFound for stale)

	// messages keyed by id, returned by MessageGet.
	messages map[string]Message
	getErr   map[string]error

	profileCalls int
}

func (f *fakeClient) GetProfile(ctx context.Context) (Profile, error) {
	f.profileCalls++
	if f.profileErr != nil {
		return Profile{}, f.profileErr
	}
	return Profile{EmailAddress: "owner@example.com", HistoryID: f.profileHistoryID}, nil
}

func (f *fakeClient) HistoryList(ctx context.Context, startHistoryID, pageToken string) (HistoryListResult, error) {
	if f.historyErr != nil {
		return HistoryListResult{}, f.historyErr
	}
	return f.pages[pageToken], nil
}

func (f *fakeClient) MessageGet(ctx context.Context, id, format string) (Message, error) {
	if f.getErr != nil {
		if err, ok := f.getErr[id]; ok {
			return Message{}, err
		}
	}
	m, ok := f.messages[id]
	if !ok {
		return Message{}, ErrNotFound
	}
	return m, nil
}

func openTestDB(t *testing.T) *sql.DB {
	t.Helper()
	conn, err := sql.Open("sqlite", "file:"+t.TempDir()+"/gmail.db?_pragma=foreign_keys(ON)")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	conn.SetMaxOpenConns(1)
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdatabase.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdatabase.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return conn
}

// newHarness builds an engine over a temp DB wired to a REAL eventplane outbox
// (so the atomic emit+advance is exercised against the actual Append path and
// the registry validation), plus the fake client.
func newHarness(t *testing.T, fc *fakeClient) (*Engine, *sql.DB, *outbox.Outbox) {
	t.Helper()
	conn := openTestDB(t)
	tmp := t.TempDir()
	ob, err := outbox.New(conn, outbox.Options{
		Source:         "gmail",
		DBPath:         tmp + "/gmail.db",
		GenerationPath: tmp + "/generation",
		Registry:       mailRegistry(),
	})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	eng := NewEngine(EngineOptions{
		DB:     conn,
		Store:  NewStore(),
		Client: fc,
		Sink:   NewOutboxProducer(ob),
	})
	return eng, conn, ob
}

// mailRegistry mirrors the three published mail.* types so the real outbox
// accepts the engine's Appends (the production registry lives in internal/mcp;
// this keeps the test self-contained without an import cycle).
func mailRegistry() outbox.Registry {
	return outbox.Registry{
		{Kind: KindReceived, Subject: "", Description: "x", Sample: mailReceivedPayload{}},
		{Kind: KindSent, Subject: "", Description: "x", Sample: mailSentPayload{}},
		{Kind: KindDeleted, Subject: "", Description: "x", Sample: mailDeletedPayload{}},
	}
}

// storedCursor reads the persisted historyId.
func storedCursor(t *testing.T, conn *sql.DB) (string, bool) {
	t.Helper()
	tx, err := conn.BeginTx(context.Background(), &sql.TxOptions{ReadOnly: true})
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	defer tx.Rollback()
	id, ok, err := (Store{}).GetHistoryID(tx)
	if err != nil {
		t.Fatalf("get cursor: %v", err)
	}
	return id, ok
}

// outboxRows returns every (kind, subject, payload) row in the outbox table in seq order.
func outboxRows(t *testing.T, conn *sql.DB) []struct {
	Kind    string
	Subject string
	Payload map[string]any
} {
	t.Helper()
	rows, err := conn.Query(`SELECT kind, subject, payload FROM outbox ORDER BY seq ASC`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	var out []struct {
		Kind    string
		Subject string
		Payload map[string]any
	}
	for rows.Next() {
		var kind, subject, payload string
		if err := rows.Scan(&kind, &subject, &payload); err != nil {
			t.Fatalf("scan: %v", err)
		}
		m := map[string]any{}
		if err := json.Unmarshal([]byte(payload), &m); err != nil {
			t.Fatalf("unmarshal payload: %v", err)
		}
		out = append(out, struct {
			Kind    string
			Subject string
			Payload map[string]any
		}{kind, subject, m})
	}
	return out
}

func msg(id, threadID string, labels []string, headers map[string]string, snippet, internalDate string) Message {
	var hs []Header
	for k, v := range headers {
		hs = append(hs, Header{Name: k, Value: v})
	}
	return Message{
		ID:           id,
		ThreadID:     threadID,
		LabelIDs:     labels,
		Snippet:      snippet,
		InternalDate: internalDate,
		Payload:      MessagePart{Headers: hs},
	}
}

// added/labeled history record builders.
func added(ids ...string) []HistoryMessageChange {
	var out []HistoryMessageChange
	for _, id := range ids {
		out = append(out, HistoryMessageChange{Message: MessageRef{ID: id, ThreadID: "t-" + id}})
	}
	return out
}

func labeled(id string, labelIDs ...string) HistoryLabelChange {
	return HistoryLabelChange{Message: MessageRef{ID: id, ThreadID: "t-" + id}, LabelIDs: labelIDs}
}

// ── fresh boot: seed, emit nothing ───────────────────────────────────────────

func TestFreshBootSeedsCursorAndEmitsNothing(t *testing.T) {
	fc := &fakeClient{profileHistoryID: "1000"}
	eng, conn, _ := newHarness(t, fc)

	if err := eng.bootstrap(context.Background()); err != nil {
		t.Fatalf("bootstrap: %v", err)
	}
	id, ok := storedCursor(t, conn)
	if !ok || id != "1000" {
		t.Fatalf("want seeded cursor 1000, got %q ok=%v", id, ok)
	}
	if rows := outboxRows(t, conn); len(rows) != 0 {
		t.Fatalf("fresh boot must emit nothing, got %v", rows)
	}
	if fc.profileCalls != 1 {
		t.Fatalf("want 1 getProfile, got %d", fc.profileCalls)
	}
}

// ── derivation table: received / sent / send-to-self / deleted / draft ────────

func TestPollDerivesEventsAndAdvancesCursorAtomically(t *testing.T) {
	tests := []struct {
		name      string
		history   []History
		messages  map[string]Message
		newHistID string
		want      []string // expected event types in order
	}{
		{
			name: "messagesAdded INBOX -> received",
			history: []History{{
				ID:            "1001",
				MessagesAdded: added("m-recv"),
			}},
			messages: map[string]Message{
				"m-recv": msg("m-recv", "t-1", []string{LabelInbox, LabelUnread},
					map[string]string{"From": "alice@example.com", "Subject": "Hi"}, "snip", "1700000000000"),
			},
			newHistID: "1100",
			want:      []string{KindReceived},
		},
		{
			name: "messagesAdded SENT (not INBOX) -> sent",
			history: []History{{
				ID:            "1002",
				MessagesAdded: added("m-sent"),
			}},
			messages: map[string]Message{
				"m-sent": msg("m-sent", "t-2", []string{LabelSent},
					map[string]string{"To": "bob@example.com", "Subject": "Out"}, "snip", "1700000001000"),
			},
			newHistID: "1100",
			want:      []string{KindSent},
		},
		{
			// Gmail realizes a send-to-self as ONE message carrying BOTH SENT and
			// INBOX (verified live). Both events must derive from that one message.
			name: "send-to-self: one message with SENT+INBOX -> both events",
			history: []History{{
				ID:            "1003",
				MessagesAdded: added("m-self"),
			}},
			messages: map[string]Message{
				"m-self": msg("m-self", "t-3", []string{LabelUnread, LabelSent, LabelInbox},
					map[string]string{"To": "me@example.com", "From": "me@example.com", "Subject": "self"}, "s", "1700000002000"),
			},
			newHistID: "1100",
			want:      []string{KindSent, KindReceived},
		},
		{
			name: "labelsAdded TRASH -> deleted",
			history: []History{{
				ID:          "1004",
				LabelsAdded: []HistoryLabelChange{labeled("m-del", LabelTrash)},
			}},
			messages: map[string]Message{
				"m-del": msg("m-del", "t-4", []string{LabelTrash}, map[string]string{"Subject": "bye"}, "", "1700000003000"),
			},
			newHistID: "1100",
			want:      []string{KindDeleted},
		},
		{
			name: "labelsAdded non-TRASH -> nothing",
			history: []History{{
				ID:          "1005",
				LabelsAdded: []HistoryLabelChange{labeled("m-x", LabelUnread)},
			}},
			messages:  map[string]Message{},
			newHistID: "1100",
			want:      nil,
		},
		{
			name: "messagesAdded draft (no INBOX/SENT) -> nothing",
			history: []History{{
				ID:            "1006",
				MessagesAdded: added("m-draft"),
			}},
			messages: map[string]Message{
				"m-draft": msg("m-draft", "t-6", []string{"DRAFT"}, map[string]string{"Subject": "wip"}, "", "1700000004000"),
			},
			newHistID: "1100",
			want:      nil,
		},
		{
			name: "duplicate message across records emits once",
			history: []History{
				{ID: "1007", MessagesAdded: added("m-dup")},
				{ID: "1008", MessagesAdded: added("m-dup")},
			},
			messages: map[string]Message{
				"m-dup": msg("m-dup", "t-7", []string{LabelInbox}, map[string]string{"From": "a@b.c", "Subject": "dup"}, "s", "1700000005000"),
			},
			newHistID: "1100",
			want:      []string{KindReceived},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			fc := &fakeClient{
				profileHistoryID: "1000",
				pages:            map[string]HistoryListResult{"": {History: tc.history, HistoryID: tc.newHistID}},
				messages:         tc.messages,
			}
			eng, conn, _ := newHarness(t, fc)

			// Seed the cursor to a known starting point.
			if err := eng.setCursor(context.Background(), "1000"); err != nil {
				t.Fatalf("seed: %v", err)
			}

			if err := eng.Poll(context.Background()); err != nil {
				t.Fatalf("poll: %v", err)
			}

			rows := outboxRows(t, conn)
			var gotTypes []string
			for _, r := range rows {
				gotTypes = append(gotTypes, r.Kind)
			}
			if !equalSlices(gotTypes, tc.want) {
				t.Fatalf("event types: want %v, got %v", tc.want, gotTypes)
			}

			// Cursor advanced to the page's historyId atomically with emission.
			id, ok := storedCursor(t, conn)
			if !ok || id != tc.newHistID {
				t.Fatalf("cursor: want %q, got %q ok=%v", tc.newHistID, id, ok)
			}
		})
	}
}

func TestPollWritesSubjectlessRoutingKindsWithCursorAdvance(t *testing.T) {
	// R-X6YL-1Y77
	fc := &fakeClient{
		profileHistoryID: "1000",
		pages: map[string]HistoryListResult{"": {
			History: []History{
				{ID: "1001", MessagesAdded: added("inbound")},
				{ID: "1002", MessagesAdded: added("outbound")},
				{ID: "1003", LabelsAdded: []HistoryLabelChange{labeled("trashed", LabelTrash)}},
			},
			HistoryID: "2000",
		}},
		messages: map[string]Message{
			"inbound":  msg("inbound", "t-in", []string{LabelInbox}, map[string]string{"From": "alice@example.com", "Subject": "hello"}, "hi", "1700000000000"),
			"outbound": msg("outbound", "t-out", []string{LabelSent}, map[string]string{"To": "bob@example.com", "Subject": "reply"}, "hello", "1700000001000"),
			"trashed":  msg("trashed", "t-trash", []string{LabelTrash}, map[string]string{"Subject": "old"}, "", "1700000002000"),
		},
	}
	eng, conn, _ := newHarness(t, fc)
	if err := eng.setCursor(context.Background(), "1000"); err != nil {
		t.Fatalf("seed: %v", err)
	}
	if err := eng.Poll(context.Background()); err != nil {
		t.Fatalf("poll: %v", err)
	}

	rows := outboxRows(t, conn)
	if len(rows) != 3 {
		t.Fatalf("outbox rows = %d, want 3: %v", len(rows), rows)
	}
	for i, want := range []string{KindReceived, KindSent, KindDeleted} {
		if rows[i].Kind != want || rows[i].Subject != "" {
			t.Fatalf("row %d = (%q, %q), want (%q, empty subject)", i, rows[i].Kind, rows[i].Subject, want)
		}
	}
	if cursor, ok := storedCursor(t, conn); !ok || cursor != "2000" {
		t.Fatalf("cursor = %q ok=%v, want committed advance to 2000 with outbox rows", cursor, ok)
	}
}

func TestProducerFeedFramesCanonicalSubjectlessKey(t *testing.T) {
	// R-XAMA-79FA
	conn := openTestDB(t)
	ob, err := outbox.New(conn, outbox.Options{
		Source:         "gmail",
		DBPath:         t.TempDir() + "/gmail.db",
		GenerationPath: t.TempDir() + "/generation",
		Registry:       mailRegistry(),
	})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	producer := NewOutboxProducer(ob)
	tx, err := conn.BeginTx(context.Background(), nil)
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	if err := producer.AppendMailEvent(tx, MailEvent{Type: KindReceived, ID: "m1", ThreadID: "t1", From: "alice@example.com", Subject: "hello", Snippet: "hi", OccurredAt: "2026-01-01T00:00:00.000000000Z"}); err != nil {
		t.Fatalf("append through producer: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit: %v", err)
	}
	producer.Ring()

	srv := httptest.NewServer(ob.FeedHandler())
	t.Cleanup(srv.Close)
	req, err := http.NewRequestWithContext(context.Background(), http.MethodGet, srv.URL, nil)
	if err != nil {
		t.Fatalf("new feed request: %v", err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatalf("get feed: %v", err)
	}
	t.Cleanup(func() { resp.Body.Close() })
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("feed status = %d", resp.StatusCode)
	}

	scanner := bufio.NewScanner(resp.Body)
	var event, data string
	for scanner.Scan() {
		line := scanner.Text()
		if line == "" {
			if event == "gmail:received" && data != "" {
				break
			}
			event, data = "", ""
			continue
		}
		if strings.HasPrefix(line, "event: ") {
			event = strings.TrimPrefix(line, "event: ")
		}
		if strings.HasPrefix(line, "data: ") {
			data = strings.TrimPrefix(line, "data: ")
		}
	}
	if err := scanner.Err(); err != nil {
		t.Fatalf("read feed: %v", err)
	}
	if event != "gmail:received" {
		t.Fatalf("SSE event = %q, want gmail:received", event)
	}
	var envelope map[string]json.RawMessage
	if err := json.Unmarshal([]byte(data), &envelope); err != nil {
		t.Fatalf("decode feed envelope: %v (%s)", err, data)
	}
	if _, ok := envelope["type"]; ok {
		t.Fatalf("feed envelope must not contain legacy type: %s", data)
	}
	if got := string(envelope["kind"]); got != `"received"` {
		t.Fatalf("feed kind = %s, want received", got)
	}
	if got := string(envelope["subject"]); got != `""` {
		t.Fatalf("feed subject = %s, want empty", got)
	}
}

// ── payload field shape ──────────────────────────────────────────────────────

func TestReceivedAndSentPayloadFields(t *testing.T) {
	fc := &fakeClient{
		profileHistoryID: "1000",
		pages: map[string]HistoryListResult{"": {
			History: []History{
				{ID: "1", MessagesAdded: added("recv")},
				{ID: "2", MessagesAdded: added("sent")},
			},
			HistoryID: "1100",
		}},
		messages: map[string]Message{
			"recv": msg("recv", "tr", []string{LabelInbox}, map[string]string{"From": "alice@example.com", "Subject": "Subj1"}, "snip-r", "1700000000000"),
			"sent": msg("sent", "ts", []string{LabelSent}, map[string]string{"To": "bob@example.com", "Subject": "Subj2"}, "snip-s", "1700000001000"),
		},
	}
	eng, conn, _ := newHarness(t, fc)
	if err := eng.setCursor(context.Background(), "1000"); err != nil {
		t.Fatalf("seed: %v", err)
	}
	if err := eng.Poll(context.Background()); err != nil {
		t.Fatalf("poll: %v", err)
	}

	rows := outboxRows(t, conn)
	if len(rows) != 2 {
		t.Fatalf("want 2 events, got %d: %v", len(rows), rows)
	}
	r := rows[0].Payload
	if rows[0].Kind != KindReceived || r["from"] != "alice@example.com" || r["subject"] != "Subj1" || r["snippet"] != "snip-r" || r["thread_id"] != "tr" || r["id"] != "recv" {
		t.Fatalf("received payload wrong: %v", r)
	}
	if r["received_at"] == "" || r["received_at"] == nil {
		t.Fatalf("received_at missing: %v", r)
	}
	s := rows[1].Payload
	if rows[1].Kind != KindSent || s["to"] != "bob@example.com" || s["subject"] != "Subj2" || s["snippet"] != "snip-s" || s["id"] != "sent" {
		t.Fatalf("sent payload wrong: %v", s)
	}
}

// ── stale cursor (404) resync ────────────────────────────────────────────────

func TestStaleCursorResyncsAndEmitsNothing(t *testing.T) {
	fc := &fakeClient{
		profileHistoryID: "9999",
		historyErr:       ErrNotFound, // history.list 404
	}
	eng, conn, _ := newHarness(t, fc)
	if err := eng.setCursor(context.Background(), "1000"); err != nil {
		t.Fatalf("seed: %v", err)
	}

	if err := eng.Poll(context.Background()); err != nil {
		t.Fatalf("poll (stale resync should not error): %v", err)
	}

	id, ok := storedCursor(t, conn)
	if !ok || id != "9999" {
		t.Fatalf("stale resync must reset cursor to current profile historyId 9999, got %q ok=%v", id, ok)
	}
	if rows := outboxRows(t, conn); len(rows) != 0 {
		t.Fatalf("stale resync must emit nothing (no backfill), got %v", rows)
	}
}

// ── atomic emit+advance: a derived event and the cursor commit together ───────

func TestEmitAndAdvanceAreOneTransaction(t *testing.T) {
	fc := &fakeClient{
		profileHistoryID: "1000",
		pages: map[string]HistoryListResult{"": {
			History:   []History{{ID: "1", MessagesAdded: added("m1")}},
			HistoryID: "2000",
		}},
		messages: map[string]Message{
			"m1": msg("m1", "t1", []string{LabelInbox}, map[string]string{"From": "a@b.c", "Subject": "s"}, "snip", "1700000000000"),
		},
	}
	eng, conn, _ := newHarness(t, fc)
	if err := eng.setCursor(context.Background(), "1000"); err != nil {
		t.Fatalf("seed: %v", err)
	}

	// Before poll: cursor at 1000, no events.
	if id, _ := storedCursor(t, conn); id != "1000" {
		t.Fatalf("precondition cursor 1000, got %q", id)
	}
	if len(outboxRows(t, conn)) != 0 {
		t.Fatal("precondition: outbox should be empty")
	}

	if err := eng.Poll(context.Background()); err != nil {
		t.Fatalf("poll: %v", err)
	}

	// After commit BOTH moved: exactly one event AND the cursor at 2000. The
	// commit() function (sync.go) is the single tx — both writes ride it.
	if rows := outboxRows(t, conn); len(rows) != 1 || rows[0].Kind != KindReceived {
		t.Fatalf("want exactly one received event, got %v", rows)
	}
	if id, _ := storedCursor(t, conn); id != "2000" {
		t.Fatalf("cursor must advance to 2000 with the emission, got %q", id)
	}
}

// ── multi-page drain ─────────────────────────────────────────────────────────

func TestMultiPageDrainEmitsAllAndAdvancesToFinalHistoryID(t *testing.T) {
	fc := &fakeClient{
		profileHistoryID: "1000",
		pages: map[string]HistoryListResult{
			"": {
				History:       []History{{ID: "1", MessagesAdded: added("p1")}},
				NextPageToken: "page2",
				HistoryID:     "1500",
			},
			"page2": {
				History:   []History{{ID: "2", MessagesAdded: added("p2")}},
				HistoryID: "2000",
			},
		},
		messages: map[string]Message{
			"p1": msg("p1", "ta", []string{LabelInbox}, map[string]string{"From": "x@y.z", "Subject": "1"}, "s1", "1700000000000"),
			"p2": msg("p2", "tb", []string{LabelInbox}, map[string]string{"From": "x@y.z", "Subject": "2"}, "s2", "1700000001000"),
		},
	}
	eng, conn, _ := newHarness(t, fc)
	if err := eng.setCursor(context.Background(), "1000"); err != nil {
		t.Fatalf("seed: %v", err)
	}
	if err := eng.Poll(context.Background()); err != nil {
		t.Fatalf("poll: %v", err)
	}
	if rows := outboxRows(t, conn); len(rows) != 2 {
		t.Fatalf("want 2 events across pages, got %d", len(rows))
	}
	if id, _ := storedCursor(t, conn); id != "2000" {
		t.Fatalf("cursor must advance to final page historyId 2000, got %q", id)
	}
}

func equalSlices(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}
