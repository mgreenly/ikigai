package outbox

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"time"

	"eventplane/routing"
)

// keepaliveInterval is the idle-connection keepalive cadence (§10.2 SHOULD ~15s).
// While actively streaming events the real frames prove liveness, so keepalives
// are emitted only while parked/caught-up.
const keepaliveInterval = 15 * time.Second

// resync reason codes (§10.1).
const (
	reasonStaleEpoch     = "stale-epoch"           // cursor's generation != live generation (§9.3)
	reasonPastHorizon    = "past-horizon"          // cursor below retention horizon — real loss (§11.1)
	reasonDiverged       = "diverged"              // cursor ahead of head (e.g. restored from older backup)
	reasonUnintelligible = "unintelligible-cursor" // cursor unparseable / from a different feed
)

// envelope is the uniform event envelope serialized into each event frame's
// `data:` line (§8.3). Payload is opaque to the library.
type envelope struct {
	ID      string          `json:"id"`
	Source  string          `json:"source"`
	Time    string          `json:"time"`
	Kind    string          `json:"kind"`
	Subject string          `json:"subject"`
	Payload json.RawMessage `json:"payload"`
}

// FeedHandler returns the GET /feed SSE handler (§7, §8, §10). It is mounted
// WITHOUT any identity middleware: the event plane is unauthenticated and
// loopback-only (§2). As defence in depth it rejects any request that arrives
// carrying nginx-injected identity headers — such a request was proxied through
// the public front door, which must never happen for the feed.
//
// First-subscription position (§7.1):
//   - a Last-Event-ID header   -> resume strictly after that opaque cursor
//   - ?from=tail (no header)    -> only new events, starting at the current head
//   - neither                   -> from the beginning of the retained outbox
func (o *Outbox) FeedHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Defence in depth: the feed must be reached only by direct loopback
		// callers. Any nginx-injected identity header means it was proxied
		// through the authenticated front door — refuse, mirroring the nginx
		// exact-match 404 on the public mount.
		if r.Header.Get("X-Owner-Email") != "" || r.Header.Get("X-Forwarded-Proto") != "" {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}

		ctx := r.Context()
		consumerID := r.Header.Get("X-Consumer-Id") // §7.1: logged for observability
		rc := http.NewResponseController(w)
		// SSE is a long-lived stream; clear the server's WriteTimeout for this
		// connection so a parked feed is not torn down mid-stream.
		_ = rc.SetWriteDeadline(time.Time{})

		h := w.Header()
		h.Set("Content-Type", "text/event-stream")
		h.Set("Cache-Control", "no-cache")
		h.Set("Connection", "keep-alive")
		w.WriteHeader(http.StatusOK)
		if err := rc.Flush(); err != nil {
			return
		}

		// Resolve the start position. The connect-time check can fail after the
		// 200 + text/event-stream are already on the wire (§7.2), so any resync
		// is itself an SSE frame.
		var startSeq int64
		if lid := r.Header.Get("Last-Event-ID"); lid != "" {
			reason, seq, err := o.checkCursor(ctx, lid)
			if err != nil {
				o.log.Error("feed: connect-time check failed", "err", err, "consumer", consumerID)
				return
			}
			if reason != "" {
				// Flush the resync frame before closing so it reaches the socket
				// (§10.1) — otherwise a permanent reason loops forever.
				_ = o.writeFrame(w, rc, "event: resync\ndata: "+resyncData(reason)+"\n\n")
				if reason == reasonPastHorizon {
					// The only reason denoting real, unrecovered loss (§11.1) —
					// SHOULD be surfaced beyond a single log line.
					o.log.Error("feed: resync — DATA LOSS", "reason", reason, "consumer", consumerID)
				} else {
					o.log.Warn("feed: resync", "reason", reason, "consumer", consumerID)
				}
				return
			}
			startSeq = seq
		} else if r.URL.Query().Get("from") == "tail" {
			seq, err := o.headSeq(ctx)
			if err != nil {
				o.log.Error("feed: head seq failed", "err", err, "consumer", consumerID)
				return
			}
			startSeq = seq
		}

		o.log.Info("feed: connected", "consumer", consumerID, "start_seq", startSeq)

		// Optional one-shot lag telemetry (§10.2 status is a MAY): tell the
		// consumer how far behind it starts. It MUST NOT act on this.
		if head, err := o.headSeq(ctx); err == nil && head > startSeq {
			_ = o.writeFrame(w, rc, "event: status\ndata: "+statusData(head-startSeq)+"\n\n")
		}

		o.stream(ctx, w, rc, startSeq)
	})
}

// stream is the drain-then-park loop: it streams every event after startSeq,
// emits caught-up on the edge to the head, then parks on the doorbell (or the
// keepalive timer) until a new event lands. It returns when the client
// disconnects or a write fails.
func (o *Outbox) stream(ctx context.Context, w io.Writer, rc *http.ResponseController, startSeq int64) {
	caughtUp := false
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		rows, last, err := o.fetch(ctx, startSeq, o.batchLimit)
		if err != nil {
			o.log.Error("feed: fetch failed", "err", err)
			return
		}
		if len(rows) > 0 {
			caughtUp = false
			for _, row := range rows {
				if err := o.writeFrame(w, rc, o.eventFrame(row)); err != nil {
					return // client gone or slow-reader write failure
				}
			}
			startSeq = last
			continue // keep draining until a fetch comes back empty
		}

		// Caught up: state a fact about our own outbox (§10.1), once per edge.
		if !caughtUp {
			if err := o.writeFrame(w, rc, "event: caught-up\ndata: {}\n\n"); err != nil {
				return
			}
			caughtUp = true
		}

		// Park until a new event rings the doorbell, the keepalive fires, or the
		// client disconnects. Subscribe BEFORE the next loop's fetch races a Ring.
		bell := o.subscribe()
		select {
		case <-ctx.Done():
			return
		case <-bell:
			// New event(s) — loop and drain.
		case <-time.After(keepaliveInterval):
			if err := o.writeFrame(w, rc, ": keepalive\n\n"); err != nil {
				return
			}
		}
	}
}

// eventFrame renders one event row as a complete SSE event frame (§8.1): the
// opaque cursor on the id: line, the type on the event: line, and the uniform
// envelope as a single compact JSON data: line.
func (o *Outbox) eventFrame(row eventRow) string {
	env := envelope{
		ID:      row.eventID,
		Source:  o.source,
		Time:    row.createdAt,
		Kind:    row.kind,
		Subject: row.subject,
		Payload: json.RawMessage(row.payload),
	}
	// json.Marshal emits compact, single-line JSON with no embedded newlines —
	// exactly one data: line per event (§8.1).
	data, err := json.Marshal(env)
	if err != nil {
		// payload is producer-controlled JSON; a marshal failure is a bug, but
		// never emit a malformed frame.
		o.log.Error("feed: marshal envelope failed", "err", err, "event_id", row.eventID)
		return ": skip\n\n"
	}
	return "id: " + makeCursor(o.generation, row.seq) + "\n" +
		"event: " + routing.Key(o.source, row.kind, row.subject) + "\n" +
		"data: " + string(data) + "\n\n"
}

// writeFrame writes a complete frame and flushes it promptly (§6.1): a caught-up
// consumer is woken per committed event, and write() blocks on a slow reader
// rather than buffering the backlog in memory.
func (o *Outbox) writeFrame(w io.Writer, rc *http.ResponseController, frame string) error {
	if _, err := io.WriteString(w, frame); err != nil {
		return err
	}
	return rc.Flush()
}

func resyncData(reason string) string {
	b, _ := json.Marshal(map[string]string{"reason": reason})
	return string(b)
}

func statusData(behind int64) string {
	b, _ := json.Marshal(map[string]int64{"behind": behind})
	return string(b)
}
