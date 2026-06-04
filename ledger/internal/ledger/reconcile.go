package ledger

import (
	"context"
	"fmt"
	"sort"
)

// Reconcile is the lone permitted mutation of existing journal rows: it
// transitions each named posting's reconciliation status. Transitions are free
// among the three states (including backward un-reconcile); setting a posting to
// its current status is an idempotent no-op. The batch is all-or-nothing — an
// unknown posting_id fails the whole call loudly (ErrNotFound) and nothing is
// written. It can never touch an amount, account, or date. Status is orthogonal
// to reversal (status lives on the posting, links on the transaction).
//
// It returns the affected transactions in full (distinct, id-ordered) so the
// caller sees the resulting per-posting statuses without a follow-up Get.
func (s *Service) Reconcile(ctx context.Context, postingIDs []string, status string) ([]Transaction, error) {
	if len(postingIDs) == 0 {
		return nil, fmt.Errorf("%w: at least one posting_id is required", ErrValidation)
	}
	if !ValidStatus(status) {
		return nil, fmt.Errorf("%w: unknown status %q", ErrValidation, status)
	}

	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	affected := map[string]struct{}{}
	for _, pid := range postingIDs {
		p, err := s.Store.GetPosting(tx, pid) // ErrNotFound fails the whole batch
		if err != nil {
			return nil, err
		}
		affected[p.TxnID] = struct{}{}
		if p.Status == status {
			continue // idempotent no-op
		}
		if err := s.Store.UpdatePostingStatus(tx, pid, status); err != nil {
			return nil, err
		}
	}
	if err := tx.Commit(); err != nil {
		return nil, fmt.Errorf("commit: %w", err)
	}

	ids := make([]string, 0, len(affected))
	for id := range affected {
		ids = append(ids, id)
	}
	sort.Strings(ids)

	rtx, err := s.DB.BeginTx(ctx, &sqlReadOnly)
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer rtx.Rollback()
	out := make([]Transaction, 0, len(ids))
	for _, id := range ids {
		t, err := s.loadFull(rtx, id)
		if err != nil {
			return nil, err
		}
		out = append(out, t)
	}
	return out, nil
}
