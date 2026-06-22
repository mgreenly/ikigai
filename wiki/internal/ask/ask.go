// Package ask answers questions from exact subject pages.
package ask

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"strings"

	"wiki/internal/llm"
	"wiki/internal/wiki"
)

const honestEmptyText = "The wiki holds nothing on that question."

// Answer is a generated answer and the wiki pages it cites.
type Answer struct {
	Found     bool
	Text      string
	Citations []Citation
}

// Citation identifies a wiki page the answer drew on.
type Citation struct {
	Path  string
	Title string
}

// Asker is the read-only subject-extraction question-answering service.
type Asker struct {
	subjects    *wiki.SubjectStore
	pages       *wiki.PageStore
	c           *llm.Client
	extractSite llm.CallSite
	synthSite   llm.CallSite
}

// New creates an Asker from the injected subject/page stores and LLM seams.
func New(subjects *wiki.SubjectStore, pages *wiki.PageStore, c *llm.Client, extractSite, synthSite llm.CallSite) *Asker {
	return &Asker{
		subjects:    subjects,
		pages:       pages,
		c:           c,
		extractSite: extractSite,
		synthSite:   synthSite,
	}
}

// Ask answers a question by extracting subject names, resolving exact subjects,
// reading their pages, and synthesizing only from those page bodies.
func (a *Asker) Ask(ctx context.Context, owner, question string) (Answer, error) {
	_ = owner
	if a == nil || a.subjects == nil || a.pages == nil {
		return Answer{}, fmt.Errorf("ask: nil stores")
	}
	if a.c == nil {
		return Answer{}, fmt.Errorf("ask: nil llm client")
	}

	extracted, err := llm.JSON[extractResult](ctx, a.c, a.extractSite, extractPrompt(question), nil)
	if err != nil {
		return Answer{}, err
	}

	pages, err := a.gatherPages(ctx, extracted.Subjects)
	if err != nil {
		return Answer{}, err
	}
	if len(pages) == 0 {
		return honestEmpty(), nil
	}

	var citations []Citation
	result, err := llm.JSON[answerResult](ctx, a.c, a.synthSite, synthPrompt(question, pages), func(out *answerResult) error {
		normalizeAnswer(out)
		if !out.Found {
			return nil
		}
		var err error
		citations, err = validateCitations(out.Citations, pages)
		return err
	})
	if err != nil {
		return Answer{}, err
	}
	normalizeAnswer(&result)
	if !result.Found {
		return honestEmpty(), nil
	}
	return Answer{Found: true, Text: result.Text, Citations: citations}, nil
}

func honestEmpty() Answer {
	return Answer{Found: false, Text: honestEmptyText}
}

type extractResult struct {
	Subjects []string `json:"subjects"`
}

type answerResult struct {
	Found     bool             `json:"found"`
	Text      string           `json:"text"`
	Citations []answerCitation `json:"citations"`
}

type answerCitation struct {
	Subject string `json:"subject"`
	Title   string `json:"title"`
}

type pageContext struct {
	Subject string `json:"subject"`
	Path    string `json:"-"`
	Title   string `json:"title"`
	Body    string `json:"body"`
}

func (a *Asker) gatherPages(ctx context.Context, names []string) ([]pageContext, error) {
	seenNames := map[string]struct{}{}
	seenSubjects := map[string]struct{}{}
	out := make([]pageContext, 0, len(names))
	for _, raw := range names {
		name := strings.TrimSpace(raw)
		if name == "" {
			continue
		}
		key := strings.ToLower(name)
		if _, ok := seenNames[key]; ok {
			continue
		}
		seenNames[key] = struct{}{}

		subject, err := a.subjects.GetByNormName(ctx, name)
		if err == sql.ErrNoRows {
			continue
		}
		if err != nil {
			return nil, err
		}
		if _, ok := seenSubjects[subject.ID]; ok {
			continue
		}

		page, err := a.pages.GetBySubject(ctx, subject.ID)
		if err == sql.ErrNoRows {
			continue
		}
		if err != nil {
			return nil, err
		}
		seenSubjects[subject.ID] = struct{}{}
		out = append(out, pageContext{
			Subject: subject.ID,
			Path:    wiki.Path(subject),
			Title:   page.Title,
			Body:    page.Body,
		})
	}
	return out, nil
}

func extractPrompt(question string) string {
	return "Extract the subject names explicitly named in the question. " +
		"Return only JSON with a subjects array of strings.\n\nQuestion: " + question
}

func synthPrompt(question string, pages []pageContext) string {
	raw, _ := json.Marshal(pages)
	return "Answer the question using only the supplied wiki pages. " +
		"Return only JSON with found, text, and citations. " +
		"Each citation must use an exact subject and title from the pages. " +
		"If the pages do not answer the question, return found=false.\n\n" +
		"Question: " + question + "\n\nPages: " + string(raw)
}

func normalizeAnswer(out *answerResult) {
	if out == nil {
		return
	}
	out.Text = strings.TrimSpace(out.Text)
	for i := range out.Citations {
		out.Citations[i].Subject = strings.TrimSpace(out.Citations[i].Subject)
		out.Citations[i].Title = strings.TrimSpace(out.Citations[i].Title)
	}
}

func validateCitations(citations []answerCitation, pages []pageContext) ([]Citation, error) {
	if len(citations) == 0 {
		return nil, fmt.Errorf("ask: found answer requires at least one citation")
	}
	allowed := make(map[answerCitation]Citation, len(pages))
	for _, page := range pages {
		allowed[answerCitation{Subject: page.Subject, Title: page.Title}] = Citation{Path: page.Path, Title: page.Title}
	}
	out := make([]Citation, 0, len(citations))
	for _, citation := range citations {
		mapped, ok := allowed[citation]
		if !ok {
			return nil, fmt.Errorf("ask: citation not in gathered pages: %+v", citation)
		}
		out = append(out, mapped)
	}
	return out, nil
}
