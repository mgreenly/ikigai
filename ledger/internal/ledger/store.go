package ledger

import (
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"
)

// Store is the SQL-only data layer (the crm repo.go role). Every method takes
// *sql.Tx so the Service composes transactions and the atomic outbox append.
type Store struct{}

// NewStore builds a Store.
func NewStore() *Store { return &Store{} }

const rfc = time.RFC3339Nano

// ── writes ────────────────────────────────────────────────────────────────

// InsertTransaction inserts the bare transaction row (no postings).
func (Store) InsertTransaction(tx *sql.Tx, t Transaction) error {
	_, err := tx.Exec(`
		INSERT INTO transactions (id, date, description, created_at, reverses_id, reversed_by_id, external_ref)
		VALUES (?, ?, ?, ?, ?, ?, ?)
	`, t.ID, t.Date, t.Description, t.CreatedAt.UTC().Format(rfc),
		nullString(t.ReversesID), nullString(t.ReversedByID), nullString(t.ExternalRef))
	if err != nil {
		return fmt.Errorf("insert transaction: %w", err)
	}
	return nil
}

// InsertPosting inserts one posting leg.
func (Store) InsertPosting(tx *sql.Tx, p Posting) error {
	_, err := tx.Exec(`
		INSERT INTO postings (id, txn_id, account, amount_cents, status, ord)
		VALUES (?, ?, ?, ?, ?, ?)
	`, p.ID, p.TxnID, p.Account, p.AmountCents, p.Status, p.Ord)
	if err != nil {
		return fmt.Errorf("insert posting: %w", err)
	}
	return nil
}

// SetReversedBy stamps reversed_by_id on the original transaction, marking it
// reversed. Returns ErrNotFound if no such id, ErrAlreadyReversed if it already
// carries a mirror (the double-reversal guard).
func (Store) SetReversedBy(tx *sql.Tx, originalID, mirrorID string) error {
	res, err := tx.Exec(`
		UPDATE transactions SET reversed_by_id = ?
		WHERE id = ? AND reversed_by_id IS NULL
	`, mirrorID, originalID)
	if err != nil {
		return fmt.Errorf("set reversed_by: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		// Either the id does not exist or it is already reversed. Disambiguate.
		var existing sql.NullString
		row := tx.QueryRow(`SELECT reversed_by_id FROM transactions WHERE id = ?`, originalID)
		if err := row.Scan(&existing); err != nil {
			if errors.Is(err, sql.ErrNoRows) {
				return ErrNotFound
			}
			return fmt.Errorf("check reversed_by: %w", err)
		}
		return ErrAlreadyReversed
	}
	return nil
}

// UpdatePostingStatus sets a posting's reconciliation status. Returns ErrNotFound
// if the posting id does not exist.
func (Store) UpdatePostingStatus(tx *sql.Tx, postingID, status string) error {
	res, err := tx.Exec(`UPDATE postings SET status = ? WHERE id = ?`, status, postingID)
	if err != nil {
		return fmt.Errorf("update posting status: %w", err)
	}
	if n, _ := res.RowsAffected(); n == 0 {
		return ErrNotFound
	}
	return nil
}

// ── reads ─────────────────────────────────────────────────────────────────

// GetTransaction returns the bare transaction row (no postings). ErrNotFound if
// absent.
func (Store) GetTransaction(tx *sql.Tx, id string) (Transaction, error) {
	row := tx.QueryRow(`
		SELECT id, date, description, created_at, reverses_id, reversed_by_id, external_ref
		FROM transactions WHERE id = ?
	`, id)
	return scanTransaction(row)
}

// GetTransactionByExternalRef returns the transaction that permanently claims ref.
func (Store) GetTransactionByExternalRef(tx *sql.Tx, ref string) (Transaction, error) {
	row := tx.QueryRow(`
		SELECT id, date, description, created_at, reverses_id, reversed_by_id, external_ref
		FROM transactions WHERE external_ref = ?
	`, ref)
	return scanTransaction(row)
}

// ListPostings returns a transaction's postings in ord order.
func (Store) ListPostings(tx *sql.Tx, txnID string) ([]Posting, error) {
	rows, err := tx.Query(`
		SELECT id, txn_id, account, amount_cents, status, ord
		FROM postings WHERE txn_id = ? ORDER BY ord ASC
	`, txnID)
	if err != nil {
		return nil, fmt.Errorf("list postings: %w", err)
	}
	defer rows.Close()
	var out []Posting
	for rows.Next() {
		p, err := scanPosting(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, p)
	}
	return out, rows.Err()
}

// GetPosting returns one posting by id. ErrNotFound if absent.
func (Store) GetPosting(tx *sql.Tx, postingID string) (Posting, error) {
	row := tx.QueryRow(`
		SELECT id, txn_id, account, amount_cents, status, ord
		FROM postings WHERE id = ?
	`, postingID)
	return scanPosting(row)
}

// AccountSum is one row of a balance report: an account and its raw signed sum.
type AccountSum struct {
	Account string
	Sum     int64
}

// SumByAccount returns the signed sum of amount_cents grouped by exact account
// path, filtered by f, ordered by account. Roll-up by depth happens in the
// service over these leaf sums.
func (Store) SumByAccount(tx *sql.Tx, f Filter) ([]AccountSum, error) {
	where, args := buildWhere(f)
	q := `SELECT p.account, COALESCE(SUM(p.amount_cents), 0)
		FROM postings p JOIN transactions t ON t.id = p.txn_id`
	if where != "" {
		q += " WHERE " + where
	}
	q += " GROUP BY p.account ORDER BY p.account ASC"
	rows, err := tx.Query(q, args...)
	if err != nil {
		return nil, fmt.Errorf("sum by account: %w", err)
	}
	defer rows.Close()
	var out []AccountSum
	for rows.Next() {
		var a AccountSum
		if err := rows.Scan(&a.Account, &a.Sum); err != nil {
			return nil, fmt.Errorf("scan account sum: %w", err)
		}
		out = append(out, a)
	}
	return out, rows.Err()
}

// RegisterRow is one posting as rendered by the register report, carrying its
// parent transaction's date/description.
type RegisterRow struct {
	TxnID       string
	Date        string
	Description string
	PostingID   string
	Account     string
	AmountCents int64
	Status      string
	Ord         int
}

// RegisterPostings returns the postings matching f, in chronological order
// (date ASC, txn id ASC, ord ASC) so the service can compute a running total.
func (Store) RegisterPostings(tx *sql.Tx, f Filter) ([]RegisterRow, error) {
	where, args := buildWhere(f)
	q := `SELECT t.id, t.date, t.description, p.id, p.account, p.amount_cents, p.status, p.ord
		FROM postings p JOIN transactions t ON t.id = p.txn_id`
	if where != "" {
		q += " WHERE " + where
	}
	q += " ORDER BY t.date ASC, t.id ASC, p.ord ASC"
	rows, err := tx.Query(q, args...)
	if err != nil {
		return nil, fmt.Errorf("register postings: %w", err)
	}
	defer rows.Close()
	var out []RegisterRow
	for rows.Next() {
		var r RegisterRow
		if err := rows.Scan(&r.TxnID, &r.Date, &r.Description, &r.PostingID, &r.Account, &r.AmountCents, &r.Status, &r.Ord); err != nil {
			return nil, fmt.Errorf("scan register row: %w", err)
		}
		out = append(out, r)
	}
	return out, rows.Err()
}

// DistinctAccounts returns every account that exists (has at least one posting),
// in ascending path order — the emergent chart of accounts.
func (Store) DistinctAccounts(tx *sql.Tx) ([]string, error) {
	rows, err := tx.Query(`SELECT DISTINCT account FROM postings ORDER BY account ASC`)
	if err != nil {
		return nil, fmt.Errorf("distinct accounts: %w", err)
	}
	defer rows.Close()
	var out []string
	for rows.Next() {
		var a string
		if err := rows.Scan(&a); err != nil {
			return nil, fmt.Errorf("scan account: %w", err)
		}
		out = append(out, a)
	}
	return out, rows.Err()
}

// ── filter / scan helpers ───────────────────────────────────────────────────

// buildWhere renders the shared WHERE fragment for the balance/register reads.
// Query is a case-insensitive substring on the full account path; From/To bound
// the txn date inclusively; Status filters the posting state.
func buildWhere(f Filter) (string, []any) {
	var (
		clauses []string
		args    []any
	)
	if f.Query != "" {
		clauses = append(clauses, "p.account LIKE '%' || ? || '%' COLLATE NOCASE")
		args = append(args, f.Query)
	}
	if f.From != "" {
		clauses = append(clauses, "t.date >= ?")
		args = append(args, f.From)
	}
	if f.To != "" {
		clauses = append(clauses, "t.date <= ?")
		args = append(args, f.To)
	}
	if f.Status != "" {
		clauses = append(clauses, "p.status = ?")
		args = append(args, f.Status)
	}
	return strings.Join(clauses, " AND "), args
}

type rowScanner interface {
	Scan(dest ...any) error
}

func scanTransaction(r rowScanner) (Transaction, error) {
	var (
		t                                     Transaction
		createdAt                             string
		reversesID, reversedByID, externalRef sql.NullString
	)
	if err := r.Scan(&t.ID, &t.Date, &t.Description, &createdAt, &reversesID, &reversedByID, &externalRef); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return Transaction{}, ErrNotFound
		}
		return Transaction{}, fmt.Errorf("scan transaction: %w", err)
	}
	t.CreatedAt, _ = time.Parse(rfc, createdAt)
	if reversesID.Valid {
		t.ReversesID = &reversesID.String
	}
	if reversedByID.Valid {
		t.ReversedByID = &reversedByID.String
	}
	if externalRef.Valid {
		t.ExternalRef = &externalRef.String
	}
	return t, nil
}

func scanPosting(r rowScanner) (Posting, error) {
	var p Posting
	if err := r.Scan(&p.ID, &p.TxnID, &p.Account, &p.AmountCents, &p.Status, &p.Ord); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return Posting{}, ErrNotFound
		}
		return Posting{}, fmt.Errorf("scan posting: %w", err)
	}
	return p, nil
}

func nullString(s *string) any {
	if s == nil {
		return nil
	}
	return *s
}
