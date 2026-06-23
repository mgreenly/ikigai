// Package extract turns ingested source text into subjects and claims.
package extract

import (
	"context"
	"fmt"
	"strings"
	"time"

	"wiki/internal/llm"
)

// ExtractedSubject is one subject with short, self-contained claims from source text.
type ExtractedSubject struct {
	Type       string   `json:"type"`
	Kind       string   `json:"kind"`
	Name       string   `json:"name"`
	OccurredAt string   `json:"occurred_at"`
	Claims     []string `json:"claims"`
}

// DocumentHeader anchors model extraction to explicit source metadata.
type DocumentHeader struct {
	Source     string
	Title      string
	Tags       []string
	ReceivedAt time.Time
}

// Extractor runs the extract-stage LLM call.
type Extractor struct {
	c                  *llm.Client
	site               llm.CallSite
	promptInstructions string
}

const defaultMaxTokens = 16384

// DefaultPromptInstructions is the baked-in production extract instruction preamble.
const DefaultPromptInstructions = `Extract subjects and claims from the source text.

Return JSON with shape {"subjects":[{"type":"entity|event|concept","kind":"...","name":"...","occurred_at":"...","claims":["..."]}]}.
Use only the source text and document header; do not infer unstated facts.

Choose subjects conservatively:
- Prefer the named people, organizations, products, places, or other entities that the header or main sentences are about.
- Emit a secondary named thing only when the text gives it independent factual claims, not merely because it is a transaction, date-bearing action, facility, division, model, technology, setting, or object connected to a primary subject.
- Treat acquisitions, openings, launches, appointments, partnerships, and similar dated actions as claims about the participating entities unless the occurrence itself is named or described as an independent subject.
- Treat concepts, methods, industries, and technologies as claim wording unless the source defines or discusses the concept itself.
- Avoid duplicate subjects for the same real-world thing; use one canonical name.

Write claims as short, self-contained prose statements with no pronouns.
For each subject, include directly stated attributes, roles, relationships, dates, actions, outcomes, ownership, and affiliations that are facts about that subject.

After choosing subjects, audit each emitted subject against the source clauses:
- Include a claim when the subject is explicitly named or clearly referred to in a clause that states a fact about it, including appositive, possessive, passive, founder/creator, ownership, affiliation, or role phrases.
- A relationship may yield a claim for each emitted subject only when the source clause directly anchors that relationship to each subject, not merely because one subject is mentioned inside another subject's action.
- Preserve stable reciprocal facts for emitted subjects when directly stated, especially origin, founding, creation, authorship, ownership, affiliation, identity, and completed transactions.
- For future-tense facts involving two emitted subjects, assign the claim to the emitted subject that will act, decide, join, keep, control, appoint, retain, manage, or otherwise carry out the future action. Do not also make it a claim about the affected emitted subject unless that affected subject is itself the future actor or the clause states a present stable fact about it.
- Future facts about a facility, program, division, product, brand, or other thing that is not emitted as its own subject may be included under the owning emitted subject when the source directly states them.
- Do not add explanatory context, motives, or consequences unless the source states them as facts about that subject.

Keep predicate direction faithful to the source:
- Do not invent a causative or receiving frame for a subject by rewriting another subject's action as that subject having, getting, receiving, adding, bringing in, or being joined by someone or something.
- Use that kind of receiving or possession claim only when the source directly states the subject receives, gains, owns, appoints, hires, creates, or undergoes the change, or when a completed transaction directly changes that subject.
- If a faithful claim would require changing who performs the verb, omit it for that subject and keep the fact with the subject that the source makes the actor, holder, or changed party.

occurred_at is required for events, optional for entities and concepts, and must be an ISO-8601 prefix (YYYY, YYYY-MM, or YYYY-MM-DD) when present.
type must be one of "entity", "event", or "concept"; kind and name must be non-empty; each subject needs at least one non-empty claim.`

// Option configures an Extractor at construction.
type Option func(*Extractor)

// WithPromptInstructions replaces the instruction preamble for this Extractor.
func WithPromptInstructions(instructions string) Option {
	return func(e *Extractor) {
		e.promptInstructions = instructions
	}
}

// New builds an Extractor from an injected LLM client and extract call site.
func New(c *llm.Client, site llm.CallSite, opts ...Option) *Extractor {
	e := &Extractor{c: c, site: site, promptInstructions: DefaultPromptInstructions}
	for _, opt := range opts {
		opt(e)
	}
	return e
}

// DefaultCallSite returns the production extract-stage generation settings.
func DefaultCallSite() llm.CallSite {
	temp := 0.0
	return llm.CallSite{
		Stage:           "extract",
		Temperature:     &temp,
		Reasoning:       llm.DisableReasoning(),
		MaxTokens:       defaultMaxTokens,
		MaxParseRetries: 2,
	}
}

// Extract extracts subjects and claims from source text.
func (e *Extractor) Extract(ctx context.Context, h DocumentHeader, text string) ([]ExtractedSubject, error) {
	if e == nil {
		return nil, fmt.Errorf("extract: nil extractor")
	}
	out, err := llm.JSON[extractResponse](ctx, e.c, e.site, renderPrompt(e.promptInstructions, h, text), validateResponse)
	if err != nil {
		return nil, err
	}
	return out.Subjects, nil
}

type extractResponse struct {
	Subjects []ExtractedSubject `json:"subjects"`
}

func renderPrompt(instructions string, h DocumentHeader, text string) string {
	var b strings.Builder
	b.WriteString(instructions)
	b.WriteString("\n\n")
	b.WriteString("Document header:\n")
	writeHeaderLine(&b, "source", h.Source)
	writeHeaderLine(&b, "title", h.Title)
	writeHeaderLine(&b, "tags", strings.Join(h.Tags, ", "))
	writeHeaderLine(&b, "received on", h.ReceivedAt.Format("2006-01-02"))
	b.WriteString("\nSource text:\n")
	b.WriteString(text)
	return b.String()
}

func writeHeaderLine(b *strings.Builder, key, value string) {
	b.WriteString(key)
	b.WriteString(": ")
	b.WriteString(value)
	b.WriteByte('\n')
}

func validateResponse(r *extractResponse) error {
	if r == nil {
		return fmt.Errorf("response required")
	}
	for i := range r.Subjects {
		if err := validateSubject(i, r.Subjects[i]); err != nil {
			return err
		}
	}
	return nil
}

func validateSubject(i int, s ExtractedSubject) error {
	switch s.Type {
	case "entity", "event", "concept":
	default:
		return fmt.Errorf("subjects[%d].type must be entity, event, or concept", i)
	}
	if strings.TrimSpace(s.Kind) == "" {
		return fmt.Errorf("subjects[%d].kind required", i)
	}
	if strings.TrimSpace(s.Name) == "" {
		return fmt.Errorf("subjects[%d].name required", i)
	}
	if s.OccurredAt == "" {
		if s.Type == "event" {
			return fmt.Errorf("subjects[%d].occurred_at required for events", i)
		}
	} else if !isISOPrefix(s.OccurredAt) {
		return fmt.Errorf("subjects[%d].occurred_at must be an ISO-8601 prefix", i)
	}
	if len(s.Claims) == 0 {
		return fmt.Errorf("subjects[%d].claims required", i)
	}
	for j, claim := range s.Claims {
		if strings.TrimSpace(claim) == "" {
			return fmt.Errorf("subjects[%d].claims[%d] required", i, j)
		}
	}
	return nil
}

func isISOPrefix(v string) bool {
	switch len(v) {
	case len("2006"):
		_, err := time.Parse("2006", v)
		return err == nil
	case len("2006-01"):
		_, err := time.Parse("2006-01", v)
		return err == nil
	case len("2006-01-02"):
		_, err := time.Parse("2006-01-02", v)
		return err == nil
	default:
		return false
	}
}
