package mcp

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strconv"
	"strings"
	"testing"

	appkitmcp "appkit/mcp"

	gm "gmail/internal/gmail"

	"registry"
)

// fakeClient is an in-memory stand-in for the P2 Gmail client. Each method
// records its arguments and returns canned values (or a canned error), so the
// tool handlers can be exercised with no network. Per the secret rules and the
// live-mutation policy, trash and delete are ONLY exercised here.
type fakeClient struct {
	// recorded args
	listQ, listToken     string
	getID, getFormat     string
	threadID             string
	sendRaw              string
	draftRaw             string
	modifyID             string
	modifyAdd, modifyRem []string
	trashID              string
	deleteID             string
	labelsCalls          int

	// canned returns
	listRes   gm.MessagesListResult
	msg       gm.Message
	thread    gm.Thread
	draft     gm.Draft
	labelsRes gm.LabelsListResult

	err error // when non-nil, every call returns it
}

func (f *fakeClient) MessagesList(_ context.Context, q, token string) (gm.MessagesListResult, error) {
	f.listQ, f.listToken = q, token
	if f.err != nil {
		return gm.MessagesListResult{}, f.err
	}
	return f.listRes, nil
}

func (f *fakeClient) MessageGet(_ context.Context, id, format string) (gm.Message, error) {
	f.getID, f.getFormat = id, format
	if f.err != nil {
		return gm.Message{}, f.err
	}
	return f.msg, nil
}

func (f *fakeClient) ThreadGet(_ context.Context, id string) (gm.Thread, error) {
	f.threadID = id
	if f.err != nil {
		return gm.Thread{}, f.err
	}
	return f.thread, nil
}

func (f *fakeClient) MessagesSend(_ context.Context, raw string) (gm.Message, error) {
	f.sendRaw = raw
	if f.err != nil {
		return gm.Message{}, f.err
	}
	return f.msg, nil
}

func (f *fakeClient) DraftCreate(_ context.Context, raw string) (gm.Draft, error) {
	f.draftRaw = raw
	if f.err != nil {
		return gm.Draft{}, f.err
	}
	return f.draft, nil
}

func (f *fakeClient) LabelsList(_ context.Context) (gm.LabelsListResult, error) {
	f.labelsCalls++
	if f.err != nil {
		return gm.LabelsListResult{}, f.err
	}
	return f.labelsRes, nil
}

func (f *fakeClient) MessageModify(_ context.Context, id string, add, rem []string) (gm.Message, error) {
	f.modifyID, f.modifyAdd, f.modifyRem = id, add, rem
	if f.err != nil {
		return gm.Message{}, f.err
	}
	return f.msg, nil
}

func (f *fakeClient) MessageTrash(_ context.Context, id string) (gm.Message, error) {
	f.trashID = id
	if f.err != nil {
		return gm.Message{}, f.err
	}
	return f.msg, nil
}

func (f *fakeClient) MessageDelete(_ context.Context, id string) error {
	f.deleteID = id
	return f.err
}

func newHandler(t *testing.T) (http.Handler, *fakeClient) {
	t.Helper()
	fc := &fakeClient{}
	h, err := appkitmcp.New(appkitmcp.Options{
		Service:      "gmail",
		Version:      "v-test",
		Instructions: Instructions,
		Tools:        Tools(fc, contentBase()),
		Events:       Events,
	})
	if err != nil {
		t.Fatalf("new appkit mcp handler: %v", err)
	}
	return h, fc
}

func contentBase() string {
	return "http://127.0.0.1:" + strconv.Itoa(registry.MustPort("gmail"))
}

// rpc drives one JSON-RPC call through ServeHTTP and returns the decoded result
// object. params is the raw JSON for "params".
func rpc(t *testing.T, h http.Handler, method, params string) map[string]any {
	t.Helper()
	body := `{"jsonrpc":"2.0","id":1,"method":"` + method + `","params":` + params + `}`
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", "me@example.com")
	req.Header.Set("X-Client-Id", "client-123")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("%s: status %d", method, rec.Code)
	}
	var env struct {
		Result map[string]any `json:"result"`
		Error  any            `json:"error"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &env); err != nil {
		t.Fatalf("%s: decode envelope: %v\n%s", method, err, rec.Body.String())
	}
	if env.Error != nil {
		t.Fatalf("%s: transport error %v", method, env.Error)
	}
	return env.Result
}

// callToolText invokes tools/call and returns the raw text content plus the
// isError flag. Used when the result text may be plain (a validation/error
// message) rather than a JSON object.
func callToolText(t *testing.T, h http.Handler, name, args string) (string, bool) {
	t.Helper()
	res := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":`+args+`}`)
	isErr, _ := res["isError"].(bool)
	content, ok := res["content"].([]any)
	if !ok || len(content) == 0 {
		t.Fatalf("%s: no content: %v", name, res)
	}
	return content[0].(map[string]any)["text"].(string), isErr
}

// callTool invokes tools/call and returns the decoded JSON text payload plus the
// isError flag. For a non-error result the text is always a JSON object.
func callTool(t *testing.T, h http.Handler, name, args string) (map[string]any, bool) {
	t.Helper()
	text, isErr := callToolText(t, h, name, args)
	if isErr {
		// Error results carry plain or JSON text; return it under a sentinel key
		// so callers asserting only isErr don't trip on a decode.
		var payload map[string]any
		if json.Unmarshal([]byte(text), &payload) == nil {
			return payload, isErr
		}
		return map[string]any{"_text": text}, isErr
	}
	var payload map[string]any
	if err := json.Unmarshal([]byte(text), &payload); err != nil {
		t.Fatalf("%s: decode payload %q: %v", name, text, err)
	}
	return payload, isErr
}

// TestToolsList_ExactlyTwelve asserts the full MCP surface is advertised: the
// two chassis tools plus the ten mailbox verbs, and nothing deferred.
func TestToolsList_ExactlyTwelve(t *testing.T) {
	h, _ := newHandler(t)
	res := rpc(t, h, "tools/list", `{}`)
	tools, _ := res["tools"].([]any)
	names := map[string]bool{}
	for _, tl := range tools {
		names[tl.(map[string]any)["name"].(string)] = true
	}
	want := []string{
		"health", "reflection",
		"list", "read", "thread",
		"labels", "send", "draft",
		"label", "unlabel",
		"trash", "delete",
	}
	// R-9NYN-SVIR
	for _, w := range want {
		if !names[w] {
			t.Errorf("tools/list missing %q (got %v)", w, names)
		}
	}
	if len(names) != len(want) {
		t.Errorf("expected %d tools, got %d: %v", len(want), len(names), names)
	}
	// Deferred verbs must NOT appear (decisions §3).
	for _, leaked := range []string{
		"reply", "sync_now",
	} {
		if names[leaked] {
			t.Errorf("surface leaks deferred tool %q", leaked)
		}
	}
}

// TestReflection covers the reflection tool's subjectless family registry.
func TestReflection(t *testing.T) {
	// R-X86H-FPXW
	h, _ := newHandler(t)

	idx, isErr := callTool(t, h, "reflection", `{}`)
	if isErr {
		t.Fatalf("reflection index isError: %v", idx)
	}
	publishes, ok := idx["publishes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing publishes array: %v", idx)
	}
	got := map[string]map[string]any{}
	for _, pe := range publishes {
		p := pe.(map[string]any)
		got[p["kind"].(string)] = p
	}
	for _, want := range []string{"received", "sent", "deleted"} {
		p, ok := got[want]
		if !ok || p["subject"] != "" {
			t.Errorf("publishes missing %q (got %v)", want, got)
		}
	}
	if len(publishes) != 3 {
		t.Errorf("expected exactly 3 published families, got %d: %v", len(publishes), publishes)
	}

	subscribes, ok := idx["subscribes"].([]any)
	if !ok {
		t.Fatalf("reflection index missing subscribes array: %v", idx)
	}
	if len(subscribes) != 0 {
		t.Fatalf("expected empty subscribes for gmail, got %v", subscribes)
	}

	detail, isErr := callTool(t, h, "reflection", `{"kind":"received"}`)
	if isErr {
		t.Fatalf("reflection detail isError: %v", detail)
	}
	if detail["kind"] != "received" || detail["subject"] != "" {
		t.Fatalf("detail kind mismatch: %v", detail)
	}
	sch, ok := detail["schema"].(map[string]any)
	if !ok || sch["type"] != "object" {
		t.Fatalf("detail schema not an object schema: %v", detail["schema"])
	}
	example, ok := detail["example"].(map[string]any)
	if !ok {
		t.Fatalf("detail example is not an object: %v", detail["example"])
	}
	properties, ok := sch["properties"].(map[string]any)
	if !ok {
		t.Fatalf("detail schema properties missing: %v", sch)
	}
	for field := range example {
		if _, ok := properties[field]; !ok {
			t.Fatalf("example field %q missing from schema: %v", field, sch)
		}
	}

	badErr, isErr := callTool(t, h, "reflection", `{"kind":"nope"}`)
	if !isErr {
		t.Fatalf("expected error for unknown event_type, got %v", badErr)
	}
	text, _ := badErr["_text"].(string)
	for _, want := range []string{"unknown event kind", "received", "sent", "deleted"} {
		if !strings.Contains(text, want) {
			t.Fatalf("expected corrective unknown kind error naming declared kinds, got %v", badErr)
		}
	}
}

func TestHealth_Envelope(t *testing.T) {
	h, _ := newHandler(t)
	p, isErr := callTool(t, h, "health", `{}`)
	if isErr {
		t.Fatal("health isError")
	}
	if p["status"] != "ok" || p["version"] != "v-test" || p["service"] != "gmail" {
		t.Errorf("health envelope keys = %v", p)
	}
	if p["owner_email"] != "me@example.com" || p["client_id"] != "client-123" {
		t.Errorf("health identity = %v", p)
	}
}

func TestUnknownTool_IsTransportError(t *testing.T) {
	h, _ := newHandler(t)
	body := `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"gmail_bogus","arguments":{}}}`
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	var env struct {
		Error map[string]any `json:"error"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &env); err != nil {
		t.Fatalf("decode envelope: %v\n%s", err, rec.Body.String())
	}
	if env.Error["message"] != "unknown tool: gmail_bogus" {
		t.Errorf("expected transport unknown-tool error, got %v", env.Error)
	}
}

// TestNewHandler_NilClientPanics guards the wiring seam.
func TestNewHandler_NilClientPanics(t *testing.T) {
	defer func() {
		if recover() == nil {
			t.Fatal("expected panic on nil client")
		}
	}()
	NewHandler(nil, contentBase(), nil)
}

func TestNewHandler_EmptyContentBasePanics(t *testing.T) {
	defer func() {
		if recover() == nil {
			t.Fatal("expected panic on empty content base")
		}
	}()
	NewHandler(&fakeClient{}, "", nil)
}

// ── read-only tools ───────────────────────────────────────────────────────

func TestList_PassesQueryAndPaging(t *testing.T) {
	h, fc := newHandler(t)
	fc.listRes = gm.MessagesListResult{
		Messages:           []gm.MessageRef{{ID: "m1", ThreadID: "t1"}, {ID: "m2", ThreadID: "t2"}},
		NextPageToken:      "next-pg",
		ResultSizeEstimate: 2,
	}
	p, isErr := callTool(t, h, "list", `{"q":"is:unread from:alice","page_token":"pg0"}`)
	if isErr {
		t.Fatalf("list isError: %v", p)
	}
	if fc.listQ != "is:unread from:alice" || fc.listToken != "pg0" {
		t.Errorf("list args q=%q token=%q", fc.listQ, fc.listToken)
	}
	if p["next_page_token"] != "next-pg" {
		t.Errorf("next_page_token = %v", p["next_page_token"])
	}
	msgs, _ := p["messages"].([]any)
	if len(msgs) != 2 {
		t.Fatalf("expected 2 messages, got %v", p["messages"])
	}
	first := msgs[0].(map[string]any)
	if first["id"] != "m1" || first["thread_id"] != "t1" {
		t.Errorf("first message = %v", first)
	}
}

func TestRead_RendersHeadersAndAttachments(t *testing.T) {
	h, fc := newHandler(t)
	fc.msg = gm.Message{
		ID: "m1", ThreadID: "t1", LabelIDs: []string{"INBOX", "UNREAD"}, Snippet: "hi there",
		Payload: gm.MessagePart{
			MimeType: "multipart/mixed",
			Headers: []gm.Header{
				{Name: "Subject", Value: "Hello"},
				{Name: "From", Value: "alice@example.com"},
			},
			Parts: []gm.MessagePart{
				{MimeType: "text/plain", Body: gm.Body{Size: 10}},
				{MimeType: "application/pdf", Filename: "doc.pdf", Body: gm.Body{Size: 2048}},
			},
		},
	}
	p, isErr := callTool(t, h, "read", `{"id":"m1"}`)
	if isErr {
		t.Fatalf("read isError: %v", p)
	}
	if fc.getID != "m1" || fc.getFormat != "full" {
		t.Errorf("MessageGet args id=%q format=%q", fc.getID, fc.getFormat)
	}
	hdrs, _ := p["headers"].(map[string]any)
	if hdrs["Subject"] != "Hello" || hdrs["From"] != "alice@example.com" {
		t.Errorf("headers = %v", hdrs)
	}
	atts, _ := p["attachments"].([]any)
	if len(atts) != 1 {
		t.Fatalf("expected 1 attachment, got %v", p["attachments"])
	}
	a := atts[0].(map[string]any)
	if a["filename"] != "doc.pdf" || a["mime_type"] != "application/pdf" {
		t.Errorf("attachment = %v", a)
	}
}

func TestRead_RendersAttachmentReference(t *testing.T) {
	// R-3JSW-5BHT
	h, fc := newHandler(t)
	attachmentID := "ephemeral-token-must-not-leak"
	fc.msg = gm.Message{ID: "message-1", Payload: gm.MessagePart{Parts: []gm.MessagePart{{
		MimeType: "application/pdf", Filename: "2026-06-inv.pdf",
		PartID: "2/with space", Body: gm.Body{Size: 184230, AttachmentID: attachmentID},
	}}}}
	p, isErr := callTool(t, h, "read", `{"id":"message-1"}`)
	if isErr {
		t.Fatalf("read isError: %v", p)
	}
	atts := p["attachments"].([]any)
	if len(atts) != 1 {
		t.Fatalf("attachments = %v", p["attachments"])
	}
	a := atts[0].(map[string]any)
	if a["filename"] != "2026-06-inv.pdf" || a["size"] != float64(184230) || a["mime_type"] != "application/pdf" {
		t.Fatalf("attachment = %v", a)
	}
	if _, ok := a["attachment_id"]; ok {
		t.Fatalf("attachment leaks ephemeral id: %v", a)
	}
	u, err := url.Parse(a["content_url"].(string))
	if err != nil {
		t.Fatalf("parse content_url: %v", err)
	}
	wantURL := contentBase() + "/attachment?" + url.Values{
		"message_id": []string{"message-1"}, "part_id": []string{"2/with space"},
	}.Encode()
	if a["content_url"] != wantURL {
		t.Fatalf("content_url = %q, want %q", a["content_url"], wantURL)
	}
	if u.Scheme != "http" || u.Host != "127.0.0.1:"+strconv.Itoa(registry.MustPort("gmail")) || u.Path != "/attachment" {
		t.Fatalf("content_url = %q", u.String())
	}
	if u.Query().Get("message_id") != "message-1" || u.Query().Get("part_id") != "2/with space" {
		t.Fatalf("content_url query = %v", u.Query())
	}
	if strings.Contains(fmt.Sprint(p), attachmentID) {
		t.Fatalf("rendered result leaks ephemeral id: %v", p)
	}
}

func TestRead_UnfetchableAttachmentsHaveMetadataOnly(t *testing.T) {
	// R-3M8O-WUZ7
	h, fc := newHandler(t)
	fc.msg = gm.Message{ID: "message-1", Payload: gm.MessagePart{Parts: []gm.MessagePart{
		{PartID: "1", MimeType: "image/png", Filename: "inline.png", Body: gm.Body{Size: 42}},
		{MimeType: "application/pdf", Filename: "missing-part-id.pdf", Body: gm.Body{Size: 43, AttachmentID: "ephemeral"}},
	}}}
	p, isErr := callTool(t, h, "read", `{"id":"message-1"}`)
	if isErr {
		t.Fatalf("read isError: %v", p)
	}
	for _, raw := range p["attachments"].([]any) {
		a := raw.(map[string]any)
		if _, ok := a["attachment_id"]; ok {
			t.Fatalf("metadata-only attachment leaks id: %v", a)
		}
		if _, ok := a["content_url"]; ok {
			t.Fatalf("metadata-only attachment has URL: %v", a)
		}
	}
}

func TestRead_MissingID(t *testing.T) {
	h, _ := newHandler(t)
	p, isErr := callTool(t, h, "read", `{}`)
	if !isErr {
		t.Fatalf("expected error for missing id, got %v", p)
	}
}

func TestThread_RendersMessages(t *testing.T) {
	h, fc := newHandler(t)
	fc.thread = gm.Thread{
		ID: "t1", Snippet: "thread snip",
		Messages: []gm.Message{
			{ID: "m1", ThreadID: "t1"},
			{ID: "m2", ThreadID: "t1"},
		},
	}
	p, isErr := callTool(t, h, "thread", `{"id":"t1"}`)
	if isErr {
		t.Fatalf("thread isError: %v", p)
	}
	if fc.threadID != "t1" {
		t.Errorf("ThreadGet id = %q", fc.threadID)
	}
	msgs, _ := p["messages"].([]any)
	if len(msgs) != 2 {
		t.Fatalf("expected 2 messages, got %v", p["messages"])
	}
}

func TestThread_RendersAttachmentReferencesPerMessage(t *testing.T) {
	// R-3L0S-J38I
	h, fc := newHandler(t)
	fc.thread = gm.Thread{ID: "thread-1", Messages: []gm.Message{
		{ID: "message-1", Payload: gm.MessagePart{Parts: []gm.MessagePart{{PartID: "2", Filename: "one.pdf", MimeType: "application/pdf", Body: gm.Body{Size: 1, AttachmentID: "att-1"}}}}},
		{ID: "message-2", Payload: gm.MessagePart{Parts: []gm.MessagePart{{PartID: "3", Filename: "two.pdf", MimeType: "application/pdf", Body: gm.Body{Size: 2, AttachmentID: "att-2"}}}}},
	}}
	p, isErr := callTool(t, h, "thread", `{"id":"thread-1"}`)
	if isErr {
		t.Fatalf("thread isError: %v", p)
	}
	msgs := p["messages"].([]any)
	if len(msgs) != 2 {
		t.Fatalf("messages = %v", p["messages"])
	}
	urls := make([]*url.URL, 0, len(msgs))
	for i, msg := range msgs {
		a := msg.(map[string]any)["attachments"].([]any)[0].(map[string]any)
		if _, ok := a["attachment_id"]; ok {
			t.Fatalf("attachment %d leaks id: %v", i, a)
		}
		u, err := url.Parse(a["content_url"].(string))
		if err != nil {
			t.Fatalf("parse attachment %d URL: %v", i, err)
		}
		if u.Query().Get("message_id") != "message-"+strconv.Itoa(i+1) {
			t.Fatalf("attachment %d URL = %q", i, u.String())
		}
		if u.Query().Get("part_id") != strconv.Itoa(i+2) {
			t.Fatalf("attachment %d part id = %q", i, u.Query().Get("part_id"))
		}
		urls = append(urls, u)
	}
	if urls[0].String() == urls[1].String() {
		t.Fatalf("thread attachment URLs should differ: %q", urls[0])
	}
}

func TestLabels_List(t *testing.T) {
	h, fc := newHandler(t)
	fc.labelsRes = gm.LabelsListResult{Labels: []gm.Label{
		{ID: "INBOX", Name: "INBOX", Type: "system"},
		{ID: "Label_42", Name: "Work", Type: "user"},
	}}
	p, isErr := callTool(t, h, "labels", `{}`)
	if isErr {
		t.Fatalf("labels isError: %v", p)
	}
	if fc.labelsCalls != 1 {
		t.Errorf("LabelsList called %d times", fc.labelsCalls)
	}
	labels, _ := p["labels"].([]any)
	if len(labels) != 2 {
		t.Fatalf("expected 2 labels, got %v", p["labels"])
	}
}

// ── mutating tools ────────────────────────────────────────────────────────

// decodeRaw decodes a base64url raw RFC-2822 message back to its bytes.
func decodeRaw(t *testing.T, raw string) string {
	t.Helper()
	b, err := base64.URLEncoding.DecodeString(raw)
	if err != nil {
		t.Fatalf("raw not base64url: %v", err)
	}
	return string(b)
}

func TestSend_BuildsRawMessage(t *testing.T) {
	h, fc := newHandler(t)
	fc.msg = gm.Message{ID: "sent1", ThreadID: "t9", LabelIDs: []string{"SENT"}}
	p, isErr := callTool(t, h, "send",
		`{"to":"bob@example.com","subject":"P4 test","body":"hello body"}`)
	if isErr {
		t.Fatalf("send isError: %v", p)
	}
	if p["id"] != "sent1" || p["thread_id"] != "t9" {
		t.Errorf("send result = %v", p)
	}
	msg := decodeRaw(t, fc.sendRaw)
	for _, want := range []string{"To: bob@example.com", "Subject: P4 test",
		"Content-Type: text/plain", "hello body"} {
		if !strings.Contains(msg, want) {
			t.Errorf("raw message missing %q:\n%s", want, msg)
		}
	}
	// Header/body separated by a blank line.
	if !strings.Contains(msg, "\r\n\r\nhello body") {
		t.Errorf("raw message missing header/body separator:\n%s", msg)
	}
}

func TestSend_MissingTo(t *testing.T) {
	h, _ := newHandler(t)
	p, isErr := callTool(t, h, "send", `{"subject":"x","body":"y"}`)
	if !isErr {
		t.Fatalf("expected error for missing to, got %v", p)
	}
}

func TestDraft_BuildsRawMessage(t *testing.T) {
	h, fc := newHandler(t)
	fc.draft = gm.Draft{ID: "d1", Message: gm.Message{ID: "dm1", ThreadID: "t3"}}
	p, isErr := callTool(t, h, "draft",
		`{"to":"carol@example.com","subject":"Draft P4","body":"draft body"}`)
	if isErr {
		t.Fatalf("draft isError: %v", p)
	}
	if p["id"] != "d1" {
		t.Errorf("draft id = %v", p["id"])
	}
	msg := decodeRaw(t, fc.draftRaw)
	if !strings.Contains(msg, "To: carol@example.com") || !strings.Contains(msg, "draft body") {
		t.Errorf("draft raw message wrong:\n%s", msg)
	}
}

func TestSubject_NonASCIIEncoded(t *testing.T) {
	h, fc := newHandler(t)
	_, isErr := callTool(t, h, "send",
		`{"to":"x@example.com","subject":"café ☕","body":"b"}`)
	if isErr {
		t.Fatal("send isError")
	}
	msg := decodeRaw(t, fc.sendRaw)
	// RFC-2047 encoded-word wrapping for non-ASCII subjects.
	if !strings.Contains(msg, "Subject: =?utf-8?") {
		t.Errorf("non-ASCII subject not encoded-word wrapped:\n%s", msg)
	}
}

func TestLabel_AddsLabel(t *testing.T) {
	h, fc := newHandler(t)
	fc.msg = gm.Message{ID: "m1", LabelIDs: []string{"Label_42"}}
	p, isErr := callTool(t, h, "label", `{"id":"m1","label_id":"Label_42"}`)
	if isErr {
		t.Fatalf("label isError: %v", p)
	}
	if fc.modifyID != "m1" || len(fc.modifyAdd) != 1 || fc.modifyAdd[0] != "Label_42" {
		t.Errorf("modify add args id=%q add=%v rem=%v", fc.modifyID, fc.modifyAdd, fc.modifyRem)
	}
	if len(fc.modifyRem) != 0 {
		t.Errorf("label should not remove, rem=%v", fc.modifyRem)
	}
}

func TestUnlabel_RemovesLabel_Archive(t *testing.T) {
	h, fc := newHandler(t)
	fc.msg = gm.Message{ID: "m1", LabelIDs: []string{}}
	// archive = remove INBOX (decisions §1).
	p, isErr := callTool(t, h, "unlabel", `{"id":"m1","label_id":"INBOX"}`)
	if isErr {
		t.Fatalf("unlabel isError: %v", p)
	}
	if fc.modifyID != "m1" || len(fc.modifyRem) != 1 || fc.modifyRem[0] != "INBOX" {
		t.Errorf("modify remove args id=%q add=%v rem=%v", fc.modifyID, fc.modifyAdd, fc.modifyRem)
	}
	if len(fc.modifyAdd) != 0 {
		t.Errorf("unlabel should not add, add=%v", fc.modifyAdd)
	}
}

func TestLabel_MissingArgs(t *testing.T) {
	h, _ := newHandler(t)
	if _, isErr := callTool(t, h, "label", `{"id":"m1"}`); !isErr {
		t.Error("expected error for missing label_id")
	}
	if _, isErr := callTool(t, h, "label", `{"label_id":"X"}`); !isErr {
		t.Error("expected error for missing id")
	}
}

// trash and delete: FAKE-CLIENT ONLY (never live, per the secret/mutation rules).

func TestTrash_CallsClient(t *testing.T) {
	h, fc := newHandler(t)
	fc.msg = gm.Message{ID: "m1", LabelIDs: []string{"TRASH"}}
	p, isErr := callTool(t, h, "trash", `{"id":"m1"}`)
	if isErr {
		t.Fatalf("trash isError: %v", p)
	}
	if fc.trashID != "m1" {
		t.Errorf("MessageTrash id = %q", fc.trashID)
	}
	labels, _ := p["label_ids"].([]any)
	if len(labels) != 1 || labels[0] != "TRASH" {
		t.Errorf("trash label_ids = %v", p["label_ids"])
	}
}

func TestTrash_MissingID(t *testing.T) {
	h, _ := newHandler(t)
	if _, isErr := callTool(t, h, "trash", `{}`); !isErr {
		t.Error("expected error for missing id")
	}
}

func TestDelete_CallsClient(t *testing.T) {
	h, fc := newHandler(t)
	p, isErr := callTool(t, h, "delete", `{"id":"m1"}`)
	if isErr {
		t.Fatalf("delete isError: %v", p)
	}
	if fc.deleteID != "m1" {
		t.Errorf("MessageDelete id = %q", fc.deleteID)
	}
	if p["deleted"] != true || p["id"] != "m1" {
		t.Errorf("delete result = %v", p)
	}
}

func TestDelete_MissingID(t *testing.T) {
	h, _ := newHandler(t)
	if _, isErr := callTool(t, h, "delete", `{}`); !isErr {
		t.Error("expected error for missing id")
	}
}

// TestClientError_MapsToToolError confirms a client failure becomes an isError
// tool result (not a transport error).
func TestClientError_MapsToToolError(t *testing.T) {
	h, fc := newHandler(t)
	fc.err = gm.ErrNotFound
	if _, isErr := callTool(t, h, "read", `{"id":"missing"}`); !isErr {
		t.Error("expected isError when client returns an error")
	}
	if _, isErr := callTool(t, h, "list", `{}`); !isErr {
		t.Error("expected isError when list client errors")
	}
}
