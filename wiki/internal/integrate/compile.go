package integrate

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"

	"agentkit/provider"

	"wiki/internal/config"
)

// Compile is the digest pass's first integrator stage (design §5): one
// full-context, structured, tool-less LLM call that reads a pile of pending event
// rows and emits subjects, targeting EXTRACT'S output schema directly (no
// prose-digest artifact). It plays extract's role for event piles — same
// structured, tool-less, golden-testable call shape — and enters the shared
// resolve→merge→commit pipeline at resolve, so merge cannot tell which integrator
// ran (design §5: "the manifest generalizes").
//
// Compile's two §5 deltas over extract: every claim carries an explicit per-claim
// cites array (the inbox ids of the events the claim rests on — events are
// presented with their ids visible), and occurred_at is resolved from the event
// payloads. Aggregation is itself knowledge creation: compile narrates outcomes
// across events, never per-event deltas, and never infers beyond what the cited
// events jointly assert.
//
// Compile is a clean, externally-callable function over an injected
// (prompt, model, effort) triple (eval obligation 1): the harness scores it by
// swapping config.CallSite and calling the same function. It emits per-claim cites
// + occurred_at as distinct, scorable outputs (obligation 3 — the
// citation-mis-attribution risk the harness measures).

// EventRow is one pending event the digest compiles: its inbox id (presented to
// the model so claims can cite it) and its payload bytes (the envelope JSON the
// model reads). The integrator never inspects the inbox threshold — the source
// hands it the bytes.
type EventRow struct {
	// ID is the inbox row id — presented to the model so a claim's cites can name
	// the specific event(s) it rests on (design §5).
	ID string
	// Source is the event source (e.g. "crm:deal.closed"); rendered as framing so
	// the model knows each event's origin.
	Source string
	// Payload is the event envelope JSON the model reads.
	Payload []byte
}

// eventSource fetches the pending event rows a digest entry compiles. Declared as
// an interface so the integrator is unit-testable without a live inbox store; the
// composition root wires a real selector-backed implementation. The returned id
// list is also the digest's STAMP-BY-ID-LIST set (design §5: "stamp by id list,
// never by selector") — re-evaluating the selector at commit would silently drop
// mid-run arrivals.
type eventSource interface {
	// Events returns the pending event rows the named digest entry selects.
	Events(ctx context.Context, entry string) ([]EventRow, error)
}

// Compiler runs the compile stage with an injected call-site triple. Construct it
// once at the composition root with the wrapper adapter and the compile CallSite;
// the worker calls Compile per digest run.
type Compiler struct {
	caller structuredCaller
	site   config.CallSite
}

// NewCompiler builds a Compiler over a structured caller and the compile call-site
// triple. The triple (prompt/model/effort) is injected — Compile never reads a
// constant or env (design §10 / obligation 1).
func NewCompiler(caller structuredCaller, site config.CallSite) *Compiler {
	return &Compiler{caller: caller, site: site}
}

// rawCompileSubject is the compile call's wire shape — extract's subject shape
// with per-claim cites. ParseCompile validates and converts it to []Subject.
type rawCompileSubject struct {
	Type       string            `json:"type"`
	Kind       string            `json:"kind"`
	Name       string            `json:"name"`
	Aliases    []string          `json:"aliases"`
	Claims     []rawCompileClaim `json:"claims"`
	OccurredAt string            `json:"occurred_at"`
}

type rawCompileClaim struct {
	Text  string   `json:"text"`
	Cites []string `json:"cites"`
}

type compileEnvelope struct {
	Subjects []rawCompileSubject `json:"subjects"`
}

// Compile runs the digest-pass compile call over a pile of pending event rows: it
// renders each event (id + source + payload) into the user message, invokes the
// injected structured triple, then parses and schema-validates the result into
// []Subject. The valid inbox-id set (validIDs) is the ids of the events presented,
// so a claim citing an id outside the pile is rejected — the model cannot
// fabricate a citation (the mis-attribution risk obligation 3 names is caught
// here for ids outside the pile; an id swap WITHIN the pile is golden-testable,
// the accepted §5 risk).
func (c *Compiler) Compile(ctx context.Context, events []EventRow) ([]Subject, error) {
	user := renderEvents(events)
	msgs := []provider.Message{{
		Role:   provider.RoleUser,
		Blocks: []provider.Block{provider.TextBlock{Text: user}},
	}}

	raw, err := c.caller.Structured(ctx, c.site, CompileSchema, msgs)
	if err != nil {
		return nil, fmt.Errorf("compile: structured call: %w", err)
	}

	validIDs := make(map[string]struct{}, len(events))
	for _, e := range events {
		validIDs[e.ID] = struct{}{}
	}
	return ParseCompile(raw, validIDs)
}

// renderEvents builds the human-readable event pile that precedes the schema
// instruction: one block per event, each labeled with its inbox id and source so
// the model can cite the specific events a claim rests on (design §5: "events are
// presented to compile with their inbox ids visible").
func renderEvents(events []EventRow) string {
	var b strings.Builder
	b.WriteString("--- event pile (cite events by [id]) ---\n")
	for _, e := range events {
		fmt.Fprintf(&b, "\n[%s] source: %s\n%s\n", e.ID, e.Source, strings.TrimSpace(string(e.Payload)))
	}
	b.WriteString("\n--- end event pile ---\n")
	return b.String()
}

// ParseCompile parses and validates a compile response body into []Subject. It is
// separated from the call so the prompt-default gate and goldens can exercise the
// parser + schema offline against a committed fixture, with no client (obligation
// 5 / the standing prompt gate). validIDs is the set of inbox ids presented to the
// model; every claim cite must be in it (a cite outside the pile is a fabricated
// citation). occurred_at is events-only (design §4.1/§5).
func ParseCompile(raw string, validIDs map[string]struct{}) ([]Subject, error) {
	var env compileEnvelope
	if err := json.Unmarshal([]byte(stripCodeFence(raw)), &env); err != nil {
		return nil, fmt.Errorf("compile: parse response: %w", err)
	}

	out := make([]Subject, 0, len(env.Subjects))
	for i, rs := range env.Subjects {
		if err := validateRawCompileSubject(rs, validIDs); err != nil {
			return nil, fmt.Errorf("compile: subject %d: %w", i, err)
		}
		claims := make([]Claim, 0, len(rs.Claims))
		for _, rc := range rs.Claims {
			claims = append(claims, Claim{
				Text:  strings.TrimSpace(rc.Text),
				Cites: cleanCites(rc.Cites),
			})
		}
		s := Subject{
			Type:    rs.Type,
			Kind:    strings.TrimSpace(rs.Kind),
			Name:    strings.TrimSpace(rs.Name),
			Aliases: cleanAliases(rs.Aliases),
			Claims:  claims,
		}
		if rs.Type == TypeEvent {
			s.OccurredAt = strings.TrimSpace(rs.OccurredAt)
		}
		out = append(out, s)
	}
	return out, nil
}

// validateRawCompileSubject enforces the §5 output contract structurally: a
// closed-set type, a non-empty name, at least one claim, and — the compile delta —
// EVERY claim carries at least one cite, and every cite resolves to an inbox id in
// the presented pile (no fabricated citations). Content quality (no per-event
// deltas, salience) is the prompt's job and Part II's score, not a parse error.
func validateRawCompileSubject(rs rawCompileSubject, validIDs map[string]struct{}) error {
	switch rs.Type {
	case TypeEntity, TypeEvent, TypeConcept:
	default:
		return fmt.Errorf("invalid type %q (must be entity|event|concept)", rs.Type)
	}
	if strings.TrimSpace(rs.Name) == "" {
		return fmt.Errorf("missing name")
	}
	nonEmpty := 0
	for j, c := range rs.Claims {
		if strings.TrimSpace(c.Text) == "" {
			continue
		}
		nonEmpty++
		cited := cleanCites(c.Cites)
		if len(cited) == 0 {
			return fmt.Errorf("claim %d has no citation (every digest claim cites its events — §5)", j)
		}
		for _, id := range cited {
			if _, ok := validIDs[id]; !ok {
				return fmt.Errorf("claim %d cites %q which is not in the event pile (fabricated citation)", j, id)
			}
		}
	}
	if nonEmpty == 0 {
		return fmt.Errorf("subject %q has no claims (claim-bearing salience gate)", rs.Name)
	}
	return nil
}
