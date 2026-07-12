package gmail

// client.go is the only HTTP-to-Gmail site (decisions §1, plan P2). It holds a
// tokenSource (refresh -> short-lived access token, cached, force-refreshed on a
// 401) and the Gmail REST v1 calls the producer (P3) and MCP surface (P4) need,
// against base https://gmail.googleapis.com/gmail/v1/users/me/...:
//
//   Producer-side (P3): GetProfile, HistoryList, MessageGet.
//   MCP-side (P4):      MessagesList, ThreadGet, MessagesSend, DraftCreate,
//                       LabelsList, MessageModify, MessageTrash, MessageDelete.
//
// Two Google hosts are in play:
//   - OAuth: https://oauth2.googleapis.com/token              (form-encoded)
//   - REST:  https://gmail.googleapis.com/gmail/v1/users/me   (bearer)
//
// No disk, no db, no HTTP server. Pure API client. Token values are never
// logged. A dead/revoked refresh token surfaces as `invalid_grant` on the
// refresh call itself: the client FAILS LOUDLY (ErrInvalidGrant) — it does NOT
// spin/retry, because the token needs human re-consent (decisions §2).
import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"
)

// Google hosts (decisions §2). Kept as vars (not consts) solely so tests can
// redirect them at a stub; production always uses the real Google hosts.
var (
	hostOAuth = "https://oauth2.googleapis.com/token"
	hostREST  = "https://gmail.googleapis.com/gmail/v1/users/me"
)

// Config carries the three dedicated OAuth credentials the Client needs. They
// are read from the environment at the main.go boundary (via .envrc/direnv in
// dev, the launcher on the box) and passed in here — the Client never reads the
// environment itself and never logs these values (decisions §2).
type Config struct {
	ClientID     string // GMAIL_CLIENT_ID
	ClientSecret string // GMAIL_CLIENT_SECRET
	RefreshToken string // GMAIL_REFRESH_TOKEN
}

// Client is the Gmail REST v1 client. It is safe for concurrent use; the
// embedded tokenSource serializes refreshes.
type Client struct {
	cfg   Config
	http  *http.Client
	token *tokenSource
}

// NewClient builds a Client from cfg. httpClient, if non-nil, is used for both
// REST and token-refresh calls (tests inject a stub with a custom
// RoundTripper to run fully offline); a nil httpClient yields a default client.
func NewClient(cfg Config, httpClient *http.Client) *Client {
	hc := httpClient
	if hc == nil {
		hc = &http.Client{Timeout: 100 * time.Second}
	}
	c := &Client{cfg: cfg, http: hc}
	c.token = &tokenSource{
		clientID:     cfg.ClientID,
		clientSecret: cfg.ClientSecret,
		refreshToken: cfg.RefreshToken,
		httpClient:   hc,
	}
	return c
}

// ---------------------------------------------------------------------------
// tokenSource: refresh_token -> short-lived access token, cached, refreshed on
// expiry or on a 401 (decisions §2). The bearer never touches disk or logs.
// ---------------------------------------------------------------------------

type tokenSource struct {
	clientID     string
	clientSecret string
	refreshToken string
	httpClient   *http.Client

	mu        sync.Mutex
	accessTok string
	expiry    time.Time
}

// tokenExpirySlack refreshes a little before the real expiry so an in-flight
// call doesn't race the boundary.
const tokenExpirySlack = 60 * time.Second

// token returns a valid access token, refreshing if the cache is empty or within
// tokenExpirySlack of expiry. Pass force=true (used on a 401) to discard the
// cache and refresh unconditionally.
func (t *tokenSource) token(ctx context.Context, force bool) (string, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	if !force && t.accessTok != "" && time.Now().Before(t.expiry.Add(-tokenExpirySlack)) {
		return t.accessTok, nil
	}
	if err := t.refreshLocked(ctx); err != nil {
		return "", err
	}
	return t.accessTok, nil
}

// invalidate drops the cached token so the next token() forces a refresh. Called
// after a 401 from a REST call.
func (t *tokenSource) invalidate() {
	t.mu.Lock()
	t.accessTok = ""
	t.mu.Unlock()
}

// refreshLocked exchanges the refresh token for a fresh access token. Caller
// holds t.mu. Credentials go in the form body (client_id/client_secret) — never
// logged. Endpoint: POST https://oauth2.googleapis.com/token.
//
// A dead/revoked refresh token returns HTTP 400 with {"error":"invalid_grant"};
// that is mapped to ErrInvalidGrant and FAILS LOUDLY — callers must not retry,
// the token needs human re-consent (decisions §2).
func (t *tokenSource) refreshLocked(ctx context.Context) error {
	form := url.Values{}
	form.Set("grant_type", "refresh_token")
	form.Set("client_id", t.clientID)
	form.Set("client_secret", t.clientSecret)
	form.Set("refresh_token", t.refreshToken)

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, hostOAuth, strings.NewReader(form.Encode()))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	resp, err := t.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("gmail token refresh: %w", err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(resp.Body, 1<<16))

	if resp.StatusCode != http.StatusOK {
		// Google's token error envelope: {"error":"...","error_description":"..."}.
		// The body may echo a description but never the token; safe to surface.
		var oe struct {
			Error       string `json:"error"`
			Description string `json:"error_description"`
		}
		_ = json.Unmarshal(body, &oe)
		if oe.Error == "invalid_grant" {
			// FAIL LOUDLY — the refresh token is dead; do NOT spin/retry.
			return fmt.Errorf("%w: %s", ErrInvalidGrant, strings.TrimSpace(oe.Description))
		}
		return fmt.Errorf("gmail token refresh: status %d: %s", resp.StatusCode, strings.TrimSpace(string(body)))
	}

	var tr struct {
		AccessToken string `json:"access_token"`
		ExpiresIn   int    `json:"expires_in"`
		TokenType   string `json:"token_type"`
	}
	if err := json.Unmarshal(body, &tr); err != nil {
		return fmt.Errorf("gmail token refresh: decode: %w", err)
	}
	if tr.AccessToken == "" {
		return fmt.Errorf("gmail token refresh: empty access_token")
	}
	t.accessTok = tr.AccessToken
	exp := tr.ExpiresIn
	if exp <= 0 {
		exp = 3600 // Google access tokens are ~1h; default if omitted.
	}
	t.expiry = time.Now().Add(time.Duration(exp) * time.Second)
	return nil
}

// ---------------------------------------------------------------------------
// rpcCall: issue an authenticated REST request, retrying once on a 401 with a
// forced token refresh (decisions §2). Decodes a 2xx JSON body into out.
// ---------------------------------------------------------------------------

// rpcCall issues method to hostREST+path (with optional query) carrying an
// optional JSON body and the bearer, decoding a 2xx response into out. On a 401
// it invalidates the cache, forces one token refresh, and retries exactly once.
// A non-2xx (after the retry) yields a descriptive error including status + a
// body snippet. ErrInvalidGrant from the refresh propagates unwrapped so callers
// can detect the dead-token case.
func (c *Client) rpcCall(ctx context.Context, method, path string, query url.Values, reqBody, out any) error {
	var payload []byte
	if reqBody != nil {
		var err error
		payload, err = json.Marshal(reqBody)
		if err != nil {
			return err
		}
	}
	target := hostREST + path
	if len(query) > 0 {
		target += "?" + query.Encode()
	}

	do := func() (*http.Response, error) {
		tok, terr := c.token.token(ctx, false)
		if terr != nil {
			return nil, terr
		}
		var bodyReader io.Reader
		if payload != nil {
			bodyReader = bytes.NewReader(payload)
		}
		req, rerr := http.NewRequestWithContext(ctx, method, target, bodyReader)
		if rerr != nil {
			return nil, rerr
		}
		req.Header.Set("Authorization", "Bearer "+tok)
		if payload != nil {
			req.Header.Set("Content-Type", "application/json")
		}
		return c.http.Do(req)
	}

	resp, err := do()
	if err != nil {
		return err
	}
	if resp.StatusCode == http.StatusUnauthorized {
		resp.Body.Close()
		c.token.invalidate()
		// Force a refresh and retry once. A dead token surfaces here as
		// ErrInvalidGrant and stops the retry — by design.
		if _, terr := c.token.token(ctx, true); terr != nil {
			return terr
		}
		resp, err = do()
		if err != nil {
			return err
		}
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return c.statusError(resp.StatusCode, body)
	}
	if out != nil {
		if err := json.Unmarshal(body, out); err != nil {
			return fmt.Errorf("gmail: decode %s: %w", path, err)
		}
	}
	return nil
}

// statusError builds a descriptive error from a non-2xx Gmail response,
// preferring Google's {"error":{"code","message"}} envelope and falling back to
// a raw body snippet.
func (c *Client) statusError(status int, body []byte) error {
	var ge struct {
		Error struct {
			Code    int    `json:"code"`
			Message string `json:"message"`
			Status  string `json:"status"`
		} `json:"error"`
	}
	if json.Unmarshal(body, &ge) == nil && ge.Error.Message != "" {
		return fmt.Errorf("gmail: status %d: %s", status, strings.TrimSpace(ge.Error.Message))
	}
	snippet := strings.TrimSpace(string(body))
	if len(snippet) > 512 {
		snippet = snippet[:512] + "…"
	}
	return fmt.Errorf("gmail: status %d: %s", status, snippet)
}

// ---------------------------------------------------------------------------
// Producer-side methods (P3 consumes): GetProfile, HistoryList, MessageGet.
// ---------------------------------------------------------------------------

// GetProfile returns the mailbox profile, including the current historyId used
// to bootstrap / resync the producer cursor (decisions §"Cursor lifecycle").
func (c *Client) GetProfile(ctx context.Context) (Profile, error) {
	var p Profile
	if err := c.rpcCall(ctx, http.MethodGet, "/profile", nil, nil, &p); err != nil {
		return Profile{}, err
	}
	return p, nil
}

// HistoryList returns one page of the change history since startHistoryId (the
// stored cursor). pageToken drains subsequent pages ("" for the first). A 404
// means the cursor is too old (Gmail retains ~1 week) and the caller must resync
// from GetProfile — decisions §"Stale-cursor resync"; surfaced as ErrNotFound.
func (c *Client) HistoryList(ctx context.Context, startHistoryID, pageToken string) (HistoryListResult, error) {
	if startHistoryID == "" {
		return HistoryListResult{}, fmt.Errorf("%w: startHistoryId required", ErrValidation)
	}
	q := url.Values{}
	q.Set("startHistoryId", startHistoryID)
	if pageToken != "" {
		q.Set("pageToken", pageToken)
	}
	var r HistoryListResult
	if err := c.rpcCall(ctx, http.MethodGet, "/history", q, nil, &r); err != nil {
		if isStatus(err, http.StatusNotFound) {
			return HistoryListResult{}, fmt.Errorf("%w: history cursor %s expired", ErrNotFound, startHistoryID)
		}
		return HistoryListResult{}, err
	}
	return r, nil
}

// MessageGet fetches a single message. format is the Gmail format param
// ("full", "metadata", "minimal", "raw"); "" defaults to "full". The producer
// uses it to enrich added messages with headers/labels/snippet (and attachment
// metadata only — no blob download, decisions §1).
func (c *Client) MessageGet(ctx context.Context, id, format string) (Message, error) {
	if id == "" {
		return Message{}, fmt.Errorf("%w: message id required", ErrValidation)
	}
	q := url.Values{}
	if format == "" {
		format = "full"
	}
	q.Set("format", format)
	var m Message
	if err := c.rpcCall(ctx, http.MethodGet, "/messages/"+url.PathEscape(id), q, nil, &m); err != nil {
		if isStatus(err, http.StatusNotFound) {
			return Message{}, fmt.Errorf("%w: message %s", ErrNotFound, id)
		}
		return Message{}, err
	}
	return m, nil
}

// AttachmentGet fetches and decodes one Gmail message attachment body.
func (c *Client) AttachmentGet(ctx context.Context, messageID, attachmentID string) ([]byte, error) {
	if messageID == "" || attachmentID == "" {
		return nil, fmt.Errorf("%w: message and attachment ids required", ErrValidation)
	}
	var body Body
	path := "/messages/" + url.PathEscape(messageID) + "/attachments/" + url.PathEscape(attachmentID)
	if err := c.rpcCall(ctx, http.MethodGet, path, nil, nil, &body); err != nil {
		if isStatus(err, http.StatusNotFound) {
			return nil, fmt.Errorf("%w: attachment %s", ErrNotFound, attachmentID)
		}
		return nil, err
	}
	decoded, err := base64.URLEncoding.DecodeString(body.Data)
	if err != nil {
		decoded, err = base64.RawURLEncoding.DecodeString(body.Data)
	}
	if err != nil {
		return nil, fmt.Errorf("gmail: decode attachment %s: %w", attachmentID, err)
	}
	return decoded, nil
}

// ---------------------------------------------------------------------------
// MCP-side methods (P4 consumes).
// ---------------------------------------------------------------------------

// MessagesList lists/searches messages (the one call behind both list and
// search). q is an optional Gmail search query ("from:", "is:unread", …);
// pageToken drains pages ("" for the first).
func (c *Client) MessagesList(ctx context.Context, q, pageToken string) (MessagesListResult, error) {
	query := url.Values{}
	if q != "" {
		query.Set("q", q)
	}
	if pageToken != "" {
		query.Set("pageToken", pageToken)
	}
	var r MessagesListResult
	if err := c.rpcCall(ctx, http.MethodGet, "/messages", query, nil, &r); err != nil {
		return MessagesListResult{}, err
	}
	return r, nil
}

// ThreadGet fetches a whole thread (its messages) by id.
func (c *Client) ThreadGet(ctx context.Context, id string) (Thread, error) {
	if id == "" {
		return Thread{}, fmt.Errorf("%w: thread id required", ErrValidation)
	}
	var t Thread
	if err := c.rpcCall(ctx, http.MethodGet, "/threads/"+url.PathEscape(id), nil, nil, &t); err != nil {
		if isStatus(err, http.StatusNotFound) {
			return Thread{}, fmt.Errorf("%w: thread %s", ErrNotFound, id)
		}
		return Thread{}, err
	}
	return t, nil
}

// MessagesSend sends a message. raw is the full RFC-2822 message, base64url
// encoded (the caller encodes; Gmail's messages.send takes {"raw": ...}). It
// returns the created message (carrying the SENT label, which the producer
// observes as a sent event on the next poll — decisions §1).
func (c *Client) MessagesSend(ctx context.Context, raw string) (Message, error) {
	if raw == "" {
		return Message{}, fmt.Errorf("%w: raw message required", ErrValidation)
	}
	reqBody := map[string]string{"raw": raw}
	var m Message
	if err := c.rpcCall(ctx, http.MethodPost, "/messages/send", nil, reqBody, &m); err != nil {
		return Message{}, err
	}
	return m, nil
}

// DraftCreate creates a draft from a base64url RFC-2822 message (distinct from
// send — decisions §1).
func (c *Client) DraftCreate(ctx context.Context, raw string) (Draft, error) {
	if raw == "" {
		return Draft{}, fmt.Errorf("%w: raw message required", ErrValidation)
	}
	reqBody := map[string]any{"message": map[string]string{"raw": raw}}
	var d Draft
	if err := c.rpcCall(ctx, http.MethodPost, "/drafts", nil, reqBody, &d); err != nil {
		return Draft{}, err
	}
	return d, nil
}

// LabelsList lists the mailbox's available labels (for the labels tool and for
// resolving label ids in label/unlabel).
func (c *Client) LabelsList(ctx context.Context) (LabelsListResult, error) {
	var r LabelsListResult
	if err := c.rpcCall(ctx, http.MethodGet, "/labels", nil, nil, &r); err != nil {
		return LabelsListResult{}, err
	}
	return r, nil
}

// MessageModify adds and/or removes label ids on a message (powers
// label/unlabel, archive = remove INBOX, mark-read = remove UNREAD —
// decisions §1). add/remove may each be empty.
func (c *Client) MessageModify(ctx context.Context, id string, add, remove []string) (Message, error) {
	if id == "" {
		return Message{}, fmt.Errorf("%w: message id required", ErrValidation)
	}
	reqBody := map[string]any{}
	if len(add) > 0 {
		reqBody["addLabelIds"] = add
	}
	if len(remove) > 0 {
		reqBody["removeLabelIds"] = remove
	}
	var m Message
	if err := c.rpcCall(ctx, http.MethodPost, "/messages/"+url.PathEscape(id)+"/modify", nil, reqBody, &m); err != nil {
		if isStatus(err, http.StatusNotFound) {
			return Message{}, fmt.Errorf("%w: message %s", ErrNotFound, id)
		}
		return Message{}, err
	}
	return m, nil
}

// MessageTrash moves a message to Trash (recoverable). The labelsAdded:TRASH
// this produces is what the producer derives deleted from (decisions §1).
func (c *Client) MessageTrash(ctx context.Context, id string) (Message, error) {
	if id == "" {
		return Message{}, fmt.Errorf("%w: message id required", ErrValidation)
	}
	var m Message
	if err := c.rpcCall(ctx, http.MethodPost, "/messages/"+url.PathEscape(id)+"/trash", nil, nil, &m); err != nil {
		if isStatus(err, http.StatusNotFound) {
			return Message{}, fmt.Errorf("%w: message %s", ErrNotFound, id)
		}
		return Message{}, err
	}
	return m, nil
}

// MessageDelete permanently deletes a message (not recoverable; the full-scope
// restricted capability — decisions §1). No response body on success.
func (c *Client) MessageDelete(ctx context.Context, id string) error {
	if id == "" {
		return fmt.Errorf("%w: message id required", ErrValidation)
	}
	if err := c.rpcCall(ctx, http.MethodDelete, "/messages/"+url.PathEscape(id), nil, nil, nil); err != nil {
		if isStatus(err, http.StatusNotFound) {
			return fmt.Errorf("%w: message %s", ErrNotFound, id)
		}
		return err
	}
	return nil
}

// isStatus reports whether err is a statusError carrying the given HTTP status.
// (statusError stringifies as "gmail: status N: ..." — checked by prefix.)
func isStatus(err error, status int) bool {
	if err == nil {
		return false
	}
	// Sentinels wrap their own message; only raw statusErrors carry the prefix.
	if errors.Is(err, ErrInvalidGrant) {
		return false
	}
	return strings.HasPrefix(err.Error(), fmt.Sprintf("gmail: status %d:", status))
}
