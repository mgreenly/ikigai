// Package gmail is the domain for the gmail connector service: an external-OAuth
// mailbox connector that exposes the normal mailbox operations as MCP tools and
// polls the Gmail History API to emit mail-change events on the event plane
// (decisions §1). This file holds the shared request/response shapes for the
// Gmail REST v1 client (client.go) plus the domain error sentinels.
//
// The layering mirrors the dropbox chassis it is cloned from: client.go is the
// only HTTP-to-Gmail site (tokenSource + the REST method set), with typed
// result structs living here. The refresh token is never logged.
package gmail

import "errors"

// Error sentinels — the structured error vocabulary. ErrInvalidGrant is the
// load-bearing one (decisions §2): a dead/revoked refresh token surfaces as an
// `invalid_grant` error from the token endpoint, and the client must FAIL
// LOUDLY rather than spin/retry — the token needs human re-consent.
var (
	// ErrInvalidGrant: the refresh token was rejected by Google's token endpoint
	// (`invalid_grant`). The refresh token is dead/revoked and must be re-minted
	// via the consent CLI; callers must not retry on this.
	ErrInvalidGrant = errors.New("gmail: invalid_grant (refresh token revoked — re-consent required)")
	// ErrValidation: malformed input at the domain boundary.
	ErrValidation = errors.New("gmail: validation")
	// ErrNotFound: a requested resource (message/thread) is absent.
	ErrNotFound = errors.New("gmail: not found")
)

// Profile is the result of users.getProfile — the mailbox owner and the current
// historyId used to bootstrap / advance the producer's sync cursor (decisions
// §"Cursor lifecycle").
type Profile struct {
	EmailAddress  string `json:"emailAddress"`
	MessagesTotal int64  `json:"messagesTotal"`
	ThreadsTotal  int64  `json:"threadsTotal"`
	HistoryID     string `json:"historyId"`
}

// MessageRef is a bare {id, threadId} pointer as returned in list/history
// payloads before enrichment via MessageGet.
type MessageRef struct {
	ID       string `json:"id"`
	ThreadID string `json:"threadId"`
}

// HistoryMessageChange is one messagesAdded/messagesDeleted record: it wraps a
// message pointer (Gmail nests it under "message").
type HistoryMessageChange struct {
	Message MessageRef `json:"message"`
}

// HistoryLabelChange is one labelsAdded/labelsRemoved record: a message pointer
// plus the label ids added/removed (decisions §1 — labelsAdded:TRASH drives
// mail.deleted).
type HistoryLabelChange struct {
	Message  MessageRef `json:"message"`
	LabelIDs []string   `json:"labelIds"`
}

// History is one history record from users.history.list. Each carries the
// change lists relevant to the three derived events (decisions §1 table).
type History struct {
	ID              string                 `json:"id"`
	Messages        []MessageRef           `json:"messages"`
	MessagesAdded   []HistoryMessageChange `json:"messagesAdded"`
	MessagesDeleted []HistoryMessageChange `json:"messagesDeleted"`
	LabelsAdded     []HistoryLabelChange   `json:"labelsAdded"`
	LabelsRemoved   []HistoryLabelChange   `json:"labelsRemoved"`
}

// HistoryListResult is one page of users.history.list: the history records, the
// next page token (empty when drained), and the mailbox's current historyId
// (the cursor to advance to once the page is applied).
type HistoryListResult struct {
	History       []History `json:"history"`
	NextPageToken string    `json:"nextPageToken"`
	HistoryID     string    `json:"historyId"`
}

// MessagesListResult is one page of users.messages.list (powers both list and
// search — the same call): bare message pointers plus pagination.
type MessagesListResult struct {
	Messages           []MessageRef `json:"messages"`
	NextPageToken      string       `json:"nextPageToken"`
	ResultSizeEstimate int64        `json:"resultSizeEstimate"`
}

// Header is one RFC-2822 header name/value from a message's payload.
type Header struct {
	Name  string `json:"name"`
	Value string `json:"value"`
}

// Body is a message-part body: size and (for leaf text parts) base64url data, or
// an attachmentId for attachment parts (the blob is fetched separately — blob
// download is deferred, decisions §1).
type Body struct {
	AttachmentID string `json:"attachmentId"`
	Size         int64  `json:"size"`
	Data         string `json:"data"`
}

// MessagePart is one MIME part of a message payload (recursive: multipart
// messages nest Parts). Filename + the Content-Type header carry attachment
// metadata (filename/size/mime) without downloading the blob.
type MessagePart struct {
	PartID   string        `json:"partId"`
	MimeType string        `json:"mimeType"`
	Filename string        `json:"filename"`
	Headers  []Header      `json:"headers"`
	Body     Body          `json:"body"`
	Parts    []MessagePart `json:"parts"`
}

// Message is a full or partial users.messages.get result. Payload is present for
// format=full/metadata; LabelIDs drives the INBOX/SENT/TRASH event branching
// (decisions §1 table). Snippet is the short preview Gmail returns.
type Message struct {
	ID           string      `json:"id"`
	ThreadID     string      `json:"threadId"`
	LabelIDs     []string    `json:"labelIds"`
	Snippet      string      `json:"snippet"`
	HistoryID    string      `json:"historyId"`
	InternalDate string      `json:"internalDate"`
	Payload      MessagePart `json:"payload"`
	SizeEstimate int64       `json:"sizeEstimate"`
	Raw          string      `json:"raw"`
}

// Thread is a users.threads.get result: the thread id plus its messages.
type Thread struct {
	ID        string    `json:"id"`
	HistoryID string    `json:"historyId"`
	Snippet   string    `json:"snippet"`
	Messages  []Message `json:"messages"`
}

// Draft is a users.drafts.create result: the draft id plus the created message.
type Draft struct {
	ID      string  `json:"id"`
	Message Message `json:"message"`
}

// Label is one entry from users.labels.list.
type Label struct {
	ID                    string `json:"id"`
	Name                  string `json:"name"`
	Type                  string `json:"type"`
	MessageListVisibility string `json:"messageListVisibility"`
	LabelListVisibility   string `json:"labelListVisibility"`
}

// LabelsListResult is the users.labels.list response.
type LabelsListResult struct {
	Labels []Label `json:"labels"`
}

// Well-known Gmail system label ids the producer/MCP layers branch on
// (decisions §1): INBOX/SENT/TRASH classify added/labeled messages; UNREAD
// powers mark-read via MessageModify.
const (
	LabelInbox  = "INBOX"
	LabelSent   = "SENT"
	LabelTrash  = "TRASH"
	LabelUnread = "UNREAD"
)
