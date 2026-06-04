package ledger

import (
	"context"
	"fmt"
)

// RegisterLine is one posting in the register, carrying its transaction's date
// and description and the running total of all matched postings up to and
// including this one.
type RegisterLine struct {
	TxnID        string
	Date         string
	Description  string
	PostingID    string
	Account      string
	AmountCents  int64
	Status       string
	RunningTotal int64
}

// RegisterReport is the `reg` report: matched postings in chronological order
// with a running total. It serves customer statements, account history, and
// search, and is the list/paginate verb.
type RegisterReport struct {
	Lines []RegisterLine
}

// Register is the `reg` report — the postings matching f in chronological order
// (date ASC, txn id ASC, posting ord ASC) with a running total. Sums are raw and
// signed, like Balance. It is a read-only transaction.
func (s *Service) Register(ctx context.Context, f Filter) (RegisterReport, error) {
	tx, err := s.DB.BeginTx(ctx, &sqlReadOnly)
	if err != nil {
		return RegisterReport{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	rows, err := s.Store.RegisterPostings(tx, f)
	if err != nil {
		return RegisterReport{}, err
	}
	rep := RegisterReport{Lines: make([]RegisterLine, 0, len(rows))}
	var running int64
	for _, r := range rows {
		running += r.AmountCents
		rep.Lines = append(rep.Lines, RegisterLine{
			TxnID:        r.TxnID,
			Date:         r.Date,
			Description:  r.Description,
			PostingID:    r.PostingID,
			Account:      r.Account,
			AmountCents:  r.AmountCents,
			Status:       r.Status,
			RunningTotal: running,
		})
	}
	return rep, nil
}
