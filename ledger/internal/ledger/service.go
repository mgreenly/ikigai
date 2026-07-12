package ledger

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"
)

// Service owns transactions, enforces the balance invariant, and (when an Outbox
// is wired) emits events. It is the integration seam — NOT a verb dispatcher;
// the MCP layer (internal/mcp/tools.go) is the sole dispatcher and arg-validation
// site. Each verb's logic lives in its own file (transaction.go, reverse.go,
// reconcile.go, balance.go, register.go, describe.go); they share the helpers
// here.
type Service struct {
	DB    *sql.DB
	Store *Store
	Now   func() time.Time
	// Outbox, when set, makes the service an event-plane producer: a
	// recorded event is appended atomically with every committed
	// transaction and the feed is rung after commit. Nil disables emission. It is
	// wired in events.go (Phase 5); kept as an interface so the domain does not
	// hard-depend on the event-plane library when emission is off.
	Outbox EventSink
}

// EventSink is the producer seam the Service appends to. The concrete
// implementation wraps the eventplane outbox (events.go). It is an interface so
// Service can run with emission disabled (Outbox == nil) without importing the
// library.
type EventSink interface {
	// AppendRecorded appends the recorded event for t on tx, atomic
	// with the journal write.
	AppendRecorded(tx *sql.Tx, t Transaction) error
	// Ring wakes parked feed connections; called after a successful commit.
	Ring()
}

// NewService builds a Service over db with the real clock.
func NewService(db *sql.DB) *Service {
	return &Service{DB: db, Store: NewStore(), Now: time.Now}
}

// sqlReadOnly is the shared read-only transaction option for the report reads.
var sqlReadOnly = sql.TxOptions{ReadOnly: true}

// assertBalanced enforces the core invariant: the postings sum to zero. It is
// re-asserted in the service even though the MCP boundary resolves elision and
// checks well-formedness — the service guarantees no unbalanced transaction is
// ever persisted (PLAN.md §5).
func assertBalanced(postings []Posting) error {
	var sum int64
	for _, p := range postings {
		sum += p.AmountCents
	}
	if sum != 0 {
		return fmt.Errorf("%w: postings sum to %d, not 0", ErrUnbalanced, sum)
	}
	return nil
}

// persist inserts a fully-resolved, balanced transaction and its postings on the
// caller's tx, then (when wired) appends the recorded event on the
// SAME tx so it commits atomically. It is the single insert helper shared by
// Record and Reverse, so every committed transaction emits exactly one event,
// reversal mirrors included (PLAN.md §6). Ring happens after Commit, in the
// caller.
func (s *Service) persist(tx *sql.Tx, t Transaction) error {
	if err := assertBalanced(t.Postings); err != nil {
		return err
	}
	if err := s.Store.InsertTransaction(tx, t); err != nil {
		if t.ExternalRef != nil && strings.Contains(err.Error(), "UNIQUE constraint failed") {
			return s.duplicateRefError(tx, *t.ExternalRef)
		}
		return err
	}
	for _, p := range t.Postings {
		if err := s.Store.InsertPosting(tx, p); err != nil {
			return err
		}
	}
	if s.Outbox != nil {
		if err := s.Outbox.AppendRecorded(tx, t); err != nil {
			return err
		}
	}
	return nil
}

// assertRefAvailable performs the error-shaping pre-check in the caller's write
// transaction. The migration's partial unique index remains the authoritative
// race backstop.
func (s *Service) assertRefAvailable(tx *sql.Tx, ref *string) error {
	if ref == nil {
		return nil
	}
	_, err := s.Store.GetTransactionByExternalRef(tx, *ref)
	if errors.Is(err, ErrNotFound) {
		return nil
	}
	if err != nil {
		return err
	}
	return s.duplicateRefError(tx, *ref)
}

func (s *Service) duplicateRefError(tx *sql.Tx, ref string) error {
	existing, err := s.Store.GetTransactionByExternalRef(tx, ref)
	if err != nil {
		return err
	}
	return fmt.Errorf("%w: external_ref %q already recorded by transaction %s", ErrDuplicateRef, ref, existing.ID)
}

// ring wakes parked feed connections after a successful commit (no-op when the
// outbox is not wired).
func (s *Service) ring() {
	if s.Outbox != nil {
		s.Outbox.Ring()
	}
}

// loadFull reads a transaction and its postings on tx. ErrNotFound if absent.
func (s *Service) loadFull(tx *sql.Tx, id string) (Transaction, error) {
	t, err := s.Store.GetTransaction(tx, id)
	if err != nil {
		return Transaction{}, err
	}
	ps, err := s.Store.ListPostings(tx, id)
	if err != nil {
		return Transaction{}, err
	}
	t.Postings = ps
	return t, nil
}

// Get returns one transaction in full (all postings + reversal links). It is a
// read-only transaction.
func (s *Service) Get(ctx context.Context, id string) (Transaction, error) {
	tx, err := s.DB.BeginTx(ctx, &sqlReadOnly)
	if err != nil {
		return Transaction{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()
	return s.loadFull(tx, id)
}
