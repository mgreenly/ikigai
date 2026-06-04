package ledger

import (
	"context"
	"fmt"

	"ledger/internal/ids"
)

// Record posts one balanced transaction. The MCP boundary has already
// canonicalized each account, validated the date, and defaulted each posting's
// status; here the service resolves the (at most one) elided amount to the
// balancing residual, asserts the postings sum to zero, and inserts the
// transaction and its postings atomically. It returns the full transaction
// (resolved residual, assigned ids) so the caller needs no follow-up Get.
func (s *Service) Record(ctx context.Context, in RecordInput) (Transaction, error) {
	t := Transaction{
		ID:          ids.NewULID(),
		Date:        in.Date,
		Description: in.Description,
		CreatedAt:   s.Now().UTC(),
	}
	postings, err := resolvePostings(t.ID, in.Postings)
	if err != nil {
		return Transaction{}, err
	}
	t.Postings = postings

	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return Transaction{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()
	if err := s.persist(tx, t); err != nil {
		return Transaction{}, err
	}
	if err := tx.Commit(); err != nil {
		return Transaction{}, fmt.Errorf("commit: %w", err)
	}
	s.ring()
	return t, nil
}

// resolvePostings turns the resolved inputs into concrete postings: it assigns a
// ULID and ord to each leg, defaults an empty status to pending, and gives the
// single elided leg (if any) the negation of the explicit sum so the
// transaction balances. Two or more elisions leave the residual undetermined and
// are rejected (the MCP boundary already enforces this; resolvePostings re-asserts
// it so the residual is never silently mis-split).
func resolvePostings(txnID string, in []PostingInput) ([]Posting, error) {
	out := make([]Posting, len(in))
	var explicitSum int64
	elidedIdx := -1
	for i, p := range in {
		status := p.Status
		if status == "" {
			status = StatusPending
		}
		out[i] = Posting{
			ID:      ids.NewULID(),
			TxnID:   txnID,
			Account: p.Account,
			Status:  status,
			Ord:     i,
		}
		if p.AmountCents == nil {
			if elidedIdx != -1 {
				return nil, fmt.Errorf("%w: at most one posting may elide its amount", ErrValidation)
			}
			elidedIdx = i
			continue
		}
		out[i].AmountCents = *p.AmountCents
		explicitSum += *p.AmountCents
	}
	if elidedIdx != -1 {
		// The elided leg receives the residual; a zero residual still yields a
		// concrete amount_cents: 0 leg (not special-cased).
		out[elidedIdx].AmountCents = -explicitSum
	}
	return out, nil
}
