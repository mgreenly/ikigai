// Package ingest is wiki's interactive front-door logic: the MCP write doors
// (ingest_text, ingest_url) and the status poll (design §2.1). A front door is
// any code path that receives input and calls the single inbox.Accept write
// function; this package holds the two MCP doors and the receipt/status contract
// they expose. The eventplane consumer doors are the other front doors and live
// in internal/consume; both funnel into the same inbox.Accept.
//
// The MCP return contract is a RECEIPT, not a job (§2.1): IngestText / IngestURL
// return inbox id + sha256 + duplicate flag, meaning "recorded, will be
// integrated." No job id exists yet — a caller that cares polls Status against
// the inbox id.
//
// On an oversized refusal a door returns inbox.ErrTooLarge AND emits
// wiki.ingest_refused (§8); the interactive caller also sees the error directly.
package ingest

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"

	"wiki/internal/events"
	"wiki/internal/inbox"
)

// Accepter is the inbox write surface the doors depend on (inbox.Store), narrowed
// for testability.
type Accepter interface {
	Accept(ctx context.Context, owner, kind, source, mime, title, tags string, bytes []byte) (inbox.Receipt, error)
	MaxBytes() int64
}

// Refuser emits wiki.ingest_refused for an oversized door refusal (§8). A nil
// Refuser makes the emit a no-op (the door still returns the error).
type Refuser interface {
	IngestRefused(ctx context.Context, ev events.IngestRefused) error
}

// Service is the interactive ingest front door. It carries the inbox writer, the
// shared DB (for the status query), the refusal emitter, and an HTTP client for
// ingest_url fetches. Constructed once at the composition root.
type Service struct {
	inbox  Accepter
	db     *sql.DB
	refuse Refuser
	http   *http.Client
}

// New builds a Service. A nil http client defaults to http.DefaultClient.
func New(in Accepter, db *sql.DB, refuse Refuser, httpc *http.Client) *Service {
	if httpc == nil {
		httpc = http.DefaultClient
	}
	return &Service{inbox: in, db: db, refuse: refuse, http: httpc}
}

// IngestText is the ingest_text door (§2.1): Accept the bytes directly as a
// document. owner is the authenticated caller (X-Owner-Email). source defaults to
// "mcp:ingest_text" when the caller supplies none; a caller-supplied source is
// used verbatim. tags is a JSON-array string ("[]" when empty).
func (s *Service) IngestText(ctx context.Context, owner, title, source, tags string, text []byte) (inbox.Receipt, error) {
	if source == "" {
		source = "mcp:ingest_text"
	}
	return s.accept(ctx, owner, "ingest_text", inbox.KindDocument, source, "text/plain", title, tags, text)
}

// IngestURL is the ingest_url door (§2.1): fetch the URL, extract its text
// server-side, then Accept(kind=document). The receipt is identical to
// ingest_text's. source is "url:<url>"; the title defaults to the URL.
func (s *Service) IngestURL(ctx context.Context, owner, url, tags string) (inbox.Receipt, error) {
	body, mime, err := s.fetch(ctx, url)
	if err != nil {
		return inbox.Receipt{}, fmt.Errorf("ingest_url fetch: %w", err)
	}
	source := "url:" + url
	return s.accept(ctx, owner, "ingest_url", inbox.KindDocument, source, mime, url, tags, body)
}

// accept is the shared door tail: Accept, and on an oversized refusal emit
// wiki.ingest_refused (§8) before returning the error so the interactive caller
// sees it too.
func (s *Service) accept(ctx context.Context, owner, door, kind, source, mime, title, tags string, body []byte) (inbox.Receipt, error) {
	rec, err := s.inbox.Accept(ctx, owner, kind, source, mime, title, tags, body)
	if errors.Is(err, inbox.ErrTooLarge) {
		if s.refuse != nil {
			// Best-effort notify; the door's own error is the caller's signal.
			_ = s.refuse.IngestRefused(ctx, events.IngestRefused{
				Door:   door,
				Source: source,
				Size:   int64(len(body)),
				Cap:    s.inbox.MaxBytes(),
			})
		}
		return inbox.Receipt{}, err
	}
	return rec, err
}

// fetch GETs a URL and returns its body + content-type. ingest_url's
// server-side "extract" in P3 is the raw fetched bytes; richer HTML→text
// extraction is a later refinement (the extract PROMPT pass is P6a; this is the
// transport fetch, not the LLM extraction).
func (s *Service) fetch(ctx context.Context, url string) ([]byte, string, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return nil, "", err
	}
	resp, err := s.http.Do(req)
	if err != nil {
		return nil, "", err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return nil, "", fmt.Errorf("status %d", resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, "", err
	}
	mime := resp.Header.Get("Content-Type")
	if i := strings.IndexByte(mime, ';'); i >= 0 {
		mime = strings.TrimSpace(mime[:i])
	}
	return body, mime, nil
}

// State is the coarse integration state Status reports for an inbox id (§2.1).
type State struct {
	ID        string `json:"id"`
	State     string `json:"state"`               // pending|running|succeeded|crashed|dead
	LastError string `json:"last_error,omitempty"` // present on a crashed/dead row
}

// Status polls the integration state of an inbox row by id (§2.1). It derives the
// state from the inbox row plus the most recent run that consumed it:
//
//   - dead_at set                  → "dead";
//   - a run exists for this row     → that run's status (running|succeeded|crashed;
//     a 'failed' run between retries presents as "pending" — it will be requeued);
//   - integrated_by set, no run row → "succeeded" (consumed);
//   - otherwise                     → "pending" (accepted, not yet integrated).
//
// An unknown id is a not-found error.
func (s *Service) Status(ctx context.Context, id string) (State, error) {
	var (
		integratedBy string
		deadAt       sql.NullInt64
	)
	err := s.db.QueryRowContext(ctx,
		`SELECT integrated_by, dead_at FROM inbox WHERE id = ?`, id).
		Scan(&integratedBy, &deadAt)
	if errors.Is(err, sql.ErrNoRows) {
		return State{}, fmt.Errorf("inbox id %q not found", id)
	}
	if err != nil {
		return State{}, fmt.Errorf("status query: %w", err)
	}

	st := State{ID: id}
	if deadAt.Valid {
		st.State = "dead"
		st.LastError = s.lastRunError(ctx, id)
		return st, nil
	}

	// Most recent run caused by this row (the runs table exists from P1; the
	// worker that writes it lands in P4, so a P3-era row simply has no run yet).
	var runStatus string
	rerr := s.db.QueryRowContext(ctx,
		`SELECT status FROM runs WHERE caused_by = ? ORDER BY started_at DESC LIMIT 1`, id).
		Scan(&runStatus)
	switch {
	case errors.Is(rerr, sql.ErrNoRows):
		if integratedBy != "" {
			st.State = "succeeded"
		} else {
			st.State = "pending"
		}
	case rerr != nil:
		return State{}, fmt.Errorf("status run query: %w", rerr)
	default:
		switch runStatus {
		case "running":
			st.State = "running"
		case "succeeded":
			st.State = "succeeded"
		case "crashed":
			st.State = "crashed"
			st.LastError = s.lastRunError(ctx, id)
		default: // 'failed' between retries: not terminal, will be requeued
			st.State = "pending"
		}
	}
	return st, nil
}

// StatusAny is Status returning the State as a transport-agnostic any, so the MCP
// handler can depend on a small interface without importing this package's State
// type. It is the method the mcp.Ingester interface binds to.
func (s *Service) StatusAny(ctx context.Context, id string) (any, error) {
	return s.Status(ctx, id)
}

// lastRunError best-effort reads a last-error string from the most recent run's
// usage JSON. The runs table has no dedicated error column in §12; a richer error
// surface lands with the failure policy (P5). For now it returns "" — the State
// shape is stable, the detail fills later.
func (s *Service) lastRunError(_ context.Context, _ string) string {
	return ""
}
