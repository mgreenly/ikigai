package ledger

import (
	"context"
	"fmt"
)

// Unit is the money unit every amount is denominated in: integer minor units
// (cents) of a single currency, USD.
const Unit = "USD cents"

// StatusInfo documents one reconciliation state for ledger_describe.
type StatusInfo struct {
	Status  string
	Meaning string
}

// Recipe tells the agent how to produce a named report from the balance/register
// verbs, so statements live in describe rather than as extra tools.
type Recipe struct {
	Name string
	How  string
}

// DescribeReport is the ledger_describe payload: the typed roots (+ normal
// balance and statement membership), the money unit, the reconciliation states
// and their meaning, the live emergent account tree, and recipes for the common
// statements. It is the first call any agent should make.
type DescribeReport struct {
	Unit         string
	Roots        []RootInfo
	Statuses     []StatusInfo
	Accounts     []string
	Recipes      []Recipe
	ExternalRefs string
}

// reconciliationStates and reportRecipes are hand-authored static content (§12);
// Describe merges them with a live SELECT DISTINCT account tree.
var reconciliationStates = []StatusInfo{
	{StatusPending, "recorded but not yet confirmed against an external source (the default)"},
	{StatusCleared, "confirmed to have cleared the account — e.g. seen on the bank/card statement"},
	{StatusReconciled, "matched against an official statement balance and locked in"},
}

var reportRecipes = []Recipe{
	{"balance_sheet", "assets, liabilities & equity at an instant: balance(query:\"Assets\"), balance(query:\"Liabilities\"), balance(query:\"Equity\") — omit period for 'now' or pass {to:DATE} for a point in time. Sums are raw signed: Assets positive, Liabilities/Equity negative."},
	{"income_statement", "profit & loss over a period: balance(query:\"Income\", period:P) and balance(query:\"Expenses\", period:P). Income is credit-normal (negative), Expenses debit-normal (positive); net income = -(Income+Expenses)."},
	{"net_worth", "balance(depth:1) for the roots, then Assets + Liabilities + Equity (raw signed, so simply their sum)."},
	{"customer_statement", "register(query:\"Assets:Receivable:<customer>\") — chronological charges, payments, and running A/R balance for one customer."},
	{"open_receivables", "balance(query:\"Assets:Receivable\") — outstanding balance per customer sub-account."},
	{"bank_reconciliation", "balance(query:\"Assets:Bank\", status:\"cleared\") vs. the same without a status filter; the difference is the uncleared items."},
}

// Describe returns the static conventions merged with the live account tree. It
// is a read-only transaction.
func (s *Service) Describe(ctx context.Context) (DescribeReport, error) {
	tx, err := s.DB.BeginTx(ctx, &sqlReadOnly)
	if err != nil {
		return DescribeReport{}, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback()

	accounts, err := s.Store.DistinctAccounts(tx)
	if err != nil {
		return DescribeReport{}, err
	}
	if accounts == nil {
		accounts = []string{}
	}
	return DescribeReport{
		Unit:         Unit,
		Roots:        Roots(),
		Statuses:     reconciliationStates,
		Accounts:     accounts,
		Recipes:      reportRecipes,
		ExternalRefs: "Use <source>:<identifier> for external_ref (for example dropbox:/bills/aws/2026-06.pdf@<content_hash> or gmail:<message-id>); this is a naming convention, not a validated format.",
	}, nil
}
