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
	c    *llm.Client
	site llm.CallSite
}

// New builds an Extractor from an injected LLM client and extract call site.
func New(c *llm.Client, site llm.CallSite) *Extractor {
	return &Extractor{c: c, site: site}
}

// DefaultCallSite returns the production extract-stage generation settings.
func DefaultCallSite(model string) llm.CallSite {
	temp := 0.0
	return llm.CallSite{
		Stage:           "extract",
		Model:           model,
		Temperature:     &temp,
		Reasoning:       llm.DisableReasoning(),
		MaxParseRetries: 2,
	}
}

// Extract extracts subjects and claims from source text.
func (e *Extractor) Extract(ctx context.Context, h DocumentHeader, text string) ([]ExtractedSubject, error) {
	if e == nil {
		return nil, fmt.Errorf("extract: nil extractor")
	}
	out, err := llm.JSON[extractResponse](ctx, e.c, e.site, renderPrompt(h, text), validateResponse)
	if err != nil {
		return nil, err
	}
	return out.Subjects, nil
}

type extractResponse struct {
	Subjects []ExtractedSubject `json:"subjects"`
}

func renderPrompt(h DocumentHeader, text string) string {
	var b strings.Builder
	b.WriteString("Extract subjects and claims from the source text.\n")
	b.WriteString("Return JSON with shape {\"subjects\":[{\"type\":\"entity|event|concept\",\"kind\":\"...\",\"name\":\"...\",\"occurred_at\":\"...\",\"claims\":[\"...\"]}]}.\n")
	b.WriteString("Use only the source text and document header; do not infer unstated facts.\n")
	b.WriteString("occurred_at is required for events, optional for entities and concepts, and must be an ISO-8601 prefix (YYYY, YYYY-MM, or YYYY-MM-DD) when present.\n")
	b.WriteString("Claims must be short, self-contained prose statements with no pronouns.\n\n")
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
