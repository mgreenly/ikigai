// Package read is wiki's read side (design §9): the public search and timeline
// verbs (zero-LLM) and the hosted-ask agent (synchronous, strictly read-only).
// It writes nothing to the knowledge layer (design §9.1) — ask runs fully
// parallel with integration, needs no flight lock, no commit, no transaction. Its
// only write is the asks accounting row (design §9.2), deliberately separate from
// runs.
//
// Ask is hosted-ask-first: the wiki runs the retrieval loop server-side as its
// own agent (config-injected triple), spending the caller exactly one tool call
// for a cited answer (context isolation as a service). The inner agent gets the
// SIX read tools — search, lookup, read_page, read_source, timeline — under a
// server-side budget; `related` is goldens-gated and NOT built (design §9.2).
package read

import (
	"encoding/json"
	"fmt"
	"strings"
)

// AnswerSchema pins ask's structured output (design §9.2 answer contract): the
// prose answer, page-level citations (subject id + title), the optional inbox-id
// sources (only when read_source was used), and the found flag.
var AnswerSchema = json.RawMessage(`{
  "type": "object",
  "additionalProperties": false,
  "required": ["answer", "citations", "found"],
  "properties": {
    "answer": {"type": "string"},
    "citations": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties": false,
        "required": ["subject", "title"],
        "properties": {
          "subject": {"type": "string"},
          "title": {"type": "string"}
        }
      }
    },
    "sources": {"type": "array", "items": {"type": "string"}},
    "found": {"type": "boolean"}
  }
}`)

// Citation is one page-level citation (design §9.2): subject id + page title, both
// followable by the caller (fetch the cited page via the public search verb).
type Citation struct {
	Subject string `json:"subject"`
	Title   string `json:"title"`
}

// Answer is ask's parsed result (design §9.2). Answer carries the prose, the
// page-level citations, the optional inbox-id sources, and the found flag.
type Answer struct {
	Answer    string     `json:"answer"`
	Citations []Citation `json:"citations"`
	Sources   []string   `json:"sources"`
	Found     bool       `json:"found"`
}

// ParseAnswer parses + validates an ask response into an Answer. Separated from
// the call so the prompt-default gate + goldens exercise the parser offline
// against a committed fixture, with no client (the standing prompt gate / eval
// obligation 5). It enforces the design's structural contract:
//
//   - a non-empty answer is required (an empty answer is never valid output);
//   - found=true requires at least one citation (a grounded answer cites a page);
//   - found=false requires zero citations (the honest "wiki has nothing" shape);
//   - every citation carries both a subject id and a title (followable).
func ParseAnswer(raw string) (Answer, error) {
	var a Answer
	if err := json.Unmarshal([]byte(extractJSONObject(stripCodeFence(raw))), &a); err != nil {
		return Answer{}, fmt.Errorf("read: parse ask answer: %w", err)
	}
	a.Answer = strings.TrimSpace(a.Answer)
	if a.Answer == "" {
		return Answer{}, fmt.Errorf("read: ask answer is empty")
	}
	cites := make([]Citation, 0, len(a.Citations))
	for _, c := range a.Citations {
		c.Subject = strings.TrimSpace(c.Subject)
		c.Title = strings.TrimSpace(c.Title)
		if c.Subject == "" {
			return Answer{}, fmt.Errorf("read: ask citation missing subject id")
		}
		cites = append(cites, c)
	}
	a.Citations = cites
	a.Sources = cleanList(a.Sources)
	if a.Found && len(a.Citations) == 0 {
		return Answer{}, fmt.Errorf("read: ask found=true but cited no page (page-level citation contract §9.2)")
	}
	if !a.Found && len(a.Citations) != 0 {
		return Answer{}, fmt.Errorf("read: ask found=false but carried citations (the not-found shape cites nothing)")
	}
	return a, nil
}

// cleanList trims and drops empty entries; nil if empty.
func cleanList(in []string) []string {
	out := make([]string, 0, len(in))
	for _, s := range in {
		if t := strings.TrimSpace(s); t != "" {
			out = append(out, t)
		}
	}
	if len(out) == 0 {
		return nil
	}
	return out
}

// extractJSONObject returns the first balanced top-level JSON object embedded in
// s. A real model often prefixes the JSON with prose ("Based on the wiki, …
// {…}"); the structured-output contract is the object, so we locate it by
// brace-matching (respecting string literals + escapes) rather than assuming the
// whole reply is JSON. If no balanced object is found, s is returned unchanged so
// json.Unmarshal surfaces the original parse error.
func extractJSONObject(s string) string {
	start := strings.IndexByte(s, '{')
	if start < 0 {
		return s
	}
	depth := 0
	inStr := false
	esc := false
	for i := start; i < len(s); i++ {
		c := s[i]
		if inStr {
			switch {
			case esc:
				esc = false
			case c == '\\':
				esc = true
			case c == '"':
				inStr = false
			}
			continue
		}
		switch c {
		case '"':
			inStr = true
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				return s[start : i+1]
			}
		}
	}
	return s
}

// stripCodeFence removes a leading/trailing Markdown code fence a model may wrap
// JSON in (the Anthropic backend has no native structured-output field — a fenced
// reply is a normal, recoverable shape, not an error).
func stripCodeFence(raw string) string {
	s := strings.TrimSpace(raw)
	if !strings.HasPrefix(s, "```") {
		return s
	}
	if i := strings.IndexByte(s, '\n'); i >= 0 {
		s = s[i+1:]
	}
	if k := strings.LastIndex(s, "```"); k >= 0 {
		s = s[:k]
	}
	return strings.TrimSpace(s)
}
