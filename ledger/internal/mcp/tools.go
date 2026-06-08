package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strings"
	"time"

	"appkit"

	"ledger/internal/ledger"

	"eventplane/consumer"
	"eventplane/outbox"
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

// toolDescriptors returns the fixed eight-verb ledger surface (PLAN.md §2). Tool
// count is a function of verbs, not entities: there is one write entity (the
// balanced transaction) and an emergent, typed account tree. Schemas are
// hand-coded with per-field docs to improve LLM hinting.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("record"),
			"Post one balanced double-entry transaction. Provide 2+ postings whose signed amount_cents sum to zero (debit +, credit −). Exactly one posting MAY omit amount_cents to receive the balancing residual. Accounts are colon-paths whose root must be a known type; sub-accounts spring into existence on first posting. Returns the full transaction with the resolved residual and assigned ids.",
			obj(map[string]any{
				"date":        typd("string", "calendar day YYYY-MM-DD (a business day, not a timestamp)"),
				"description": typd("string", "payee / memo"),
				"status":      enumd("default reconciliation status for postings that omit their own (defaults to pending)", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
				"postings": array(obj(map[string]any{
					"account":      typd("string", "colon-path, e.g. Assets:Bank:Checking; root must be Assets|Liabilities|Equity|Income(alias Revenue)|Expenses — call describe to see the live chart"),
					"amount_cents": typd("integer", "signed minor units in USD cents (debit +, credit −); omit on at most one posting to receive the balancing residual"),
					"status":       enumd("this posting's reconciliation status; overrides the transaction default", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
				}, "account")),
			}, "date", "description", "postings")),

		desc(tool("reverse"),
			"The correction primitive. Posts the sign-flipped mirror of an existing transaction (whole-transaction only) and links the two both ways. The journal is immutable — there is no edit or delete; corrections are compensating facts. Blocked if the transaction is already reversed (reverse its mirror instead). Returns the new mirror transaction.",
			obj(map[string]any{
				"id":   typd("string", "id of the transaction to reverse"),
				"date": typd("string", "optional YYYY-MM-DD for the mirror (defaults to the original's date)"),
				"memo": typd("string", "optional description for the mirror (defaults to 'Reversal of: <original>')"),
			}, "id")),

		desc(tool("reconcile"),
			"Transition the reconciliation status of one or more postings — the ONLY permitted mutation of existing journal rows. It can never touch an amount, account, or date. Transitions among pending/cleared/reconciled are free (including backward). All-or-nothing: an unknown posting_id fails the whole call. Returns the affected transactions in full.",
			obj(map[string]any{
				"posting_ids": array(typ("string")),
				"status":      enumd("the status to set on every listed posting", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
			}, "posting_ids", "status")),

		desc(tool("balance"),
			"The `bal` report and the live chart of accounts. With no arguments returns the whole emergent account tree with raw signed balances (Assets/Expenses positive, Liabilities/Equity/Income negative; the whole-ledger total is 0). Filter by account substring, period, depth, and reconciliation status.",
			obj(map[string]any{
				"query":  typd("string", "case-insensitive substring matched against the full account path; omit for every account"),
				"period": periodSchema(),
				"depth":  typd("integer", "roll accounts up to this many colon-levels (1 = roots); omit or 0 for leaf accounts as posted"),
				"status": enumd("restrict to postings in this reconciliation state — e.g. cleared for a bank-reconciliation view", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
			})),

		desc(tool("register"),
			"The `reg` report — matched postings in chronological order with a running total; the list verb. Raw signed amounts like balance.",
			obj(map[string]any{
				"query":  typd("string", "case-insensitive substring matched against the full account path; omit for every posting"),
				"period": periodSchema(),
				"status": enumd("restrict to postings in this reconciliation state", ledger.StatusPending, ledger.StatusCleared, ledger.StatusReconciled),
			})),

		desc(tool("get"), "Fetch one transaction in full: all postings, per-posting status, ord, and reversal links.", obj(map[string]any{"id": typd("string", "the transaction id")}, "id")),

		desc(tool("describe"),
			"Discovery — the first call any agent should make. Returns the five typed roots (with normal balance and which statement they feed), the money unit (USD cents), the reconciliation states and their meaning, the live emergent account tree, and recipes for producing a balance sheet / P&L / customer statement from balance + register. Takes no inputs.",
			obj(map[string]any{})),

		desc(tool("health"), "Health + diagnostics for the ledger service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). Takes no inputs.", obj(map[string]any{})),

		desc(tool("reflection"),
			"Self-describe ledger's edges in the event graph. With no arguments, returns the index {publishes:[{type,description}], subscribes:[{source,filter,description}]} — ledger is a producer, so subscribes is empty. Pass 'event_type' (a published type) for its detail {type, description, schema, example}.",
			obj(map[string]any{
				"event_type": typd("string", "optional; a published event type to fetch the schema+example detail for"),
			})),
	}
}

// ── schema helpers ──────────────────────────────────────────────────────────

func desc(name, description string, schema map[string]any) map[string]any {
	return map[string]any{"name": name, "description": description, "inputSchema": schema}
}

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

// ── dispatch ──────────────────────────────────────────────────────────────

type toolCallParams struct {
	Name      string          `json:"name"`
	Arguments json.RawMessage `json:"arguments"`
}

func (h *Handler) handleToolCall(ctx context.Context, w http.ResponseWriter, req jsonRPCRequest, id Identity) {
	var p toolCallParams
	if err := json.Unmarshal(req.Params, &p); err != nil {
		writeJSONRPCError(w, req.ID, -32602, "invalid params")
		return
	}
	res, err := h.dispatchTool(ctx, p.Name, p.Arguments, id)
	if err != nil {
		writeJSONRPCResult(w, req.ID, toolResultErr(err.Error()))
		return
	}
	writeJSONRPCResult(w, req.ID, res)
}

func (h *Handler) dispatchTool(ctx context.Context, name string, argsRaw json.RawMessage, id Identity) (map[string]any, error) {
	svc := h.ledger
	switch name {
	case tool("record"):
		return toolRecord(ctx, svc, argsRaw)
	case tool("reverse"):
		return toolReverse(ctx, svc, argsRaw)
	case tool("reconcile"):
		return toolReconcile(ctx, svc, argsRaw)
	case tool("balance"):
		return toolBalance(ctx, svc, argsRaw)
	case tool("register"):
		return toolRegister(ctx, svc, argsRaw)
	case tool("get"):
		return toolGet(ctx, svc, argsRaw)
	case tool("describe"):
		return toolDescribe(ctx, svc)
	case tool("health"):
		return h.toolHealth(ctx, id)
	case tool("reflection"):
		return h.toolReflection(argsRaw)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── tool implementations ─────────────────────────────────────────────────

// toolHealth renders the shared health envelope (status/version/service/details)
// via appkit.Envelope and then adds the authenticated caller's identity — the
// end-to-end auth-chain proof (DECISIONS §6). ledger supplies no reporter, so
// details renders as {}.
func (h *Handler) toolHealth(ctx context.Context, id Identity) (map[string]any, error) {
	details := map[string]any{}
	if h.health != nil {
		d, err := h.health(ctx)
		if err != nil {
			details = map[string]any{"error": err.Error()}
		} else if d != nil {
			details = d
		}
	}
	env := appkit.Envelope(h.version, h.service, details) // status/version/service/details
	env["owner_email"] = id.OwnerEmail
	env["client_id"] = id.ClientID
	return toolResultJSON(env)
}

// toolReflection self-describes ledger's edges in the event graph (the
// ikigenba_<svc>_reflection tool). No event_type → the index {publishes,
// subscribes}; with event_type → that published type's {type, description,
// schema, example}. An unknown event_type returns a corrective error listing the
// valid types (the same bad_root pattern), not an empty result.
func (h *Handler) toolReflection(raw json.RawMessage) (map[string]any, error) {
	var a struct {
		EventType string `json:"event_type,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	if a.EventType != "" {
		detail, err := h.events.Detail(a.EventType)
		if err != nil {
			var unknown *outbox.UnknownEventTypeError
			if errors.As(err, &unknown) {
				return toolResultErr(reflectionUnknownTypeError(unknown)), nil
			}
			return nil, err
		}
		return toolResultJSON(detail)
	}
	return toolResultJSON(map[string]any{
		"publishes":  h.events.Index(),
		"subscribes": renderSubscriptions(h.subscriptions),
	})
}

// renderSubscriptions flattens the live subscription provider to the reflection
// in-edges: one {source, filter, description} per Subscription. The Handler is
// dropped — only the declared graph edge is reported. A nil provider (or nil
// result) renders as an empty list.
func renderSubscriptions(provider func() []consumer.Subscription) []map[string]any {
	out := []map[string]any{}
	if provider == nil {
		return out
	}
	for _, s := range provider() {
		out = append(out, map[string]any{
			"source":      s.Source,
			"filter":      s.Filter,
			"description": s.Description,
		})
	}
	return out
}

// reflectionUnknownTypeError renders the corrective error envelope for an unknown
// event_type, listing the valid types so the agent can self-correct (mirrors the
// bad_root corrective message).
func reflectionUnknownTypeError(e *outbox.UnknownEventTypeError) string {
	env := map[string]any{"error": map[string]any{
		"code":    "unknown_event_type",
		"message": "unknown event_type " + e.Type + "; valid types: " + strings.Join(e.Valid, ", "),
	}}
	b, _ := json.Marshal(env)
	return string(b)
}

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
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	date, err := parseDay(a.Date)
	if err != nil {
		return toolResultErr(translateLedgerError(err)), nil
	}
	if a.Status != "" && !ledger.ValidStatus(a.Status) {
		return toolResultErr(translateLedgerError(fmt.Errorf("%w: unknown status %q", ledger.ErrValidation, a.Status))), nil
	}
	// Well-formedness validated at this boundary (re-asserted in the service):
	// ≥2 postings, ≤1 elided amount.
	if len(a.Postings) < 2 {
		return toolResultErr(translateLedgerError(fmt.Errorf("%w: a transaction needs at least two postings", ledger.ErrValidation))), nil
	}
	elisions := 0
	postings := make([]ledger.PostingInput, len(a.Postings))
	for i, p := range a.Postings {
		account, err := ledger.CanonicalizeAccount(p.Account)
		if err != nil {
			return toolResultErr(translateLedgerError(err)), nil
		}
		status := p.Status
		if status == "" {
			status = a.Status // inherit the transaction default ("" → service defaults to pending)
		} else if !ledger.ValidStatus(status) {
			return toolResultErr(translateLedgerError(fmt.Errorf("%w: unknown posting status %q", ledger.ErrValidation, status))), nil
		}
		if p.AmountCents == nil {
			elisions++
		}
		postings[i] = ledger.PostingInput{Account: account, AmountCents: p.AmountCents, Status: status}
	}
	if elisions > 1 {
		return toolResultErr(translateLedgerError(fmt.Errorf("%w: at most one posting may elide its amount", ledger.ErrValidation))), nil
	}
	out, err := svc.Record(ctx, ledger.RecordInput{Date: date, Description: a.Description, Postings: postings})
	if err != nil {
		return toolResultErr(translateLedgerError(err)), nil
	}
	return toolResultJSON(transactionJSON(out))
}

func toolReverse(ctx context.Context, svc *ledger.Service, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		ID   string  `json:"id"`
		Date *string `json:"date,omitempty"`
		Memo *string `json:"memo,omitempty"`
	}
	if err := json.Unmarshal(raw, &a); err != nil {
		return nil, err
	}
	if a.Date != nil && *a.Date != "" {
		if _, err := parseDay(*a.Date); err != nil {
			return toolResultErr(translateLedgerError(err)), nil
		}
	}
	out, err := svc.Reverse(ctx, a.ID, a.Date, a.Memo)
	if err != nil {
		return toolResultErr(translateLedgerError(err)), nil
	}
	return toolResultJSON(transactionJSON(out))
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
		return toolResultErr(translateLedgerError(err)), nil
	}
	txns := make([]map[string]any, len(out))
	for i, t := range out {
		txns[i] = transactionJSON(t)
	}
	return toolResultJSON(map[string]any{"transactions": txns})
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
		return toolResultErr(translateLedgerError(err)), nil
	}
	rep, err := svc.Balance(ctx, f, a.Depth)
	if err != nil {
		return toolResultErr(translateLedgerError(err)), nil
	}
	lines := make([]map[string]any, len(rep.Lines))
	for i, l := range rep.Lines {
		lines[i] = map[string]any{"account": l.Account, "amount_cents": l.Sum}
	}
	return toolResultJSON(map[string]any{"lines": lines, "total": rep.Total, "unit": ledger.Unit})
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
		return toolResultErr(translateLedgerError(err)), nil
	}
	rep, err := svc.Register(ctx, f)
	if err != nil {
		return toolResultErr(translateLedgerError(err)), nil
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
	return toolResultJSON(map[string]any{"lines": lines, "unit": ledger.Unit})
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
		return toolResultErr(translateLedgerError(err)), nil
	}
	return toolResultJSON(transactionJSON(out))
}

func toolDescribe(ctx context.Context, svc *ledger.Service) (map[string]any, error) {
	rep, err := svc.Describe(ctx)
	if err != nil {
		return toolResultErr(translateLedgerError(err)), nil
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
	return toolResultJSON(map[string]any{
		"unit":                  rep.Unit,
		"roots":                 roots,
		"reconciliation_states": states,
		"accounts":              rep.Accounts,
		"recipes":               recipes,
	})
}

// ── shared helpers ──────────────────────────────────────────────────────

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
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
