package gmail

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"log/slog"
	"strconv"
	"time"
)

// sync.go is the producer engine (decisions §1 producer + scheduled + cursor
// lifecycle). Unlike dropbox's longpoll loop, gmail POLLS the Gmail History API
// on a fixed interval (GMAIL_POLL_INTERVAL, default 60s). Each tick:
//
//  1. read the stored historyId cursor;
//  2. drain users.history.list(startHistoryId=cursor) across all pages;
//  3. derive mail.* events per the decisions §1 table (enriching added messages
//     with one messages.get each — no batching, fine at single-owner volume);
//  4. in ONE transaction, Append every derived event AND advance the cursor to
//     the page set's new historyId (cron's fireOne pattern), then Ring().
//
// The single-tx step (3→4 commit) is the load-bearing correctness property: a
// crash never double-emits or skips, because "emitted" and "cursor advanced"
// commit together or not at all.
//
// Cursor lifecycle (decisions §"Cursor lifecycle"):
//   - Fresh boot (no stored cursor): seed from getProfile().historyId, emit
//     NOTHING for pre-existing mail.
//   - Stale-cursor resync (history.list 404 → ErrNotFound; Gmail retains ~1
//     week): treated identically to a fresh boot — reset the cursor to the
//     current getProfile().historyId, emit nothing for the gap, log a warning.
//     Never backfill.

// gmailAPI is the slice of the Gmail client the engine needs. It is an interface
// so a unit test can inject a fake returning canned profile/history/messages and
// exercise the derivation + atomic-advance rules deterministically without live
// Gmail. *Client satisfies it.
type gmailAPI interface {
	GetProfile(ctx context.Context) (Profile, error)
	HistoryList(ctx context.Context, startHistoryID, pageToken string) (HistoryListResult, error)
	MessageGet(ctx context.Context, id, format string) (Message, error)
}

// Engine drives the poll loop. It owns the cursor advance and event derivation;
// the EventSink (when wired) makes it a producer.
type Engine struct {
	db     *sql.DB
	store  *Store
	client gmailAPI
	sink   EventSink
	log    *slog.Logger

	interval time.Duration
}

// EngineOptions configures NewEngine.
type EngineOptions struct {
	DB       *sql.DB
	Store    *Store
	Client   gmailAPI
	Sink     EventSink // nil disables emission (events still derived, just not appended)
	Logger   *slog.Logger
	Interval time.Duration
}

const defaultInterval = 60 * time.Second

// NewEngine builds an Engine. A zero Interval defaults to 60s (decisions §1).
func NewEngine(opts EngineOptions) *Engine {
	log := opts.Logger
	if log == nil {
		log = slog.Default()
	}
	store := opts.Store
	if store == nil {
		store = NewStore()
	}
	iv := opts.Interval
	if iv <= 0 {
		iv = defaultInterval
	}
	return &Engine{
		db:       opts.DB,
		store:    store,
		client:   opts.Client,
		sink:     opts.Sink,
		log:      log,
		interval: iv,
	}
}

// SetSink attaches the producer EventSink after construction. main wires it in
// the Producer hook (which appkit runs after Handlers builds the engine but
// before Workers starts the loop), so the sink is set by the time Run ticks.
func (e *Engine) SetSink(sink EventSink) { e.sink = sink }

// Run blocks running the poll loop until ctx is cancelled, then returns nil
// (a Spec.Workers entry: a SIGTERM cancels ctx and unwinds the server with it).
// It first bootstraps the cursor (seed on fresh boot), then ticks every interval.
func (e *Engine) Run(ctx context.Context) error {
	e.log.Info("gmail producer starting", "interval", e.interval.String())
	if err := e.bootstrap(ctx); err != nil {
		if ctx.Err() != nil {
			return nil
		}
		// A dead refresh token surfaces here: fail loudly, stop the poll cleanly
		// (do not spin) — the token needs human re-consent (decisions §2).
		if errors.Is(err, ErrInvalidGrant) {
			e.log.Error("gmail producer stopping: refresh token revoked — re-consent required", "err", err.Error())
			return nil
		}
		// Other bootstrap failures (e.g. transient network) are logged; the steady
		// loop retries the seed on the next tick.
		e.log.Error("gmail producer bootstrap failed", "err", err.Error())
	}

	t := time.NewTicker(e.interval)
	defer t.Stop()
	for {
		select {
		case <-ctx.Done():
			e.log.Info("gmail producer stopped")
			return nil
		case <-t.C:
			if err := e.Poll(ctx); err != nil {
				if ctx.Err() != nil {
					e.log.Info("gmail producer stopped")
					return nil
				}
				if errors.Is(err, ErrInvalidGrant) {
					e.log.Error("gmail producer stopping: refresh token revoked — re-consent required", "err", err.Error())
					return nil
				}
				e.log.Error("gmail poll failed", "err", err.Error())
			}
		}
	}
}

// bootstrap seeds the cursor from getProfile().historyId when none is stored
// (fresh boot), emitting nothing for pre-existing mail. When a cursor already
// exists, it is a no-op (the poll loop resumes from it).
func (e *Engine) bootstrap(ctx context.Context) error {
	_, ok, err := e.readCursor(ctx)
	if err != nil {
		return err
	}
	if ok {
		e.log.Info("gmail producer resuming from persisted historyId")
		return nil
	}
	return e.seedCursor(ctx, "fresh boot")
}

// seedCursor resets the stored cursor to the live getProfile().historyId and
// emits nothing for the gap (used for both fresh boot and stale-cursor resync,
// decisions §"Cursor lifecycle").
func (e *Engine) seedCursor(ctx context.Context, reason string) error {
	prof, err := e.client.GetProfile(ctx)
	if err != nil {
		return fmt.Errorf("getProfile (%s): %w", reason, err)
	}
	if err := e.setCursor(ctx, prof.HistoryID); err != nil {
		return err
	}
	e.log.Info("gmail producer cursor seeded", "reason", reason, "history_id", prof.HistoryID, "email", prof.EmailAddress)
	return nil
}

// Poll runs one poll cycle: drain history from the stored cursor, derive events,
// and atomically append them + advance the cursor in one tx. It is exported so a
// test (or a future sync_now tool) can drive a single cycle deterministically.
//
// A 404 from history.list (ErrNotFound) means the cursor is stale (Gmail retains
// ~1 week); Poll resyncs by re-seeding from getProfile and emits nothing for the
// gap (decisions §"Stale-cursor resync"). No stored cursor (e.g. bootstrap
// failed earlier) triggers a seed and returns — events flow from the next tick.
func (e *Engine) Poll(ctx context.Context) error {
	cursor, ok, err := e.readCursor(ctx)
	if err != nil {
		return err
	}
	if !ok {
		return e.seedCursor(ctx, "no stored cursor")
	}

	events, newHistoryID, err := e.drain(ctx, cursor)
	if err != nil {
		if errors.Is(err, ErrNotFound) {
			e.log.Warn("gmail history cursor stale — resyncing from current mailbox state (no backfill)", "stale_history_id", cursor)
			return e.seedCursor(ctx, "stale-cursor resync")
		}
		return err
	}

	// No advance signalled (empty page set with no newer historyId): nothing to do.
	if newHistoryID == "" || newHistoryID == cursor {
		if len(events) == 0 {
			return nil
		}
		// Defensive: events without a forward cursor would risk replay; treat the
		// drained historyId as authoritative even if equal is unexpected.
	}

	if err := e.commit(ctx, events, newHistoryID); err != nil {
		return err
	}
	if e.sink != nil && len(events) > 0 {
		e.sink.Ring()
	}
	if len(events) > 0 {
		e.log.Info("gmail poll emitted", "count", len(events), "history_id", newHistoryID)
	}
	return nil
}

// drain pages users.history.list from cursor, deriving events across every page,
// and returns the derived events plus the new historyId to advance to. Added
// messages are enriched with one messages.get each; a message that appears in
// multiple history records is fetched and emitted at most once per drain (Gmail
// can repeat a message across overlapping records). Derivation order: a
// send-to-self yields both a SENT copy and an INBOX copy → both sent and
// received events (decisions §1).
func (e *Engine) drain(ctx context.Context, cursor string) ([]MailEvent, string, error) {
	var events []MailEvent
	// Dedup added/trashed message ids across the whole drain so overlapping
	// history records don't emit the same event twice.
	seenAdded := map[string]bool{}
	seenTrashed := map[string]bool{}
	// Cache MessageGet results within a drain (a message may recur across records).
	msgCache := map[string]*Message{}

	newHistoryID := cursor
	pageToken := ""
	for {
		if ctx.Err() != nil {
			return nil, "", ctx.Err()
		}
		res, err := e.client.HistoryList(ctx, cursor, pageToken)
		if err != nil {
			return nil, "", err
		}
		for _, h := range res.History {
			// messagesAdded → received (INBOX) and/or sent (SENT, not INBOX).
			for _, ma := range h.MessagesAdded {
				id := ma.Message.ID
				if id == "" || seenAdded[id] {
					continue
				}
				msg, err := e.getMessage(ctx, id, msgCache)
				if err != nil {
					// A message added then quickly deleted may 404 on get; skip it
					// (best-effort, decisions philosophy) rather than wedge the drain.
					if errors.Is(err, ErrNotFound) {
						e.log.Warn("gmail skipping added message that is no longer fetchable", "id", id)
						seenAdded[id] = true
						continue
					}
					return nil, "", err
				}
				derived := deriveAddedEvents(msg)
				if len(derived) > 0 {
					seenAdded[id] = true
					events = append(events, derived...)
				}
			}
			// labelsAdded: TRASH → deleted.
			for _, la := range h.LabelsAdded {
				if !containsLabel(la.LabelIDs, LabelTrash) {
					continue
				}
				id := la.Message.ID
				if id == "" || seenTrashed[id] {
					continue
				}
				msg, err := e.getMessage(ctx, id, msgCache)
				if err != nil {
					if errors.Is(err, ErrNotFound) {
						e.log.Warn("gmail skipping trashed message that is no longer fetchable", "id", id)
						seenTrashed[id] = true
						continue
					}
					return nil, "", err
				}
				seenTrashed[id] = true
				events = append(events, deriveDeletedEvent(msg))
			}
		}
		if res.HistoryID != "" {
			newHistoryID = res.HistoryID
		}
		if res.NextPageToken == "" {
			break
		}
		pageToken = res.NextPageToken
	}
	return events, newHistoryID, nil
}

// getMessage fetches a message (format=metadata is enough for headers/labels/
// snippet) using a per-drain cache so a recurring id is fetched once.
func (e *Engine) getMessage(ctx context.Context, id string, cache map[string]*Message) (*Message, error) {
	if m, ok := cache[id]; ok {
		return m, nil
	}
	m, err := e.client.MessageGet(ctx, id, "metadata")
	if err != nil {
		return nil, err
	}
	cache[id] = &m
	return &m, nil
}

// deriveAddedEvents maps an added message to its mail.* events (decisions §1
// table): INBOX → received, SENT → sent. The two rules are INDEPENDENT,
// matching the decisions doc's explicit promise that a send-to-self "yields BOTH
// a SENT copy and an INBOX copy → emits both sent and received events." Gmail
// realizes a send-to-self as ONE message carrying BOTH SENT and INBOX labels
// (verified live: a self-send lands [UNREAD, SENT, INBOX]) rather than two
// separate copies, so both events must be derived from that single message; the
// doc's "two copies" wording describes the intent, not the wire reality. A normal
// outbound send (SENT, no INBOX) emits only sent; a normal inbound (INBOX,
// no SENT) emits only received; a message with neither (e.g. a draft) emits
// nothing.
func deriveAddedEvents(m *Message) []MailEvent {
	occurred := internalDateToRFC3339Nano(m.InternalDate)
	var out []MailEvent
	if containsLabel(m.LabelIDs, LabelSent) {
		out = append(out, MailEvent{
			Type:       KindSent,
			ID:         m.ID,
			ThreadID:   m.ThreadID,
			To:         header(m, "To"),
			Subject:    header(m, "Subject"),
			Snippet:    m.Snippet,
			OccurredAt: occurred,
		})
	}
	if containsLabel(m.LabelIDs, LabelInbox) {
		out = append(out, MailEvent{
			Type:       KindReceived,
			ID:         m.ID,
			ThreadID:   m.ThreadID,
			From:       header(m, "From"),
			Subject:    header(m, "Subject"),
			Snippet:    m.Snippet,
			OccurredAt: occurred,
		})
	}
	return out
}

// deriveDeletedEvent maps a trashed message to a deleted event.
func deriveDeletedEvent(m *Message) MailEvent {
	return MailEvent{
		Type:       KindDeleted,
		ID:         m.ID,
		ThreadID:   m.ThreadID,
		Subject:    header(m, "Subject"),
		OccurredAt: time.Now().UTC().Format(eventTimeFormat),
	}
}

// commit appends every derived event AND advances the cursor to newHistoryID in
// ONE transaction (cron fireOne pattern, decisions §1). If no events were
// derived this still advances the cursor so a quiet poll moves forward.
func (e *Engine) commit(ctx context.Context, events []MailEvent, newHistoryID string) error {
	tx, err := e.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	if e.sink != nil {
		for _, ev := range events {
			if err := e.sink.AppendMailEvent(tx, ev); err != nil {
				return err
			}
		}
	}
	if err := e.store.SetHistoryID(tx, newHistoryID, time.Now().UTC().Format(eventTimeFormat)); err != nil {
		return err
	}
	return tx.Commit()
}

// ── small tx-scoped cursor helpers ───────────────────────────────────────────

func (e *Engine) readCursor(ctx context.Context) (string, bool, error) {
	tx, err := e.db.BeginTx(ctx, &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return "", false, err
	}
	defer tx.Rollback()
	return e.store.GetHistoryID(tx)
}

func (e *Engine) setCursor(ctx context.Context, historyID string) error {
	tx, err := e.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()
	if err := e.store.SetHistoryID(tx, historyID, time.Now().UTC().Format(eventTimeFormat)); err != nil {
		return err
	}
	return tx.Commit()
}

// ── header / label / time helpers ────────────────────────────────────────────

// header returns the value of the named RFC-2822 header from a message payload
// (case-insensitive match), or "" if absent.
func header(m *Message, name string) string {
	for _, h := range m.Payload.Headers {
		if equalFold(h.Name, name) {
			return h.Value
		}
	}
	return ""
}

func equalFold(a, b string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := 0; i < len(a); i++ {
		ca, cb := a[i], b[i]
		if 'A' <= ca && ca <= 'Z' {
			ca += 'a' - 'A'
		}
		if 'A' <= cb && cb <= 'Z' {
			cb += 'a' - 'A'
		}
		if ca != cb {
			return false
		}
	}
	return true
}

func containsLabel(labels []string, want string) bool {
	for _, l := range labels {
		if l == want {
			return true
		}
	}
	return false
}

// internalDateToRFC3339Nano converts Gmail's internalDate (epoch milliseconds,
// as a string) to the suite's RFC3339Nano UTC rendering. Falls back to now() if
// the value is missing or unparseable, so a payload always carries a timestamp.
func internalDateToRFC3339Nano(internalDate string) string {
	if internalDate != "" {
		if ms, err := strconv.ParseInt(internalDate, 10, 64); err == nil {
			return time.UnixMilli(ms).UTC().Format(eventTimeFormat)
		}
	}
	return time.Now().UTC().Format(eventTimeFormat)
}
