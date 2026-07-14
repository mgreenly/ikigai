package mcp

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"ledger/internal/ledger"
)

// timeFormat renders RFC3339Nano with fixed fractional width, matching crm's
// read API so timestamps are stable across the suite.
const timeFormat = "2006-01-02T15:04:05.000000000Z07:00"

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

// Tools returns ledger's service-owned MCP tool declarations. The shared appkit
// MCP transport appends the chassis health and reflection tools.
func Tools(svc *ledger.Service) []appkitmcp.Tool {
	if svc == nil {
		panic("mcp: ledger service is required")
	}
	return []appkitmcp.Tool{
		{
			Name:        tool("record"),
			Description: "Post one balanced double-entry transaction. Provide 2+ postings whose signed amount_cents sum to zero (debit +, credit −). Exactly one posting MAY omit amount_cents to receive the balancing residual. Accounts are colon-paths whose root must be a known type; sub-accounts spring into existence on first posting. Returns the full transaction with the resolved residual and assigned ids.",
			InputSchema: obj(map[string]any{
				"date":         typd("string", "calendar day YYYY-MM-DD (a business day, not a timestamp)"),
				"description":  typd("string", "payee / memo"),
				"status":       enumd("default reconciliation status for postings that omit their own (defaults to pending)", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
				"external_ref": typd("string", "<source>:<identifier> convention, e.g. dropbox:/bills/aws/2026-06.pdf@<content_hash> — a naming convention, not a validated format"),
				"postings": array(obj(map[string]any{
					"account":      typd("string", "colon-path, e.g. Assets:Bank:Checking; root must be Assets|Liabilities|Equity|Income(alias Revenue)|Expenses — call describe to see the live chart"),
					"amount_cents": typd("integer", "signed minor units in USD cents (debit +, credit −); omit on at most one posting to receive the balancing residual"),
					"status":       enumd("this posting's reconciliation status; overrides the transaction default", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
				}, "account")),
			}, "date", "description", "postings"),
			OutputSchema: transactionSchema(),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return toolRecord(ctx, svc, args)
			},
		},
		{
			Name:        tool("reverse"),
			Description: "The correction primitive. Posts the sign-flipped mirror of an existing transaction (whole-transaction only) and links the two both ways. The journal is immutable — there is no edit or delete; corrections are compensating facts. Blocked if the transaction is already reversed (reverse its mirror instead). Returns the new mirror transaction.",
			InputSchema: obj(map[string]any{
				"id":           typd("string", "id of the transaction to reverse"),
				"date":         typd("string", "optional YYYY-MM-DD for the mirror (defaults to the original's date)"),
				"memo":         typd("string", "optional description for the mirror (defaults to 'Reversal of: <original>')"),
				"external_ref": typd("string", "<source>:<identifier> convention, e.g. dropbox:/bills/aws/2026-06.pdf@<content_hash> — a naming convention, not a validated format"),
			}, "id"),
			OutputSchema: transactionSchema(),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return toolReverse(ctx, svc, args)
			},
		},
		{
			Name:        tool("reconcile"),
			Description: "Transition the reconciliation status of one or more postings — the ONLY permitted mutation of existing journal rows. It can never touch an amount, account, or date. Transitions among pending/cleared/reconciled are free (including backward). All-or-nothing: an unknown posting_id fails the whole call. Returns the affected transactions in full.",
			InputSchema: obj(map[string]any{
				"posting_ids": array(typ("string")),
				"status":      enumd("the status to set on every listed posting", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
			}, "posting_ids", "status"),
			OutputSchema: obj(map[string]any{
				"transactions": array(transactionSchema()),
			}, "transactions"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return toolReconcile(ctx, svc, args)
			},
		},
		{
			Name:        tool("balance"),
			Description: "The `bal` report and the live chart of accounts. With no arguments returns the whole emergent account tree with raw signed balances (Assets/Expenses positive, Liabilities/Equity/Income negative; the whole-ledger total is 0). Filter by account substring, period, depth, and reconciliation status.",
			InputSchema: obj(map[string]any{
				"query":  typd("string", "case-insensitive substring matched against the full account path; omit for every account"),
				"period": periodSchema(),
				"depth":  typd("integer", "roll accounts up to this many colon-levels (1 = roots); omit or 0 for leaf accounts as posted"),
				"status": enumd("restrict to postings in this reconciliation state — e.g. cleared for a bank-reconciliation view", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
			}),
			OutputSchema: obj(map[string]any{
				"lines": array(obj(map[string]any{
					"account":      typ("string"),
					"amount_cents": typ("integer"),
				}, "account", "amount_cents")),
				"total": typ("integer"),
				"unit":  typ("string"),
			}, "lines", "total", "unit"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return toolBalance(ctx, svc, args)
			},
		},
		{
			Name:        tool("register"),
			Description: "The `reg` report — matched postings in chronological order with a running total; the list verb. Raw signed amounts like balance.",
			InputSchema: obj(map[string]any{
				"query":  typd("string", "case-insensitive substring matched against the full account path; omit for every posting"),
				"period": periodSchema(),
				"status": enumd("restrict to postings in this reconciliation state", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
			}),
			OutputSchema: obj(map[string]any{
				"lines": array(obj(map[string]any{
					"txn_id":        typ("string"),
					"date":          typ("string"),
					"description":   typ("string"),
					"posting_id":    typ("string"),
					"account":       typ("string"),
					"amount_cents":  typ("integer"),
					"status":        typ("string"),
					"running_total": typ("integer"),
				}, "txn_id", "date", "description", "posting_id", "account", "amount_cents", "status", "running_total")),
				"unit": typ("string"),
			}, "lines", "unit"),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return toolRegister(ctx, svc, args)
			},
		},
		{
			Name:         tool("get"),
			Description:  "Fetch one transaction in full: all postings, per-posting status, ord, and reversal links.",
			InputSchema:  obj(map[string]any{"id": typd("string", "the transaction id")}, "id"),
			OutputSchema: transactionSchema(),
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return toolGet(ctx, svc, args)
			},
		},
		{
			Name:        tool("describe"),
			Description: "Discovery — the first call any agent should make. Returns the five typed roots (with normal balance and which statement they feed), the money unit (USD cents), the reconciliation states and their meaning, the live emergent account tree, and recipes for producing a balance sheet / P&L / customer statement from balance + register. Takes no inputs.",
			InputSchema: obj(map[string]any{}),
			Handler: func(ctx context.Context, _ json.RawMessage, _ server.Identity) (map[string]any, error) {
				return toolDescribe(ctx, svc)
			},
		},
	}
}

// ── schema helpers ──────────────────────────────────────────────────────────

func obj(props map[string]any, required ...string) map[string]any {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	return o
}

func typ(t string) map[string]any { return map[string]any{"type": t} }

func typd(t, d string) map[string]any { return map[string]any{"type": t, "description": d} }

func enumd(d string, values ...string) map[string]any {
	return map[string]any{"type": "string", "enum": values, "description": d}
}

func array(items map[string]any) map[string]any {
	return map[string]any{"type": "array", "items": items}
}

func transactionSchema() map[string]any {
	return obj(map[string]any{
		"id":             typ("string"),
		"date":           typ("string"),
		"description":    typ("string"),
		"created_at":     typ("string"),
		"reverses_id":    typ("string"),
		"reversed_by_id": typ("string"),
		"external_ref":   typ("string"),
		"postings": array(obj(map[string]any{
			"id":           typ("string"),
			"account":      typ("string"),
			"amount_cents": typ("integer"),
			"status":       typ("string"),
			"ord":          typ("integer"),
		}, "id", "account", "amount_cents", "status", "ord")),
	}, "id", "date", "description", "created_at", "postings")
}

// periodSchema describes the dual-form period argument: a bucket string
// ("2026", "2026-06", "2026-06-01") or an inclusive {from,to} range.
func periodSchema() map[string]any {
	return map[string]any{
		"description": "reporting period: a bucket string \"2026\" | \"2026-06\" | \"2026-06-01\", or an inclusive range {\"from\":\"YYYY-MM-DD\",\"to\":\"YYYY-MM-DD\"}",
		"oneOf": []map[string]any{
			{"type": "string"},
			{"type": "object", "properties": map[string]any{
				"from": typ("string"),
				"to":   typ("string"),
			}},
		},
	}
}

// ── tool implementations ─────────────────────────────────────────────────

type postingArg struct {
	Account     string `json:"account"`
	AmountCents *int64 `json:"amount_cents,omitempty"`
	Status      string `json:"status,omitempty"`
}

func toolRecord(ctx context.Context, svc *ledger.Service, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Date        string       `json:"date"`
		Description string       `json:"description"`
		Status      string       `json:"status,omitempty"`
		Postings    []postingArg `json:"postings"`
		ExternalRef *string      `json:"external_ref,omitempty"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if a.ExternalRef != nil && *a.ExternalRef == "" {
		return translateLedgerError(fmt.Errorf("%w: external_ref must not be empty", ledger.ErrValidation)), nil
	}
	date, err := parseDay(a.Date)
	if err != nil {
		return translateLedgerError(err), nil
	}
	if a.Status != "" && !ledger.ValidStatus(a.Status) {
		return translateLedgerError(fmt.Errorf("%w: unknown status %q", ledger.ErrValidation, a.Status)), nil
	}
	// Well-formedness validated at this boundary (re-asserted in the service):
	// ≥2 postings, ≤1 elided amount.
	if len(a.Postings) < 2 {
		return translateLedgerError(fmt.Errorf("%w: a transaction needs at least two postings", ledger.ErrValidation)), nil
	}
	elisions := 0
	postings := make([]ledger.PostingInput, len(a.Postings))
	for i, p := range a.Postings {
		account, err := ledger.CanonicalizeAccount(p.Account)
		if err != nil {
			return translateLedgerError(err), nil
		}
		status := p.Status
		if status == "" {
			status = a.Status // inherit the transaction default ("" → service defaults to pending)
		} else if !ledger.ValidStatus(status) {
			return translateLedgerError(fmt.Errorf("%w: unknown posting status %q", ledger.ErrValidation, status)), nil
		}
		if p.AmountCents == nil {
			elisions++
		}
		postings[i] = ledger.PostingInput{Account: account, AmountCents: p.AmountCents, Status: status}
	}
	if elisions > 1 {
		return translateLedgerError(fmt.Errorf("%w: at most one posting may elide its amount", ledger.ErrValidation)), nil
	}
	out, err := svc.Record(ctx, ledger.RecordInput{Date: date, Description: a.Description, Postings: postings, ExternalRef: a.ExternalRef})
	if err != nil {
		return translateLedgerError(err), nil
	}
	return appkitmcp.StructuredResult(transactionJSON(out))
}

func toolReverse(ctx context.Context, svc *ledger.Service, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID          string  `json:"id"`
		Date        *string `json:"date,omitempty"`
		Memo        *string `json:"memo,omitempty"`
		ExternalRef *string `json:"external_ref,omitempty"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if a.Date != nil && *a.Date != "" {
		if _, err := parseDay(*a.Date); err != nil {
			return translateLedgerError(err), nil
		}
	}
	if a.ExternalRef != nil && *a.ExternalRef == "" {
		return translateLedgerError(fmt.Errorf("%w: external_ref must not be empty", ledger.ErrValidation)), nil
	}
	out, err := svc.Reverse(ctx, a.ID, a.Date, a.Memo, a.ExternalRef)
	if err != nil {
		return translateLedgerError(err), nil
	}
	return appkitmcp.StructuredResult(transactionJSON(out))
}

func toolReconcile(ctx context.Context, svc *ledger.Service, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		PostingIDs []string `json:"posting_ids"`
		Status     string   `json:"status"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	out, err := svc.Reconcile(ctx, a.PostingIDs, a.Status)
	if err != nil {
		return translateLedgerError(err), nil
	}
	txns := make([]map[string]any, len(out))
	for i, t := range out {
		txns[i] = transactionJSON(t)
	}
	return appkitmcp.StructuredResult(map[string]any{"transactions": txns})
}

func toolBalance(ctx context.Context, svc *ledger.Service, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Query  string          `json:"query,omitempty"`
		Period json.RawMessage `json:"period,omitempty"`
		Depth  int             `json:"depth,omitempty"`
		Status string          `json:"status,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	f, err := buildFilter(a.Query, a.Period, a.Status)
	if err != nil {
		return translateLedgerError(err), nil
	}
	rep, err := svc.Balance(ctx, f, a.Depth)
	if err != nil {
		return translateLedgerError(err), nil
	}
	lines := make([]map[string]any, len(rep.Lines))
	for i, l := range rep.Lines {
		lines[i] = map[string]any{"account": l.Account, "amount_cents": l.Sum}
	}
	return appkitmcp.StructuredResult(map[string]any{"lines": lines, "total": rep.Total, "unit": ledger.Unit})
}

func toolRegister(ctx context.Context, svc *ledger.Service, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Query  string          `json:"query,omitempty"`
		Period json.RawMessage `json:"period,omitempty"`
		Status string          `json:"status,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	f, err := buildFilter(a.Query, a.Period, a.Status)
	if err != nil {
		return translateLedgerError(err), nil
	}
	rep, err := svc.Register(ctx, f)
	if err != nil {
		return translateLedgerError(err), nil
	}
	lines := make([]map[string]any, len(rep.Lines))
	for i, l := range rep.Lines {
		lines[i] = map[string]any{
			"txn_id":        l.TxnID,
			"date":          l.Date,
			"description":   l.Description,
			"posting_id":    l.PostingID,
			"account":       l.Account,
			"amount_cents":  l.AmountCents,
			"status":        l.Status,
			"running_total": l.RunningTotal,
		}
	}
	return appkitmcp.StructuredResult(map[string]any{"lines": lines, "unit": ledger.Unit})
}

func toolGet(ctx context.Context, svc *ledger.Service, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	out, err := svc.Get(ctx, a.ID)
	if err != nil {
		return translateLedgerError(err), nil
	}
	return appkitmcp.StructuredResult(transactionJSON(out))
}

func toolDescribe(ctx context.Context, svc *ledger.Service) (map[string]any, error) {
	rep, err := svc.Describe(ctx)
	if err != nil {
		return translateLedgerError(err), nil
	}
	roots := make([]map[string]any, len(rep.Roots))
	for i, r := range rep.Roots {
		roots[i] = map[string]any{"type": r.Type, "normal_balance": r.NormalBalance, "statement": r.Statement}
	}
	states := make([]map[string]any, len(rep.Statuses))
	for i, s := range rep.Statuses {
		states[i] = map[string]any{"status": s.Status, "meaning": s.Meaning}
	}
	recipes := make([]map[string]any, len(rep.Recipes))
	for i, rc := range rep.Recipes {
		recipes[i] = map[string]any{"name": rc.Name, "how": rc.How}
	}
	payload := map[string]any{
		"unit":                  rep.Unit,
		"roots":                 roots,
		"reconciliation_states": states,
		"accounts":              rep.Accounts,
		"recipes":               recipes,
		"external_refs":         rep.ExternalRefs,
	}
	b, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}
	return appkitmcp.TextResult(string(b)), nil
}

// transactionJSON renders the rich transaction shape returned by
// record/reverse/reconcile/get.
func transactionJSON(t ledger.Transaction) map[string]any {
	postings := make([]map[string]any, len(t.Postings))
	for i, p := range t.Postings {
		postings[i] = map[string]any{
			"id":           p.ID,
			"account":      p.Account,
			"amount_cents": p.AmountCents,
			"status":       p.Status,
			"ord":          p.Ord,
		}
	}
	out := map[string]any{
		"id":          t.ID,
		"date":        t.Date,
		"description": t.Description,
		"created_at":  t.CreatedAt.UTC().Format(timeFormat),
		"postings":    postings,
	}
	if t.ReversesID != nil {
		out["reverses_id"] = *t.ReversesID
	}
	if t.ReversedByID != nil {
		out["reversed_by_id"] = *t.ReversedByID
	}
	if t.ExternalRef != nil {
		out["external_ref"] = *t.ExternalRef
	}
	return out
}

// buildFilter assembles a ledger.Filter from the query/period/status args,
// expanding the period bucket-or-range and validating the status enum. This is
// the date-parsing / normalization site (PLAN.md §8).
func buildFilter(query string, period json.RawMessage, status string) (ledger.Filter, error) {
	from, to, err := parsePeriod(period)
	if err != nil {
		return ledger.Filter{}, err
	}
	if status != "" && !ledger.ValidStatus(status) {
		return ledger.Filter{}, fmt.Errorf("%w: unknown status %q", ledger.ErrValidation, status)
	}
	return ledger.Filter{Query: query, From: from, To: to, Status: status}, nil
}

// parseDay enforces a canonical zero-padded YYYY-MM-DD calendar day.
func parseDay(s string) (string, error) {
	tt, err := time.Parse("2006-01-02", s)
	if err != nil || tt.Format("2006-01-02") != s {
		return "", fmt.Errorf("%w: date must be a valid YYYY-MM-DD calendar day, got %q", ledger.ErrValidation, s)
	}
	return s, nil
}

// parsePeriod expands the dual-form period argument into an inclusive [from,to]
// date range over the YYYY-MM-DD date column. An absent period yields an open
// range. A bucket string ("2026"|"2026-06"|"2026-06-01") expands; an object
// {from,to} is validated as-is.
func parsePeriod(raw json.RawMessage) (from, to string, err error) {
	if len(raw) == 0 || string(raw) == "null" {
		return "", "", nil
	}
	var bucket string
	if json.Unmarshal(raw, &bucket) == nil {
		return expandBucket(bucket)
	}
	var rng struct {
		From string `json:"from"`
		To   string `json:"to"`
	}
	if err := json.Unmarshal(raw, &rng); err != nil {
		return "", "", fmt.Errorf("%w: period must be a bucket string or {from,to}", ledger.ErrValidation)
	}
	if rng.From != "" {
		if from, err = parseDay(rng.From); err != nil {
			return "", "", err
		}
	}
	if rng.To != "" {
		if to, err = parseDay(rng.To); err != nil {
			return "", "", err
		}
	}
	return from, to, nil
}

func expandBucket(b string) (from, to string, err error) {
	switch len(b) {
	case 4: // YYYY
		if _, e := time.Parse("2006", b); e != nil {
			return "", "", badPeriod(b)
		}
		return b + "-01-01", b + "-12-31", nil
	case 7: // YYYY-MM
		tt, e := time.Parse("2006-01", b)
		if e != nil {
			return "", "", badPeriod(b)
		}
		return b + "-01", tt.AddDate(0, 1, -1).Format("2006-01-02"), nil
	case 10: // YYYY-MM-DD
		d, e := parseDay(b)
		if e != nil {
			return "", "", e
		}
		return d, d, nil
	default:
		return "", "", badPeriod(b)
	}
}

func badPeriod(b string) error {
	return fmt.Errorf("%w: period bucket %q must be YYYY, YYYY-MM, or YYYY-MM-DD", ledger.ErrValidation, b)
}
