// Package compile builds durable wiki pages from canonical subjects and claims.
package compile

import (
	"context"
	"fmt"
	"log/slog"
	"strings"
	"unicode/utf8"

	"wiki/internal/llm"
	"wiki/internal/wiki"
)

// PageCharCap is the maximum generated page body length in Unicode code points.
const PageCharCap = 12000

// Compiler rebuilds wiki pages from subject identity and complete claim sets.
type Compiler struct {
	c          *llm.Client
	site       llm.CallSite
	maxTighten int
	log        *slog.Logger
}

// New builds a Compiler from an injected LLM client and compile call site.
func New(c *llm.Client, site llm.CallSite, log *slog.Logger) *Compiler {
	return &Compiler{c: c, site: site, maxTighten: 2, log: log}
}

// Compile rebuilds one subject's page from its complete claim set.
func (c *Compiler) Compile(ctx context.Context, s wiki.Subject, claims []wiki.Claim) (title, body string, err error) {
	if c == nil {
		return "", "", fmt.Errorf("compile: nil compiler")
	}

	maxTighten := c.maxTighten
	if maxTighten < 0 {
		maxTighten = 0
	}

	prompt := renderPrompt(s, claims, PageCharCap, "")
	var last compileResponse
	for attempt := 0; attempt <= maxTighten; attempt++ {
		out, err := llm.JSON[compileResponse](ctx, c.c, c.site, prompt, validateResponse)
		if err != nil {
			return "", "", err
		}

		out.Title = strings.TrimSpace(out.Title)
		out.Body = strings.TrimSpace(out.Body)
		last = out
		if utf8.RuneCountInString(out.Body) <= PageCharCap {
			return out.Title, out.Body, nil
		}

		prompt = renderPrompt(s, claims, PageCharCap, fmt.Sprintf(
			"The previous body was %d characters; recompile it to fit the %d-character cap.",
			utf8.RuneCountInString(out.Body), PageCharCap,
		))
	}

	if c.log != nil {
		c.log.WarnContext(ctx, "compile body exceeded page character cap after tightening",
			"subject_id", s.ID,
			"body_chars", utf8.RuneCountInString(last.Body),
			"cap", PageCharCap,
		)
	}
	return last.Title, truncateRunes(last.Body, PageCharCap), nil
}

type compileResponse struct {
	Title string `json:"title"`
	Body  string `json:"body"`
}

func validateResponse(r *compileResponse) error {
	if r == nil {
		return fmt.Errorf("response required")
	}
	if strings.TrimSpace(r.Title) == "" {
		return fmt.Errorf("title required")
	}
	if strings.TrimSpace(r.Body) == "" {
		return fmt.Errorf("body required")
	}
	return nil
}

func renderPrompt(s wiki.Subject, claims []wiki.Claim, cap int, tighten string) string {
	var b strings.Builder
	b.WriteString("Compile one wiki page from the subject identity and complete claim set below.\n")
	b.WriteString("Use only the subject identity and claims; do not use previous pages, prior page bodies, source documents, or unstated facts.\n")
	b.WriteString("Return only JSON with shape {\"title\":\"...\",\"body\":\"...\"}.\n")
	fmt.Fprintf(&b, "The body must be no more than %d Unicode code points.\n", cap)
	if strings.TrimSpace(tighten) != "" {
		b.WriteString(tighten)
		b.WriteByte('\n')
	}

	b.WriteString("\nSubject identity:\n")
	writePromptLine(&b, "id", s.ID)
	writePromptLine(&b, "name", s.Name)
	writePromptLine(&b, "norm_name", s.NormName)
	writePromptLine(&b, "type", s.Type)

	b.WriteString("\nComplete claims:\n")
	if len(claims) == 0 {
		b.WriteString("- none\n")
		return b.String()
	}
	for i, claim := range claims {
		fmt.Fprintf(&b, "%d. [%s] %s\n", i+1, claim.ID, strings.TrimSpace(claim.Body))
	}
	return b.String()
}

func writePromptLine(b *strings.Builder, key, value string) {
	b.WriteString(key)
	b.WriteString(": ")
	b.WriteString(value)
	b.WriteByte('\n')
}

func truncateRunes(s string, cap int) string {
	if cap < 0 {
		cap = 0
	}
	if utf8.RuneCountInString(s) <= cap {
		return s
	}
	runes := []rune(s)
	return string(runes[:cap])
}
