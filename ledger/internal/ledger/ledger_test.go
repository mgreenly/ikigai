package ledger

import (
	"context"
	"database/sql"
	"errors"
	"testing"
	"time"

	"ledger/internal/db"

	_ "modernc.org/sqlite"
)

func openDB(t *testing.T) *sql.DB {
	t.Helper()
	conn, err := sql.Open("sqlite", ":memory:")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	if _, err := conn.Exec(`PRAGMA foreign_keys = ON`); err != nil {
		t.Fatalf("fk pragma: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return conn
}

func mkSvc(t *testing.T) *Service {
	s := NewService(openDB(t))
	s.Now = func() time.Time { return time.Now().UTC() }
	return s
}

func i64(v int64) *int64 { return &v }

// record is a small helper for tests: amounts may be nil to elide.
func record(t *testing.T, s *Service, date, desc string, legs ...PostingInput) Transaction {
	t.Helper()
	out, err := s.Record(context.Background(), RecordInput{Date: date, Description: desc, Postings: legs})
	if err != nil {
		t.Fatalf("record: %v", err)
	}
	return out
}

func leg(account string, cents *int64) PostingInput {
	return PostingInput{Account: account, AmountCents: cents}
}

// ── CanonicalizeAccount ─────────────────────────────────────────────────────

func TestCanonicalizeAccount(t *testing.T) {
	cases := []struct {
		in, want string
		err      error
	}{
		{"Assets:Bank:Checking", "Assets:Bank:Checking", nil},
		{"assets:bank:checking", "Assets:bank:checking", nil}, // root case folded, sub-path preserved
		{"Revenue:Hosting", "Income:Hosting", nil},            // alias folded
		{"revenue", "Income", nil},
		{"  Assets:Receivable:Acme  ", "Assets:Receivable:Acme", nil}, // trimmed
		{"Bogus:Account", "", ErrBadRoot},
		{"", "", ErrValidation},
		{"Assets::Checking", "", ErrValidation}, // empty segment
		{":Assets", "", ErrValidation},          // leading colon
		{"Assets:", "", ErrValidation},          // trailing colon
		{"Assets:Bank\x01", "", ErrValidation},  // control char
	}
	for _, c := range cases {
		got, err := CanonicalizeAccount(c.in)
		if c.err != nil {
			if !errors.Is(err, c.err) {
				t.Errorf("CanonicalizeAccount(%q): err = %v, want %v", c.in, err, c.err)
			}
			continue
		}
		if err != nil {
			t.Errorf("CanonicalizeAccount(%q): unexpected err %v", c.in, err)
		}
		if got != c.want {
			t.Errorf("CanonicalizeAccount(%q) = %q, want %q", c.in, got, c.want)
		}
	}
}

// ── Record / Get ────────────────────────────────────────────────────────────

func TestRecord_ElisionResolvesResidual(t *testing.T) {
	s := mkSvc(t)
	out := record(t, s, "2026-06-01", "Acme — June hosting",
		leg("Assets:Receivable:Acme", i64(5000)),
		leg("Income:Hosting", nil), // elided → -5000
	)
	if len(out.Postings) != 2 {
		t.Fatalf("postings: got %d want 2", len(out.Postings))
	}
	if out.Postings[1].AmountCents != -5000 {
		t.Errorf("residual: got %d want -5000", out.Postings[1].AmountCents)
	}
	if out.Postings[0].Ord != 0 || out.Postings[1].Ord != 1 {
		t.Errorf("ord not in array order: %+v", out.Postings)
	}
	for _, p := range out.Postings {
		if p.Status != StatusPending {
			t.Errorf("default status: got %q want pending", p.Status)
		}
	}

	// Get returns the same rich shape.
	got, err := s.Get(context.Background(), out.ID)
	if err != nil {
		t.Fatalf("get: %v", err)
	}
	if len(got.Postings) != 2 || got.Postings[1].AmountCents != -5000 {
		t.Errorf("get round-trip mismatch: %+v", got.Postings)
	}
}

func TestRecord_ExplicitBalanced_OK(t *testing.T) {
	s := mkSvc(t)
	out := record(t, s, "2026-06-09", "Acme payment",
		leg("Assets:Bank:Checking", i64(5000)),
		leg("Assets:Receivable:Acme", i64(-5000)),
	)
	if out.ID == "" {
		t.Fatal("expected an id")
	}
}

func TestRecord_Unbalanced_Errors(t *testing.T) {
	s := mkSvc(t)
	_, err := s.Record(context.Background(), RecordInput{
		Date: "2026-06-01", Description: "bad",
		Postings: []PostingInput{
			leg("Assets:Bank:Checking", i64(5000)),
			leg("Income:Hosting", i64(-4000)),
		},
	})
	if !errors.Is(err, ErrUnbalanced) {
		t.Fatalf("err = %v, want ErrUnbalanced", err)
	}
	// Nothing persisted.
	var n int
	if err := s.DB.QueryRow(`SELECT COUNT(*) FROM transactions`).Scan(&n); err != nil {
		t.Fatal(err)
	}
	if n != 0 {
		t.Errorf("rolled-back insert leaked %d transactions", n)
	}
}

func TestRecord_TwoElisions_Errors(t *testing.T) {
	s := mkSvc(t)
	_, err := s.Record(context.Background(), RecordInput{
		Date: "2026-06-01", Description: "ambiguous",
		Postings: []PostingInput{
			leg("Assets:Bank:Checking", nil),
			leg("Income:Hosting", nil),
		},
	})
	if !errors.Is(err, ErrValidation) {
		t.Fatalf("err = %v, want ErrValidation", err)
	}
}

func TestGet_NotFound(t *testing.T) {
	s := mkSvc(t)
	_, err := s.Get(context.Background(), "NOPE")
	if !errors.Is(err, ErrNotFound) {
		t.Fatalf("err = %v, want ErrNotFound", err)
	}
}

// ── Reverse ─────────────────────────────────────────────────────────────────

func TestReverse_MirrorsAndLinks(t *testing.T) {
	s := mkSvc(t)
	orig := record(t, s, "2026-06-01", "Acme — June hosting",
		leg("Assets:Receivable:Acme", i64(5000)),
		leg("Income:Hosting", i64(-5000)),
	)
	mirror, err := s.Reverse(context.Background(), orig.ID, nil, nil)
	if err != nil {
		t.Fatalf("reverse: %v", err)
	}
	if mirror.ReversesID == nil || *mirror.ReversesID != orig.ID {
		t.Errorf("mirror.reverses_id = %v, want %s", mirror.ReversesID, orig.ID)
	}
	// Signs flipped, status reset to pending.
	for i, p := range mirror.Postings {
		if p.AmountCents != -orig.Postings[i].AmountCents {
			t.Errorf("leg %d not sign-flipped: %d vs %d", i, p.AmountCents, orig.Postings[i].AmountCents)
		}
		if p.Status != StatusPending {
			t.Errorf("mirror leg %d status = %q, want pending", i, p.Status)
		}
	}
	// Original now linked forward.
	reloaded, err := s.Get(context.Background(), orig.ID)
	if err != nil {
		t.Fatal(err)
	}
	if reloaded.ReversedByID == nil || *reloaded.ReversedByID != mirror.ID {
		t.Errorf("original.reversed_by_id = %v, want %s", reloaded.ReversedByID, mirror.ID)
	}
}

func TestReverse_DoubleReverseBlocked_ReverseOfReverseAllowed(t *testing.T) {
	s := mkSvc(t)
	orig := record(t, s, "2026-06-01", "x",
		leg("Assets:Bank:Checking", i64(100)),
		leg("Income:Hosting", i64(-100)),
	)
	mirror, err := s.Reverse(context.Background(), orig.ID, nil, nil)
	if err != nil {
		t.Fatalf("first reverse: %v", err)
	}
	// Reversing the original again is blocked.
	if _, err := s.Reverse(context.Background(), orig.ID, nil, nil); !errors.Is(err, ErrAlreadyReversed) {
		t.Fatalf("double reverse err = %v, want ErrAlreadyReversed", err)
	}
	// Reversing the reversal is allowed (re-creates the original effect).
	if _, err := s.Reverse(context.Background(), mirror.ID, nil, nil); err != nil {
		t.Fatalf("reverse-of-reverse: %v", err)
	}
}

func TestReverse_NotFound(t *testing.T) {
	s := mkSvc(t)
	if _, err := s.Reverse(context.Background(), "NOPE", nil, nil); !errors.Is(err, ErrNotFound) {
		t.Fatalf("err = %v, want ErrNotFound", err)
	}
}

// ── Reads: Balance / Register / Describe ────────────────────────────────────

// seedBillingBooks records the §10 worked example: a charge and a cleared
// payment, leaving Acme's A/R at zero and the bank +5000.
func seedBillingBooks(t *testing.T, s *Service) {
	t.Helper()
	record(t, s, "2026-06-01", "Acme — June hosting",
		leg("Assets:Receivable:Acme", i64(5000)),
		leg("Income:Hosting", nil))
	out, err := s.Record(context.Background(), RecordInput{
		Date: "2026-06-09", Description: "Acme payment",
		Postings: []PostingInput{
			{Account: "Assets:Bank:Checking", AmountCents: i64(5000), Status: StatusCleared},
			{Account: "Assets:Receivable:Acme", AmountCents: nil},
		},
	})
	if err != nil {
		t.Fatalf("payment: %v", err)
	}
	_ = out
}

func TestBalance_WholeLedgerSumsToZero(t *testing.T) {
	s := mkSvc(t)
	seedBillingBooks(t, s)
	rep, err := s.Balance(context.Background(), Filter{}, 0)
	if err != nil {
		t.Fatalf("balance: %v", err)
	}
	if rep.Total != 0 {
		t.Errorf("whole-ledger total = %d, want 0", rep.Total)
	}
}

func TestBalance_QueryAndDepthRollup(t *testing.T) {
	s := mkSvc(t)
	seedBillingBooks(t, s)

	// Per-customer A/R: Acme nets to zero after payment.
	ar, err := s.Balance(context.Background(), Filter{Query: "Assets:Receivable"}, 0)
	if err != nil {
		t.Fatal(err)
	}
	var acme int64 = -1
	for _, l := range ar.Lines {
		if l.Account == "Assets:Receivable:Acme" {
			acme = l.Sum
		}
	}
	if acme != 0 {
		t.Errorf("Acme A/R = %d, want 0", acme)
	}

	// Depth-1 roll-up: every Assets:* leaf folds into one "Assets" line.
	d1, err := s.Balance(context.Background(), Filter{Query: "Assets"}, 1)
	if err != nil {
		t.Fatal(err)
	}
	for _, l := range d1.Lines {
		if l.Account == "Assets" {
			if l.Sum != 5000 {
				t.Errorf("rolled-up Assets = %d, want 5000", l.Sum)
			}
		}
		if got := truncateAccount(l.Account, 1); got != l.Account {
			t.Errorf("depth-1 line not truncated: %q", l.Account)
		}
	}
}

func TestBalance_StatusFilter(t *testing.T) {
	s := mkSvc(t)
	seedBillingBooks(t, s)
	// Only the cleared bank leg counts under status:"cleared".
	rep, err := s.Balance(context.Background(), Filter{Query: "Assets:Bank", Status: StatusCleared}, 0)
	if err != nil {
		t.Fatal(err)
	}
	var bank int64
	for _, l := range rep.Lines {
		if l.Account == "Assets:Bank:Checking" {
			bank = l.Sum
		}
	}
	if bank != 5000 {
		t.Errorf("cleared bank balance = %d, want 5000", bank)
	}
	// Pending filter excludes the cleared bank leg entirely.
	pend, err := s.Balance(context.Background(), Filter{Query: "Assets:Bank", Status: StatusPending}, 0)
	if err != nil {
		t.Fatal(err)
	}
	if len(pend.Lines) != 0 {
		t.Errorf("pending bank lines = %v, want none", pend.Lines)
	}
}

func TestRegister_RunningTotal(t *testing.T) {
	s := mkSvc(t)
	seedBillingBooks(t, s)
	rep, err := s.Register(context.Background(), Filter{Query: "Assets:Receivable:Acme"})
	if err != nil {
		t.Fatal(err)
	}
	if len(rep.Lines) != 2 {
		t.Fatalf("lines = %d, want 2", len(rep.Lines))
	}
	// +5000 charge then -5000 payment; running total returns to 0.
	if rep.Lines[0].RunningTotal != 5000 || rep.Lines[1].RunningTotal != 0 {
		t.Errorf("running totals = %d,%d want 5000,0", rep.Lines[0].RunningTotal, rep.Lines[1].RunningTotal)
	}
	// Chronological order.
	if rep.Lines[0].Date > rep.Lines[1].Date {
		t.Errorf("not chronological: %s then %s", rep.Lines[0].Date, rep.Lines[1].Date)
	}
}

func TestRegister_PeriodFilter(t *testing.T) {
	s := mkSvc(t)
	seedBillingBooks(t, s)
	// Restrict to June 1 only — just the charge.
	rep, err := s.Register(context.Background(), Filter{Query: "Assets:Receivable:Acme", From: "2026-06-01", To: "2026-06-01"})
	if err != nil {
		t.Fatal(err)
	}
	if len(rep.Lines) != 1 || rep.Lines[0].AmountCents != 5000 {
		t.Fatalf("June-1 register = %+v, want one +5000 line", rep.Lines)
	}
}

func TestDescribe_RootsAndLiveTree(t *testing.T) {
	s := mkSvc(t)
	// Empty ledger: roots present, no accounts yet.
	d0, err := s.Describe(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if len(d0.Roots) != 5 {
		t.Errorf("roots = %d, want 5", len(d0.Roots))
	}
	if d0.Unit != "USD cents" {
		t.Errorf("unit = %q", d0.Unit)
	}
	if len(d0.Accounts) != 0 {
		t.Errorf("empty-ledger accounts = %v, want none", d0.Accounts)
	}
	if len(d0.Recipes) == 0 || len(d0.Statuses) != 3 {
		t.Errorf("static content missing: recipes=%d statuses=%d", len(d0.Recipes), len(d0.Statuses))
	}

	seedBillingBooks(t, s)
	d1, err := s.Describe(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	want := map[string]bool{
		"Assets:Receivable:Acme": false,
		"Assets:Bank:Checking":   false,
		"Income:Hosting":         false,
	}
	for _, a := range d1.Accounts {
		if _, ok := want[a]; ok {
			want[a] = true
		}
	}
	for a, seen := range want {
		if !seen {
			t.Errorf("live tree missing %q", a)
		}
	}
}

// ── Reconcile ───────────────────────────────────────────────────────────────

func TestReconcile_ClearedVsLedgerBalanceDiffer(t *testing.T) {
	s := mkSvc(t)
	// A bank deposit recorded pending (both legs default pending).
	dep := record(t, s, "2026-06-02", "deposit",
		leg("Assets:Bank:Checking", i64(7000)),
		leg("Income:Hosting", i64(-7000)),
	)
	bankPosting := dep.Postings[0]
	if bankPosting.Account != "Assets:Bank:Checking" {
		t.Fatalf("unexpected leg order: %+v", dep.Postings)
	}

	// Before reconcile: cleared view is empty, ledger view shows 7000.
	cleared0, _ := s.Balance(context.Background(), Filter{Query: "Assets:Bank", Status: StatusCleared}, 0)
	if len(cleared0.Lines) != 0 {
		t.Errorf("pre-reconcile cleared = %+v, want empty", cleared0.Lines)
	}

	// Clear just the bank leg.
	affected, err := s.Reconcile(context.Background(), []string{bankPosting.ID}, StatusCleared)
	if err != nil {
		t.Fatalf("reconcile: %v", err)
	}
	if len(affected) != 1 || affected[0].ID != dep.ID {
		t.Fatalf("affected = %+v, want the deposit txn", affected)
	}
	var clearedLeg bool
	for _, p := range affected[0].Postings {
		if p.ID == bankPosting.ID && p.Status == StatusCleared {
			clearedLeg = true
		}
	}
	if !clearedLeg {
		t.Errorf("bank leg not cleared in returned txn: %+v", affected[0].Postings)
	}

	// After: cleared balance == ledger balance for the bank (only that leg).
	cleared1, _ := s.Balance(context.Background(), Filter{Query: "Assets:Bank", Status: StatusCleared}, 0)
	ledger1, _ := s.Balance(context.Background(), Filter{Query: "Assets:Bank"}, 0)
	if len(cleared1.Lines) != 1 || cleared1.Lines[0].Sum != 7000 {
		t.Errorf("cleared bank = %+v, want one 7000 line", cleared1.Lines)
	}
	if ledger1.Lines[0].Sum != 7000 {
		t.Errorf("ledger bank = %d, want 7000", ledger1.Lines[0].Sum)
	}
}

func TestReconcile_FreeTransitionsAndIdempotent(t *testing.T) {
	s := mkSvc(t)
	tx := record(t, s, "2026-06-02", "x",
		leg("Assets:Bank:Checking", i64(100)),
		leg("Income:Hosting", i64(-100)),
	)
	pid := tx.Postings[0].ID
	// pending -> reconciled -> back to pending (free, incl. backward).
	if _, err := s.Reconcile(context.Background(), []string{pid}, StatusReconciled); err != nil {
		t.Fatal(err)
	}
	// Idempotent no-op: setting to the current status succeeds.
	if _, err := s.Reconcile(context.Background(), []string{pid}, StatusReconciled); err != nil {
		t.Fatalf("idempotent reconcile: %v", err)
	}
	out, err := s.Reconcile(context.Background(), []string{pid}, StatusPending)
	if err != nil {
		t.Fatal(err)
	}
	if out[0].Postings[0].Status != StatusPending {
		t.Errorf("un-reconcile failed: %q", out[0].Postings[0].Status)
	}
}

func TestReconcile_UnknownIDFailsWholeBatch(t *testing.T) {
	s := mkSvc(t)
	tx := record(t, s, "2026-06-02", "x",
		leg("Assets:Bank:Checking", i64(100)),
		leg("Income:Hosting", i64(-100)),
	)
	good := tx.Postings[0].ID
	_, err := s.Reconcile(context.Background(), []string{good, "NOPE"}, StatusCleared)
	if !errors.Is(err, ErrNotFound) {
		t.Fatalf("err = %v, want ErrNotFound", err)
	}
	// The good posting must NOT have been cleared — all-or-nothing.
	got, _ := s.Get(context.Background(), tx.ID)
	if got.Postings[0].Status != StatusPending {
		t.Errorf("partial batch leaked a write: %q", got.Postings[0].Status)
	}
}

func TestReconcile_BadStatus(t *testing.T) {
	s := mkSvc(t)
	tx := record(t, s, "2026-06-02", "x",
		leg("Assets:Bank:Checking", i64(100)),
		leg("Income:Hosting", i64(-100)),
	)
	if _, err := s.Reconcile(context.Background(), []string{tx.Postings[0].ID}, "bogus"); !errors.Is(err, ErrValidation) {
		t.Fatalf("err = %v, want ErrValidation", err)
	}
}
