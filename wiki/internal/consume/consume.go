// Package consume is wiki's event-plane consumer domain: it turns dropbox
// file-lifecycle events into autonomous ingests. It is the consumer-side mirror
// of notify's internal/push — where notify maps crm's contact.created to an ntfy
// POST, wiki maps dropbox's file.created/file.modified (for a hardcoded
// wiki/ingest folder) into the SAME async ingest core the wiki_ingest_text verb
// uses (wiki/internal/ingest). The consumer is just another trigger producing
// bytes; it does NOT fork the pipeline (PLAN Task 6.1, GOALS Triggers).
//
// Autonomous is the floor (GOALS): a file landing in wiki/ingest has no human in
// the loop, so ingest runs unattended. The immutable raw/ store is what makes
// that safe — re-running the integration over identical bytes is a no-op, so the
// event plane's at-least-once re-delivery never duplicates or corrupts state.
//
// Dropbox is a single-box, single-owner producer (dropbox/CLAUDE.md "one box is
// one owner"): its events carry NO owner, only a literal path + a loopback
// content_url. So this consumer maps every wiki/ingest event to ONE configured
// box owner (Config.Owner, resolved at the composition root from WIKI_OWNER) and
// fetches the bytes by reference over content_url.
package consume

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"path"
	"strings"
	"time"

	"eventplane/consumer"

	"wiki/internal/ingest"
	"wiki/internal/store"
)

// dropbox event type names (dropbox/internal/dropbox/events.go). The consumer
// filters on these; only create/modify trigger an ingest.
const (
	eventFileCreated  = "file.created"
	eventFileModified = "file.modified"
	eventFileDeleted  = "file.deleted"
)

// IngestFolder is the hardcoded Dropbox folder wiki watches (PLAN Task 6.1,
// GOALS Triggers). Only file events at or beneath this prefix are ingested;
// everything else in the app folder is ignored (the cursor still advances, §7.3).
// Dropbox display paths are absolute within the app folder and begin with "/",
// so the watched prefix is "/wiki/ingest".
const IngestFolder = "/wiki/ingest"

// fetchTimeout bounds a single content fetch over the loopback /content endpoint.
// The fetch is on the consumer's hot path (it blocks the cursor advance for this
// one event), but the body is a local-mirror file served off loopback, so this is
// generous-but-bounded — a black-holed dropbox /content never wedges the feed
// forever.
const fetchTimeout = 30 * time.Second

// Ingester is the slice of the ingest core this consumer needs: the single
// Ingest entrypoint shared with wiki_ingest_text (PLAN Task 6.1 — REUSE the
// core, do not fork it). The real *ingest.Core satisfies it directly (no
// adapter); the handler holds the interface so the test drives a stub without the
// agent/job machinery.
type Ingester interface {
	Ingest(ctx context.Context, owner, collection string, content []byte, meta store.RawMeta) (ingest.Result, error)
}

// fetchFunc fetches the bytes a dropbox event references. It defaults to an HTTP
// GET of the event's content_url (the dropbox /content loopback endpoint); tests
// inject a stub so the handler never touches the network.
type fetchFunc func(ctx context.Context, contentURL string) ([]byte, error)

// Config is the consumer-domain configuration, resolved once at the composition
// root (cmd/wiki/main.go) and handed to Handler.
type Config struct {
	// Owner is the box owner every wiki/ingest event is filed under. Dropbox is
	// single-owner and its events carry no owner, so the owner is service config
	// (WIKI_OWNER), not derived from the event. Required.
	Owner string
	// Ingester is the shared ingest core. Required.
	Ingester Ingester
	// HTTPClient fetches content_url. Defaults to a fetchTimeout-bounded client.
	HTTPClient *http.Client
	// Logger defaults to slog.Default().
	Logger *slog.Logger
}

// dropboxFilePayload is the slice of dropbox's file-lifecycle payload the
// consumer reads (dropbox/internal/dropbox/events.go filePayload). The bare
// `path` is the literal Dropbox path within the app folder; content_url is the
// loopback /content fetch-by-reference URL; content_hash is carried for logging /
// future race-detection (dropbox/PLAN §5 "race honesty").
type dropboxFilePayload struct {
	Event       string `json:"event"`
	Path        string `json:"path"`
	Rev         string `json:"rev"`
	ContentHash string `json:"content_hash"`
	Size        int64  `json:"size"`
	ContentURL  string `json:"content_url"`
	OccurredAt  string `json:"occurred_at"`
}

// Handler returns the consumer.Handler wiki hands to the engine (PLAN Task 6.1).
// It runs the ingest effect ONLY for file.created / file.modified events whose
// path is at or beneath the hardcoded wiki/ingest folder; every other event
// (file.deleted, files outside wiki/ingest, non-file types) is ignored — the
// engine still commits the cursor for those, so they do not re-arrive (§7.3).
//
// For a matched event it: (1) fetches the bytes by reference over content_url,
// (2) calls Core.Ingest for the configured box owner with source provenance
// "dropbox:<path>". Ingest is SYNCHRONOUS in the handler (unlike notify's
// fire-and-forget push) but cheap: it only persists the immutable raw doc and
// SPAWNS the async integration job, returning a job id immediately — the agentic
// pass runs in the background, so the cursor advance is not blocked on the LLM.
//
// Delete policy: file.deleted is intentionally a NO-OP. The wiki's raw/ store is
// immutable and append-don't-destroy (GOALS invariants) — a file removed from
// Dropbox does not un-file what was already integrated. We log it at Info so the
// decision is observable, then advance the cursor.
//
// Failure classification (event-triggering decisions §1): the handler stays dumb
// and just returns what each step yields. Malformed payload → ErrSkip (poison →
// log loud + advance). Fetch → propagated: httpFetch wraps ErrSkip for a 404/410/
// 409 ("gone") and returns a plain error for 5xx/transport (→ stall + retry).
// Empty content → nil (a valid empty file is nothing to do, not poison). Ingest
// failure → plain error → STALL so the work is retried (the latent-bug fix —
// under the old commit-regardless engine it silently dropped the file).
//
// Idempotency / at-least-once: a re-delivered identical event is a safe no-op —
// WriteRaw is idempotent on the content sha256 (AlreadyHad=true), and re-running
// the integration over the same raw doc is safe by design (the agent reads
// index-first and updates rather than duplicates). The consumer therefore needs
// no dedup table (003_feed_offset.sql ships none). The stall+retry above relies on
// this: replaying a stalled event after a transient failure cannot duplicate state.
func Handler(cfg Config) consumer.Handler {
	logger := cfg.Logger
	if logger == nil {
		logger = slog.Default()
	}
	client := cfg.HTTPClient
	if client == nil {
		client = &http.Client{Timeout: fetchTimeout}
	}
	fetch := httpFetch(client)
	return handlerWith(cfg.Owner, cfg.Ingester, fetch, logger)
}

// handlerWith is the testable core of Handler: it takes the resolved owner,
// ingester, fetch func, and logger directly so a test can inject a stub fetch and
// a stub ingester without an HTTP server. Handler is the production wiring over
// it. A nil logger defaults to slog.Default() so callers (and tests) may omit it.
func handlerWith(owner string, ing Ingester, fetch fetchFunc, logger *slog.Logger) consumer.Handler {
	if logger == nil {
		logger = slog.Default()
	}
	return func(ctx context.Context, ev consumer.Event) error {
		// Filter 1: only file create/modify trigger an ingest. file.deleted is a
		// deliberate no-op (immutable raw); any other type is not ours.
		switch ev.Type {
		case eventFileCreated, eventFileModified:
			// fall through to handle
		case eventFileDeleted:
			logger.Info("consume: ignoring file.deleted (immutable raw — no un-filing)",
				"event_id", ev.ID, "source", ev.Source)
			return nil
		default:
			return nil // not ours — the engine advances the cursor anyway (§7.3)
		}

		var p dropboxFilePayload
		if err := json.Unmarshal(ev.Payload, &p); err != nil {
			// A malformed payload is semantic poison — it can never decode, so wrap
			// ErrSkip: the engine logs it loud and advances past it rather than
			// stalling the feed forever (event-triggering decisions §1).
			return fmt.Errorf("consume: decode %s %s: %w: %w", ev.Type, ev.ID, err, consumer.ErrSkip)
		}

		// Filter 2: only the hardcoded wiki/ingest folder. A path outside it is
		// ignored (the wiki/ingest folder is the autonomous front door, GOALS).
		if !underIngestFolder(p.Path) {
			logger.Debug("consume: ignoring event outside wiki/ingest",
				"path", p.Path, "event", ev.Type, "event_id", ev.ID)
			return nil
		}

		// Fetch the bytes by reference over the loopback /content endpoint
		// (dropbox is fetch-by-reference, never bytes-inline). The handler stays dumb
		// and just PROPAGATES whatever fetch returns (event-triggering decisions §1):
		// httpFetch wraps ErrSkip for a 404/410/409 ("gone" → log loud + advance) and
		// returns a plain error for 5xx/transport faults (→ stall + retry from the
		// committed cursor). The %w preserves the ErrSkip wrap for errors.Is.
		data, err := fetch(ctx, p.ContentURL)
		if err != nil {
			return fmt.Errorf("consume: fetch %s for %s: %w", p.ContentURL, ev.ID, err)
		}
		if len(data) == 0 {
			// A valid empty file is not poison and not a failure: there is simply
			// nothing to ingest. Advance the cursor (silent nil), do not stall.
			logger.Warn("consume: fetched empty content, skipping ingest",
				"path", p.Path, "event_id", ev.ID)
			return nil
		}

		// Feed the SAME async ingest core wiki_ingest_text uses. Owner is the
		// configured box owner (dropbox is single-owner); source provenance names
		// dropbox + the literal path; collection is the default ("").
		res, err := ing.Ingest(ctx, owner, "", data, store.RawMeta{
			Title:  path.Base(p.Path),
			Source: "dropbox:" + p.Path,
		})
		if err != nil {
			// An Ingest failure is transient (it persists the immutable raw doc and
			// spawns the async job; a failure here means that write did not land).
			// Return a PLAIN error so the engine STALLS and replays the event from the
			// committed cursor — the work is retried, not lost. This is the latent-bug
			// fix: under the old commit-regardless engine this same error silently
			// dropped the file from the index (event-triggering decisions §1).
			return fmt.Errorf("consume: ingest %s (event %s): %w", p.Path, ev.ID, err)
		}
		logger.Info("consume: filed dropbox file",
			"path", p.Path, "owner", owner, "job_id", res.JobID,
			"sha256", res.Sha256, "already_had", res.AlreadyHad, "event", ev.Type)
		return nil
	}
}

// underIngestFolder reports whether a literal Dropbox path is at or beneath the
// hardcoded wiki/ingest folder. Dropbox display paths are absolute within the app
// folder ("/wiki/ingest/notes.md"); the match is on cleaned path segments so a
// sibling like "/wiki/ingest-archive/x.md" does NOT match the "/wiki/ingest"
// prefix (segment-boundary check, not a raw HasPrefix). Dropbox is
// case-insensitive + case-preserving, so the comparison folds case.
func underIngestFolder(p string) bool {
	if p == "" {
		return false
	}
	clean := path.Clean(p)
	folder := IngestFolder
	if strings.EqualFold(clean, folder) {
		// The folder itself is structural, not a file — no ingest. (Folders emit no
		// events in dropbox v1, but guard anyway.)
		return false
	}
	prefix := folder + "/"
	return len(clean) > len(prefix) && strings.EqualFold(clean[:len(prefix)], prefix)
}

// httpFetch builds the production fetchFunc: an HTTP GET of the content_url. The
// dropbox /content endpoint is loopback-only and unauthenticated (the perimeter
// is "it's on 127.0.0.1"), so no auth headers are sent.
//
// The fetch layer owns the transient-vs-terminal split, because that is where the
// HTTP status is in scope (event-triggering decisions §1):
//   - 404 / 410 / 409 ("gone": the file moved/was deleted before we fetched) is a
//     permanently-unprocessable fetch → wrap consumer.ErrSkip so the handler
//     propagates a skip (log loud + advance, never retry a 404 forever).
//   - 5xx, transport errors, and everything else stay a PLAIN error → the handler
//     propagates a stall and the engine retries from the committed cursor.
func httpFetch(client *http.Client) fetchFunc {
	return func(ctx context.Context, contentURL string) ([]byte, error) {
		req, err := http.NewRequestWithContext(ctx, http.MethodGet, contentURL, nil)
		if err != nil {
			return nil, fmt.Errorf("build request: %w", err)
		}
		resp, err := client.Do(req)
		if err != nil {
			// Transport error (dial/reset/timeout): transient → plain error → stall.
			return nil, err
		}
		defer resp.Body.Close()
		if resp.StatusCode/100 != 2 {
			io.Copy(io.Discard, resp.Body) //nolint:errcheck // drain for connection reuse
			switch resp.StatusCode {
			case http.StatusNotFound, http.StatusGone, http.StatusConflict:
				// Gone: permanently unprocessable → skip past it.
				return nil, fmt.Errorf("content fetch returned %d (gone): %w", resp.StatusCode, consumer.ErrSkip)
			default:
				// 5xx and any other non-2xx: transient → plain error → stall+retry.
				return nil, fmt.Errorf("content fetch returned %d", resp.StatusCode)
			}
		}
		return io.ReadAll(resp.Body)
	}
}
