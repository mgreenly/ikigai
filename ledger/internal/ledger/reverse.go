package ledger

import (
	"context"
	"fmt"

	"ledger/internal/ids"
)

// Reverse posts the compensating mirror of an existing transaction — the
// correction primitive. It inserts a new transaction whose postings are the
// sign-flipped mirror of the original (whole-transaction only, no partial legs),
// links the two both ways (reverses_id on the mirror, reversed_by_id on the
// original), and resets the mirror's legs to pending (a reversal has not cleared
// anything). The original stays in the journal untouched; the books show the
// mistake and its compensation.
//
// Double-reversal is blocked: if the original already carries a mirror,
// ErrAlreadyReversed. Reversing a reversal is allowed — it re-creates the
// original effect. date overrides the mirror's date (defaults to the original's);
// memo overrides its description (defaults to "Reversal of: <original>").
func (s *Service) Reverse(ctx context.Context, id string, date, memo *string, externalRefs ...*string) (Transaction, error) {
	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return Transaction{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()
	var externalRef *string
	if len(externalRefs) > 0 {
		externalRef = externalRefs[0]
	}

	orig, err := s.loadFull(tx, id)
	if err != nil {
		return Transaction{}, err
	}

	mirror := Transaction{
		ID:          ids.NewULID(),
		Date:        orig.Date,
		Description: "Reversal of: " + orig.Description,
		CreatedAt:   s.Now().UTC(),
		ReversesID:  &orig.ID,
		ExternalRef: externalRef,
	}
	if date != nil && *date != "" {
		mirror.Date = *date
	}
	if memo != nil && *memo != "" {
		mirror.Description = *memo
	}
	mirror.Postings = make([]Posting, len(orig.Postings))
	for i, p := range orig.Postings {
		mirror.Postings[i] = Posting{
			ID:          ids.NewULID(),
			TxnID:       mirror.ID,
			Account:     p.Account,
			AmountCents: -p.AmountCents,
			Status:      StatusPending,
			Ord:         p.Ord,
		}
	}

	if err := s.assertRefAvailable(tx, mirror.ExternalRef); err != nil {
		return Transaction{}, err
	}
	if err := s.persist(tx, mirror); err != nil {
		return Transaction{}, err
	}
	// Stamp the original as reversed. This is the atomic double-reversal guard:
	// it fails with ErrAlreadyReversed if the original already has a mirror, and
	// the rollback discards the mirror we just inserted.
	if err := s.Store.SetReversedBy(tx, orig.ID, mirror.ID); err != nil {
		return Transaction{}, err
	}
	if err := tx.Commit(); err != nil {
		return Transaction{}, fmt.Errorf("commit: %w", err)
	}
	s.ring()
	return mirror, nil
}
