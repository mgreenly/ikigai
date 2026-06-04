// Package ledger is the double-entry bookkeeping domain (PLAN.md). The layering
// mirrors the crm chassis: store.go is the SQL-only data layer (every method
// takes *sql.Tx), and Service owns transactions, the balance invariant, and
// event emission. Argument validation/normalization (account canonicalization,
// date parsing, elision well-formedness) lives at the MCP boundary in
// internal/mcp/tools.go — exactly where crm does its normalization — and the
// service re-asserts the balance invariant defensively.
//
// The journal is immutable: there is no upsert and no delete for journal facts.
// Corrections are linked reversals (Service.Reverse); the lone permitted
// mutation of an existing row is a posting's reconciliation status
// (Service.Reconcile). Money is integer cents, single currency (USD); reads
// return raw signed sums (ledger-cli convention), so a balance over every
// account sums to zero.
package ledger

import (
	"errors"
	"strings"
	"time"
)

// Error sentinels — the structured error vocabulary translated to wire shape in
// internal/mcp/tools.go (the same sentinel→wire pattern crm uses).
var (
	// ErrUnbalanced: explicit postings (after elision resolves) do not sum to zero.
	ErrUnbalanced = errors.New("ledger: unbalanced")
	// ErrBadRoot: an account's root segment is not one of the five known types.
	ErrBadRoot = errors.New("ledger: bad root")
	// ErrValidation: malformed input (too few postings, multiple elisions, bad
	// account path, bad date, unknown status, …).
	ErrValidation = errors.New("ledger: validation")
	// ErrNotFound: a transaction or posting id does not exist.
	ErrNotFound = errors.New("ledger: not found")
	// ErrAlreadyReversed: the target transaction already has a reversal mirror.
	ErrAlreadyReversed = errors.New("ledger: already reversed")
)

// Reconciliation states (PLAN.md §3, §4). Free transitions among the three;
// status lives on the posting and defaults to pending.
const (
	StatusPending    = "pending"
	StatusCleared    = "cleared"
	StatusReconciled = "reconciled"
)

// ValidStatus reports whether s is one of the three reconciliation states.
func ValidStatus(s string) bool {
	switch s {
	case StatusPending, StatusCleared, StatusReconciled:
		return true
	default:
		return false
	}
}

// RootInfo describes one of the five typed account roots: its canonical
// spelling, normal balance, and which financial statement it feeds.
type RootInfo struct {
	Type          string // canonical root, e.g. "Assets"
	NormalBalance string // "debit" or "credit"
	Statement     string // "balance_sheet" or "income_statement"
}

const (
	NormalDebit  = "debit"
	NormalCredit = "credit"

	StatementBalanceSheet = "balance_sheet"
	StatementIncome       = "income_statement"
)

// rootByAlias maps a case-folded root alias to its canonical RootInfo. Assets &
// Expenses are debit-normal; Liabilities, Equity & Income are credit-normal.
// Assets/Liabilities/Equity feed the balance sheet; Income/Expenses feed the
// income statement (P&L). "Revenue" is an accepted alias of "Income".
var rootByAlias = map[string]RootInfo{
	"assets":      {Type: "Assets", NormalBalance: NormalDebit, Statement: StatementBalanceSheet},
	"liabilities": {Type: "Liabilities", NormalBalance: NormalCredit, Statement: StatementBalanceSheet},
	"equity":      {Type: "Equity", NormalBalance: NormalCredit, Statement: StatementBalanceSheet},
	"income":      {Type: "Income", NormalBalance: NormalCredit, Statement: StatementIncome},
	"revenue":     {Type: "Income", NormalBalance: NormalCredit, Statement: StatementIncome},
	"expenses":    {Type: "Expenses", NormalBalance: NormalDebit, Statement: StatementIncome},
}

// Roots returns the five canonical roots in a stable display order, each with
// its normal balance and statement membership. Used by ledger_describe.
func Roots() []RootInfo {
	return []RootInfo{
		rootByAlias["assets"],
		rootByAlias["liabilities"],
		rootByAlias["equity"],
		rootByAlias["income"],
		rootByAlias["expenses"],
	}
}

// LookupRoot returns the RootInfo for an account's root segment (the part before
// the first colon), folding alias and case. ok is false when the root is not one
// of the five known types.
func LookupRoot(account string) (RootInfo, bool) {
	root := account
	if i := strings.IndexByte(account, ':'); i >= 0 {
		root = account[:i]
	}
	info, ok := rootByAlias[strings.ToLower(strings.TrimSpace(root))]
	return info, ok
}

// CanonicalizeAccount validates and normalizes a colon-path account. The root
// alias is folded (Revenue→Income) and its case is folded (assets→Assets) so the
// emergent chart can never fork; sub-path case is preserved as the agent wrote
// it. Empty segments (A::B), leading/trailing colons, and control characters are
// rejected. An unknown root returns ErrBadRoot; other malformations return
// ErrValidation.
func CanonicalizeAccount(account string) (string, error) {
	s := strings.TrimSpace(account)
	if s == "" {
		return "", ErrValidation
	}
	for _, r := range s {
		if r < 0x20 || r == 0x7f {
			return "", ErrValidation
		}
	}
	segs := strings.Split(s, ":")
	for _, seg := range segs {
		if strings.TrimSpace(seg) == "" {
			// Empty segment, or a leading/trailing colon.
			return "", ErrValidation
		}
	}
	info, ok := LookupRoot(segs[0])
	if !ok {
		return "", ErrBadRoot
	}
	segs[0] = info.Type
	return strings.Join(segs, ":"), nil
}

// Transaction is one journal entry: a set of balanced postings plus reversal
// linkage. Postings is populated for the rich read shape (get/record/reverse/
// reconcile return shape) and empty for the bare row.
type Transaction struct {
	ID           string
	Date         string // bare YYYY-MM-DD
	Description  string
	CreatedAt    time.Time
	ReversesID   *string
	ReversedByID *string
	Postings     []Posting
}

// Posting is one leg of a transaction. AmountCents is signed minor units
// (debit +, credit −), stored raw with no sign normalization.
type Posting struct {
	ID          string
	TxnID       string
	Account     string
	AmountCents int64
	Status      string
	Ord         int
}

// PostingInput is a resolved posting handed to Service.Record by the MCP
// boundary: the account is already canonicalized and the status already
// defaulted. AmountCents is nil for the (at most one) elided leg, which receives
// the balancing residual in Service.Record.
type PostingInput struct {
	Account     string
	AmountCents *int64
	Status      string
}

// RecordInput is the resolved transaction handed to Service.Record. Date is a
// validated bare YYYY-MM-DD; every posting's account is canonicalized and status
// is one of the three states.
type RecordInput struct {
	Date        string
	Description string
	Postings    []PostingInput
}

// Filter drives the balance and register reads. Query is a case-insensitive
// substring matched against the full account path. From/To bound the txn date
// inclusively (empty = open). Status, when set, filters postings by
// reconciliation state.
type Filter struct {
	Query  string
	From   string // inclusive YYYY-MM-DD, "" = open
	To     string // inclusive YYYY-MM-DD, "" = open
	Status string // "" = any
}
