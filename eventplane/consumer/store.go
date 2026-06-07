package consumer

import (
	"context"
	"database/sql"
	"fmt"
	"time"
)

// store is the engine's read/write access to the feed_offset table (§10.3). It
// owns the per-upstream cursor row and is the only writer of it (the consumer is
// the sole writer of feed_offset — event-protocol.md). Every error it returns is
// a STRUCTURAL fault (a missing table, a closed DB) — a deploy/programming bug
// the engine surfaces by crashing, never a transport error to retry (decision
// 11).
type store struct {
	db     *sql.DB
	source string
	now    func() time.Time
}

// offsetState is the durable bootstrap/resume state for one upstream.
type offsetState struct {
	exists     bool
	cursor     sql.NullString // opaque committed cursor; invalid (NULL) before the first commit
	subscribed bool           // the first-subscription marker (§7.1, §10)
}

// load reads the feed_offset row for this upstream. A missing row (exists=false)
// means a fresh subscription that has not bootstrapped yet.
func (s *store) load(ctx context.Context) (offsetState, error) {
	var st offsetState
	var subscribed int
	err := s.db.QueryRowContext(ctx,
		`SELECT cursor, subscribed FROM feed_offset WHERE source = ?`, s.source,
	).Scan(&st.cursor, &subscribed)
	switch err {
	case nil:
		st.exists = true
		st.subscribed = subscribed != 0
		return st, nil
	case sql.ErrNoRows:
		return st, nil
	default:
		return st, fmt.Errorf("consumer: load feed_offset(%s): %w", s.source, err)
	}
}

// markSubscribed records the first-subscription choice durably BEFORE the first
// connect (§7.1, §10): it inserts (or leaves) the row with subscribed=1 and a
// NULL cursor. This is the bootstrap marker that prevents a re-bootstrap from
// silently re-dropping events: once it is set, a cursor-less reconnect resolves
// to "from the beginning" (over-delivery the best-effort hop tolerates), never a
// fresh `tail` (which would drop the gap).
func (s *store) markSubscribed(ctx context.Context) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO feed_offset (source, cursor, subscribed, updated_at)
		      VALUES (?, NULL, 1, ?)
		 ON CONFLICT(source) DO UPDATE SET subscribed = 1, updated_at = excluded.updated_at`,
		s.source, s.nowStr(),
	)
	if err != nil {
		return fmt.Errorf("consumer: mark subscribed(%s): %w", s.source, err)
	}
	return nil
}

// commit advances the durable committed cursor for this upstream (§10). The
// engine calls it only when the handler's return value permits an advance — nil
// or ErrSkip (event-triggering decisions §1); a stalling error skips this call
// so the event re-delivers. There is no effect/dedup row to coordinate with, so
// this is a single bare UPDATE. subscribed is set to 1 here too, so the first
// real event also satisfies the bootstrap marker.
func (s *store) commit(ctx context.Context, cursor string) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO feed_offset (source, cursor, subscribed, updated_at)
		      VALUES (?, ?, 1, ?)
		 ON CONFLICT(source) DO UPDATE SET cursor = excluded.cursor, subscribed = 1, updated_at = excluded.updated_at`,
		s.source, cursor, s.nowStr(),
	)
	if err != nil {
		return fmt.Errorf("consumer: commit cursor(%s): %w", s.source, err)
	}
	return nil
}

// clearForResync discards the stored cursor and clears the subscribed marker so
// the consumer re-bootstraps fresh, honoring its configured first-subscription
// choice (decision 9). The producer told us our position is void (§10.1); we
// reconnect from scratch, not from the dead cursor.
func (s *store) clearForResync(ctx context.Context) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO feed_offset (source, cursor, subscribed, updated_at)
		      VALUES (?, NULL, 0, ?)
		 ON CONFLICT(source) DO UPDATE SET cursor = NULL, subscribed = 0, updated_at = excluded.updated_at`,
		s.source, s.nowStr(),
	)
	if err != nil {
		return fmt.Errorf("consumer: clear feed_offset(%s): %w", s.source, err)
	}
	return nil
}

func (s *store) nowStr() string {
	return s.now().UTC().Format(time.RFC3339Nano)
}
