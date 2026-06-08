package mcp

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"mime"
	"net/http"
	"strings"

	"appkit"

	gm "gmail/internal/gmail"

	"eventplane/consumer"
	"eventplane/outbox"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

// ── published events (producer half) ─────────────────────────────────────────

// Mail event type names (decisions §1). Emission lands in P3; the registry is
// declared here in P1 so the reflection tool already self-describes the producer.
const (
	EventMailReceived = "mail.received"
	EventMailSent     = "mail.sent"
	EventMailDeleted  = "mail.deleted"
)

// mailReceivedPayload is the wire shape of a mail.received event (decisions §1
// table): an inbound message that landed in INBOX.
type mailReceivedPayload struct {
	ID         string `json:"id"`
	ThreadID   string `json:"thread_id"`
	From       string `json:"from"`
	Subject    string `json:"subject"`
	Snippet    string `json:"snippet"`
	ReceivedAt string `json:"received_at"`
}

// mailSentPayload is the wire shape of a mail.sent event: a message that carries
// SENT (and not INBOX) — our own sends, whether via MCP or the Gmail UI.
type mailSentPayload struct {
	ID       string `json:"id"`
	ThreadID string `json:"thread_id"`
	To       string `json:"to"`
	Subject  string `json:"subject"`
	Snippet  string `json:"snippet"`
	SentAt   string `json:"sent_at"`
}

// mailDeletedPayload is the wire shape of a mail.deleted event: a message moved
// to Trash (labelsAdded: TRASH), not a permanent expunge.
type mailDeletedPayload struct {
	ID        string `json:"id"`
	ThreadID  string `json:"thread_id"`
	Subject   string `json:"subject"`
	DeletedAt string `json:"deleted_at"`
}

// Events is the published-event Registry for the reflection tool and (in P3)
// Append-time validation, wired via Spec.Events. The three mail.* types are the
// producer's complete published set (decisions §1). Each entry carries a
// filled-in Sample of its real payload struct — the single source for both the
// reflected JSON Schema and the worked example, so schema/example/wire shape
// can't diverge.
var Events = outbox.Registry{
	{
		Type:        EventMailReceived,
		Description: "An inbound message arrived in the mailbox (Gmail History messagesAdded carrying the INBOX label). Carries message identity + envelope headers; fetch the full message via the read tool.",
		Sample: mailReceivedPayload{
			ID:         "18f2a1b3c4d5e6f7",
			ThreadID:   "18f2a1b3c4d5e6f0",
			From:       "alice@example.com",
			Subject:    "Lunch tomorrow?",
			Snippet:    "Are you free around noon to grab lunch...",
			ReceivedAt: "2026-06-03T12:00:00.000000000Z",
		},
	},
	{
		Type:        EventMailSent,
		Description: "A message was sent from the mailbox (Gmail History messagesAdded carrying the SENT label and not INBOX) — covers our own sends uniformly, whether via the send MCP tool or the Gmail UI.",
		Sample: mailSentPayload{
			ID:       "18f2a1b3c4d5e6f8",
			ThreadID: "18f2a1b3c4d5e6f0",
			To:       "bob@example.com",
			Subject:  "Re: Lunch tomorrow?",
			Snippet:  "Sounds good, noon works for me...",
			SentAt:   "2026-06-03T12:05:00.000000000Z",
		},
	},
	{
		Type:        EventMailDeleted,
		Description: "A message was moved to Trash (Gmail History labelsAdded: TRASH). This is the discard signal, not a permanent expunge — the message still exists in Trash, so its payload is still fetchable.",
		Sample: mailDeletedPayload{
			ID:        "18f2a1b3c4d5e6f9",
			ThreadID:  "18f2a1b3c4d5e6f0",
			Subject:   "Old newsletter",
			DeletedAt: "2026-06-03T12:10:00.000000000Z",
		},
	},
}

// ── tool descriptors ──────────────────────────────────────────────────────────

// toolDescriptors returns the full P4 gmail mailbox surface (decisions §1): the
// normal-mailbox operations over the P2 client, plus the two chassis tools.
// Read-only tools (list/read/thread/labels) only fetch; mutating tools
// (send/draft/label/unlabel/trash/delete) change the mailbox. trash is
// recoverable; delete is a PERMANENT expunge (the full-scope destructive verb).
// Deferred and intentionally absent: reply, attachment download, sync_now
// (decisions §3).
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("health"),
			"Health + diagnostics for the gmail service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). Takes no inputs.",
			obj(map[string]any{})),
		desc(tool("reflection"),
			"Self-describe gmail's edges in the event graph. With no arguments, returns the index {publishes:[{type,description}], subscribes:[{source,filter,description}]} — gmail is a producer, so subscribes is empty. Pass 'event_type' (a published type) for its detail {type, description, schema, example}.",
			obj(map[string]any{
				"event_type": descTyp("string", "optional; a published event type to fetch the schema+example detail for"),
			})),

		// ── read-only ──────────────────────────────────────────────────────
		desc(tool("list"),
			"List or SEARCH messages (one call either way). Returns bare message pointers {id, thread_id} plus a next_page_token for pagination and a result_size_estimate. Use read to fetch a pointer's headers/body.",
			obj(map[string]any{
				"q":          descTyp("string", "optional Gmail search query (e.g. 'from:alice@example.com', 'subject:invoice', 'is:unread', 'after:2026/01/01'); empty lists recent messages"),
				"page_token": descTyp("string", "optional pagination cursor from a prior call's next_page_token"),
			})),
		desc(tool("read"),
			"Read a full message by id: its headers, snippet, label ids, and attachment METADATA (filename, size, mime_type) for each attachment part. Attachment blob download is not supported.",
			obj(map[string]any{
				"id": descTyp("string", "the message id (from list/search or an event payload)"),
			}, "id")),
		desc(tool("thread"),
			"Read a whole thread by id: the thread's messages in order, each with headers, snippet, label ids, and attachment metadata.",
			obj(map[string]any{
				"id": descTyp("string", "the thread id (thread_id from a message or event payload)"),
			}, "id")),
		desc(tool("labels"),
			"List the mailbox's available labels — system labels (INBOX, SENT, UNREAD, TRASH, …) and user labels — as {id, name, type}. Use a label id with label/unlabel. Takes no inputs.",
			obj(map[string]any{})),

		// ── mutating ───────────────────────────────────────────────────────
		desc(tool("send"),
			"Send an email. Composes an RFC-2822 message from the structured fields and sends it via Gmail. The send shows up as a mail.sent event on the next poll. Returns the created message {id, thread_id, label_ids}.",
			obj(map[string]any{
				"to":      descTyp("string", "recipient email address (To: header)"),
				"subject": descTyp("string", "the Subject: header"),
				"body":    descTyp("string", "the plain-text message body"),
			}, "to", "subject", "body")),
		desc(tool("draft"),
			"Create a draft (does NOT send). Composes an RFC-2822 message from the structured fields and saves it as a Gmail draft. Returns the created draft {id, message:{id, thread_id}}.",
			obj(map[string]any{
				"to":      descTyp("string", "recipient email address (To: header)"),
				"subject": descTyp("string", "the Subject: header"),
				"body":    descTyp("string", "the plain-text message body"),
			}, "to", "subject", "body")),
		desc(tool("label"),
			"Apply a label to a message (adds the label id). Returns the updated message {id, label_ids}.",
			obj(map[string]any{
				"id":       descTyp("string", "the message id"),
				"label_id": descTyp("string", "the label id to add (a system id like INBOX/UNREAD or a user label id from the labels tool)"),
			}, "id", "label_id")),
		desc(tool("unlabel"),
			"Remove a label from a message (removes the label id). Remove INBOX to archive; remove UNREAD to mark read. Returns the updated message {id, label_ids}.",
			obj(map[string]any{
				"id":       descTyp("string", "the message id"),
				"label_id": descTyp("string", "the label id to remove (e.g. INBOX to archive, UNREAD to mark read)"),
			}, "id", "label_id")),
		desc(tool("trash"),
			"Move a message to Trash (RECOVERABLE). Emits a mail.deleted event on the next poll. Returns the updated message {id, label_ids}.",
			obj(map[string]any{
				"id": descTyp("string", "the message id to trash"),
			}, "id")),
		desc(tool("delete"),
			"PERMANENTLY delete a message (NOT recoverable — bypasses Trash). This is the full-scope destructive operation; prefer trash unless permanent removal is intended. Returns {id, deleted:true}.",
			obj(map[string]any{
				"id": descTyp("string", "the message id to permanently delete"),
			}, "id")),
	}
}

// ── schema helpers ──────────────────────────────────────────────────────────

func desc(name, description string, schema map[string]any) map[string]any {
	return map[string]any{"name": name, "description": description, "inputSchema": schema}
}

func obj(props map[string]any, required ...string) map[string]any {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	return o
}

func descTyp(t, description string) map[string]any {
	return map[string]any{"type": t, "description": description}
}

// ── dispatch ──────────────────────────────────────────────────────────────

type toolCallParams struct {
	Name      string          `json:"name"`
	Arguments json.RawMessage `json:"arguments"`
}

func (h *Handler) handleToolCall(ctx context.Context, w http.ResponseWriter, req jsonRPCRequest, id Identity) {
	var p toolCallParams
	if err := json.Unmarshal(req.Params, &p); err != nil {
		writeJSONRPCError(w, req.ID, -32602, "invalid params")
		return
	}
	res, err := h.dispatchTool(ctx, p.Name, p.Arguments, id)
	if err != nil {
		writeJSONRPCResult(w, req.ID, toolResultErr(err.Error()))
		return
	}
	writeJSONRPCResult(w, req.ID, res)
}

func (h *Handler) dispatchTool(ctx context.Context, name string, argsRaw json.RawMessage, id Identity) (map[string]any, error) {
	switch name {
	case tool("health"):
		return h.toolHealth(ctx, id)
	case tool("reflection"):
		return h.toolReflection(argsRaw)
	case tool("list"):
		return h.toolList(ctx, argsRaw)
	case tool("read"):
		return h.toolRead(ctx, argsRaw)
	case tool("thread"):
		return h.toolThread(ctx, argsRaw)
	case tool("labels"):
		return h.toolLabels(ctx)
	case tool("send"):
		return h.toolSend(ctx, argsRaw)
	case tool("draft"):
		return h.toolDraft(ctx, argsRaw)
	case tool("label"):
		return h.toolLabel(ctx, argsRaw)
	case tool("unlabel"):
		return h.toolUnlabel(ctx, argsRaw)
	case tool("trash"):
		return h.toolTrash(ctx, argsRaw)
	case tool("delete"):
		return h.toolDelete(ctx, argsRaw)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── tool implementations ─────────────────────────────────────────────────

// toolHealth renders the shared health envelope (status/version/service/details)
// via appkit.Envelope and then adds the authenticated caller's identity — the
// end-to-end auth-chain proof. gmail has no per-service reporter in P1, so
// details renders as an empty object unless a reporter is later wired.
func (h *Handler) toolHealth(ctx context.Context, id Identity) (map[string]any, error) {
	details := map[string]any{}
	if h.health != nil {
		d, err := h.health(ctx)
		if err != nil {
			details = map[string]any{"error": err.Error()}
		} else if d != nil {
			details = d
		}
	}
	env := appkit.Envelope(h.version, h.service, details) // status/version/service/details
	env["owner_email"] = id.OwnerEmail
	env["client_id"] = id.ClientID
	return toolResultJSON(env)
}

// toolReflection self-describes gmail's edges in the event graph (the
// ikigenba_<svc>_reflection tool). No event_type → the index {publishes,
// subscribes}; with event_type → that published type's {type, description,
// schema, example}. An unknown event_type returns a corrective error listing the
// valid types, not an empty result.
func (h *Handler) toolReflection(raw json.RawMessage) (map[string]any, error) {
	var a struct {
		EventType string `json:"event_type,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	if a.EventType != "" {
		detail, err := h.events.Detail(a.EventType)
		if err != nil {
			var unknown *outbox.UnknownEventTypeError
			if errors.As(err, &unknown) {
				return toolResultErr(reflectionUnknownTypeError(unknown)), nil
			}
			return nil, err
		}
		return toolResultJSON(detail)
	}
	return toolResultJSON(map[string]any{
		"publishes":  h.events.Index(),
		"subscribes": renderSubscriptions(h.subscriptions),
	})
}

// renderSubscriptions flattens the live subscription provider to the reflection
// in-edges: one {source, filter, description} per Subscription. A nil provider
// (or nil result) renders as an empty list — gmail is a producer, so this is
// always empty.
func renderSubscriptions(provider func() []consumer.Subscription) []map[string]any {
	out := []map[string]any{}
	if provider == nil {
		return out
	}
	for _, s := range provider() {
		out = append(out, map[string]any{
			"source":      s.Source,
			"filter":      s.Filter,
			"description": s.Description,
		})
	}
	return out
}

// reflectionUnknownTypeError renders the corrective error envelope for an unknown
// event_type, listing the valid types so the agent can self-correct.
func reflectionUnknownTypeError(e *outbox.UnknownEventTypeError) string {
	env := map[string]any{"error": map[string]any{
		"code":    "unknown_event_type",
		"message": "unknown event_type " + e.Type + "; valid types: " + strings.Join(e.Valid, ", "),
	}}
	b, _ := json.Marshal(env)
	return string(b)
}

// ── mailbox tool implementations (P4) ─────────────────────────────────────

// toolList lists/searches messages (one tool over MessagesList — list and
// search are the same Gmail call). Returns bare pointers + pagination.
func (h *Handler) toolList(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Q         string `json:"q"`
		PageToken string `json:"page_token"`
	}
	if err := decodeArgs(raw, &a); err != nil {
		return nil, err
	}
	res, err := h.client.MessagesList(ctx, a.Q, a.PageToken)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	msgs := make([]map[string]any, 0, len(res.Messages))
	for _, m := range res.Messages {
		msgs = append(msgs, map[string]any{"id": m.ID, "thread_id": m.ThreadID})
	}
	return toolResultJSON(map[string]any{
		"messages":             msgs,
		"next_page_token":      res.NextPageToken,
		"result_size_estimate": res.ResultSizeEstimate,
	})
}

// toolRead fetches a full message and renders headers + snippet + label ids +
// attachment metadata (no blob download — decisions §1).
func (h *Handler) toolRead(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := decodeArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.ID == "" {
		return toolResultErr("id is required"), nil
	}
	m, err := h.client.MessageGet(ctx, a.ID, "full")
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(renderMessage(m))
}

// toolThread reads a whole thread and renders each message.
func (h *Handler) toolThread(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := decodeArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.ID == "" {
		return toolResultErr("id is required"), nil
	}
	t, err := h.client.ThreadGet(ctx, a.ID)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	msgs := make([]map[string]any, 0, len(t.Messages))
	for _, m := range t.Messages {
		msgs = append(msgs, renderMessage(m))
	}
	return toolResultJSON(map[string]any{
		"id":       t.ID,
		"snippet":  t.Snippet,
		"messages": msgs,
	})
}

// toolLabels lists the mailbox labels as {id, name, type}.
func (h *Handler) toolLabels(ctx context.Context) (map[string]any, error) {
	res, err := h.client.LabelsList(ctx)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	labels := make([]map[string]any, 0, len(res.Labels))
	for _, l := range res.Labels {
		labels = append(labels, map[string]any{"id": l.ID, "name": l.Name, "type": l.Type})
	}
	return toolResultJSON(map[string]any{"labels": labels})
}

// toolSend composes an RFC-2822 message from {to, subject, body}, base64url-
// encodes it, and sends it. The send surfaces as mail.sent on the next poll.
func (h *Handler) toolSend(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	to, subject, body, errRes := h.composeArgs(raw)
	if errRes != nil {
		return errRes, nil
	}
	rawMsg := buildRawMessage(to, subject, body)
	m, err := h.client.MessagesSend(ctx, rawMsg)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{
		"id":        m.ID,
		"thread_id": m.ThreadID,
		"label_ids": orEmpty(m.LabelIDs),
	})
}

// toolDraft composes the same RFC-2822 message and saves it as a draft (does not
// send — distinct from send, decisions §1).
func (h *Handler) toolDraft(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	to, subject, body, errRes := h.composeArgs(raw)
	if errRes != nil {
		return errRes, nil
	}
	rawMsg := buildRawMessage(to, subject, body)
	d, err := h.client.DraftCreate(ctx, rawMsg)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{
		"id": d.ID,
		"message": map[string]any{
			"id":        d.Message.ID,
			"thread_id": d.Message.ThreadID,
		},
	})
}

// toolLabel applies a label id to a message (MessageModify add). Archive =
// unlabel INBOX; mark-read = unlabel UNREAD (decisions §1).
func (h *Handler) toolLabel(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	return h.modifyLabel(ctx, raw, true)
}

// toolUnlabel removes a label id from a message (MessageModify remove).
func (h *Handler) toolUnlabel(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	return h.modifyLabel(ctx, raw, false)
}

// modifyLabel is the shared add/remove implementation behind label/unlabel.
func (h *Handler) modifyLabel(ctx context.Context, raw json.RawMessage, add bool) (map[string]any, error) {
	var a struct {
		ID      string `json:"id"`
		LabelID string `json:"label_id"`
	}
	if err := decodeArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.ID == "" {
		return toolResultErr("id is required"), nil
	}
	if a.LabelID == "" {
		return toolResultErr("label_id is required"), nil
	}
	var m gm.Message
	var err error
	if add {
		m, err = h.client.MessageModify(ctx, a.ID, []string{a.LabelID}, nil)
	} else {
		m, err = h.client.MessageModify(ctx, a.ID, nil, []string{a.LabelID})
	}
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{"id": m.ID, "label_ids": orEmpty(m.LabelIDs)})
}

// toolTrash moves a message to Trash (recoverable; emits mail.deleted next poll).
func (h *Handler) toolTrash(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := decodeArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.ID == "" {
		return toolResultErr("id is required"), nil
	}
	m, err := h.client.MessageTrash(ctx, a.ID)
	if err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{"id": m.ID, "label_ids": orEmpty(m.LabelIDs)})
}

// toolDelete PERMANENTLY deletes a message (not recoverable — decisions §1).
func (h *Handler) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := decodeArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.ID == "" {
		return toolResultErr("id is required"), nil
	}
	if err := h.client.MessageDelete(ctx, a.ID); err != nil {
		return toolResultErr(err.Error()), nil
	}
	return toolResultJSON(map[string]any{"id": a.ID, "deleted": true})
}

// ── mailbox helpers ──────────────────────────────────────────────────────

// decodeArgs unmarshals tool arguments into v, tolerating absent/empty params.
func decodeArgs(raw json.RawMessage, v any) error {
	if len(raw) == 0 {
		return nil
	}
	return json.Unmarshal(raw, v)
}

// composeArgs decodes + validates the shared {to, subject, body} send/draft
// inputs. On a validation miss it returns a tool-error result (second return is
// nil only when all three are present).
func (h *Handler) composeArgs(raw json.RawMessage) (to, subject, body string, errRes map[string]any) {
	var a struct {
		To      string `json:"to"`
		Subject string `json:"subject"`
		Body    string `json:"body"`
	}
	if err := decodeArgs(raw, &a); err != nil {
		return "", "", "", toolResultErr(err.Error())
	}
	if a.To == "" {
		return "", "", "", toolResultErr("to is required")
	}
	if a.Subject == "" {
		return "", "", "", toolResultErr("subject is required")
	}
	return a.To, a.Subject, a.Body, nil
}

// buildRawMessage assembles a minimal RFC-2822 message from structured fields
// and base64url-encodes it for Gmail's messages.send / drafts.create {"raw"}.
// The subject is RFC-2047 encoded-word wrapped so non-ASCII subjects survive;
// the body is sent as UTF-8 text/plain. From is omitted — Gmail fills it with
// the authenticated mailbox.
func buildRawMessage(to, subject, body string) string {
	var b strings.Builder
	fmt.Fprintf(&b, "To: %s\r\n", to)
	fmt.Fprintf(&b, "Subject: %s\r\n", mime.QEncoding.Encode("utf-8", subject))
	b.WriteString("MIME-Version: 1.0\r\n")
	b.WriteString("Content-Type: text/plain; charset=\"UTF-8\"\r\n")
	b.WriteString("\r\n")
	b.WriteString(body)
	return base64.URLEncoding.EncodeToString([]byte(b.String()))
}

// renderMessage projects a gmail.Message into the read/thread tool shape:
// headers as a name→value map, snippet, label ids, and attachment metadata
// (filename/size/mime — no blob).
func renderMessage(m gm.Message) map[string]any {
	headers := map[string]string{}
	collectHeaders(m.Payload, headers)
	atts := []map[string]any{}
	collectAttachments(m.Payload, &atts)
	return map[string]any{
		"id":            m.ID,
		"thread_id":     m.ThreadID,
		"label_ids":     orEmpty(m.LabelIDs),
		"snippet":       m.Snippet,
		"internal_date": m.InternalDate,
		"headers":       headers,
		"attachments":   atts,
	}
}

// collectHeaders flattens the top-level part's headers into name→value. Only the
// top-level payload headers (To/From/Subject/Date/…) are surfaced.
func collectHeaders(p gm.MessagePart, out map[string]string) {
	for _, hdr := range p.Headers {
		if _, seen := out[hdr.Name]; !seen {
			out[hdr.Name] = hdr.Value
		}
	}
}

// collectAttachments walks the MIME tree and records every part that carries a
// filename (an attachment) as {filename, size, mime_type} — metadata only.
func collectAttachments(p gm.MessagePart, out *[]map[string]any) {
	if p.Filename != "" {
		*out = append(*out, map[string]any{
			"filename":  p.Filename,
			"size":      p.Body.Size,
			"mime_type": p.MimeType,
		})
	}
	for _, child := range p.Parts {
		collectAttachments(child, out)
	}
}

// orEmpty normalizes a nil string slice to an empty one for stable JSON output.
func orEmpty(s []string) []string {
	if s == nil {
		return []string{}
	}
	return s
}

// ── shared helpers ──────────────────────────────────────────────────────

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
