package ledger

import (
	"context"
	"fmt"
	"sort"
	"strings"
)

// BalanceLine is one account (possibly depth-rolled-up) and its raw signed sum.
type BalanceLine struct {
	Account string
	Sum     int64
}

// BalanceReport is the `bal` report: per-account signed sums plus the grand
// total. Sums are raw and signed (ledger-cli convention): credit-normal roots
// (Liabilities/Equity/Income) report negative, debit-normal (Assets/Expenses)
// positive, and a balance over every account sums to zero (Total == 0) — a free
// correctness check. ledger_describe publishes each root's normal balance so the
// agent can present the numbers.
type BalanceReport struct {
	Lines []BalanceLine
	Total int64
}

// Balance is the `bal` report and the live chart of accounts. With an empty
// Filter it returns the whole emergent account tree with balances. depth, when
// > 0, rolls each account up to its first depth colon-segments and aggregates
// (deeper accounts fold into their depth-N ancestor); depth <= 0 reports leaf
// accounts as posted. It is a read-only transaction.
func (s *Service) Balance(ctx context.Context, f Filter, depth int) (BalanceReport, error) {
	tx, err := s.DB.BeginTx(ctx, &sqlReadOnly)
	if err != nil {
		return BalanceReport{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	sums, err := s.Store.SumByAccount(tx, f)
	if err != nil {
		return BalanceReport{}, err
	}
	return rollUp(sums, depth), nil
}

// rollUp aggregates leaf account sums to the requested depth and computes the
// grand total. Leaf sums arrive account-sorted from the store; the rolled-up
// keys are re-sorted to keep output deterministic.
func rollUp(sums []AccountSum, depth int) BalanceReport {
	var rep BalanceReport
	if depth <= 0 {
		rep.Lines = make([]BalanceLine, 0, len(sums))
		for _, a := range sums {
			rep.Lines = append(rep.Lines, BalanceLine{Account: a.Account, Sum: a.Sum})
			rep.Total += a.Sum
		}
		return rep
	}
	agg := map[string]int64{}
	var order []string
	for _, a := range sums {
		key := truncateAccount(a.Account, depth)
		if _, seen := agg[key]; !seen {
			order = append(order, key)
		}
		agg[key] += a.Sum
		rep.Total += a.Sum
	}
	sort.Strings(order)
	rep.Lines = make([]BalanceLine, 0, len(order))
	for _, k := range order {
		rep.Lines = append(rep.Lines, BalanceLine{Account: k, Sum: agg[k]})
	}
	return rep
}

// truncateAccount keeps the first depth colon-segments of an account path.
func truncateAccount(account string, depth int) string {
	segs := strings.Split(account, ":")
	if depth >= len(segs) {
		return account
	}
	return strings.Join(segs[:depth], ":")
}
