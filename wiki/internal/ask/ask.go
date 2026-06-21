// Package ask answers questions from retrieved wiki context.
package ask

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"strings"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/llm"
	"wiki/internal/retrieve"
	"wiki/internal/wiki"
)

const honestEmptyText = "The wiki holds nothing on that question."

// Answer is a generated answer and the wiki pages it cites.
type Answer struct {
	Found     bool
	Text      string
	Citations []Citation
	Sources   []string
}

// Citation identifies a wiki page the answer drew on.
type Citation struct {
	Subject string
	Title   string
}

// SourceReader reads original source text by job id.
type SourceReader interface {
	ReadSource(ctx context.Context, owner, jobID string) (string, error)
}

// Asker is the read-only grounded question-answering service.
type Asker struct {
	rs            *retrieve.Service
	pages         *wiki.PageStore
	src           SourceReader
	c             *llm.Client
	site          llm.CallSite
	maxIter       int
	readSourceCap int
}

// New creates an Asker from the injected retrieval, page, source, and LLM seams.
func New(rs *retrieve.Service, pages *wiki.PageStore, src SourceReader, c *llm.Client, site llm.CallSite, maxIter, readSourceCap int) *Asker {
	return &Asker{
		rs:            rs,
		pages:         pages,
		src:           src,
		c:             c,
		site:          site,
		maxIter:       maxIter,
		readSourceCap: readSourceCap,
	}
}

// Ask answers a question using only retrieved wiki context and read-only tools.
func (a *Asker) Ask(ctx context.Context, owner, question string) (Answer, error) {
	if a == nil || a.rs == nil {
		return Answer{}, fmt.Errorf("ask: nil retriever")
	}
	hits, err := a.rs.Search(ctx, question, 8)
	if err != nil {
		return Answer{}, err
	}
	if len(hits) == 0 {
		return honestEmpty(), nil
	}
	if a.c == nil {
		return Answer{}, fmt.Errorf("ask: nil llm client")
	}

	ctxs := a.contexts(ctx, hits)
	readSources := make(map[string]struct{})
	conv := a.c.Converse(a.site, a.tools(owner, readSources))
	if a.maxIter > 0 {
		conv.MaxToolIterations = a.maxIter
	}
	stream := conv.Send(ctx, askPrompt(question, ctxs))

	var final string
	for ev := range stream.Events() {
		if done, ok := ev.(agentkit.MessageDone); ok {
			final = messageText(done.Message)
		}
	}
	if err := stream.Err(); err != nil {
		return Answer{}, err
	}

	ans, err := parseAnswer(final)
	if err != nil {
		return Answer{}, err
	}
	if !ans.Found {
		return honestEmpty(), nil
	}
	if err := validateCitations(ans.Citations, ctxs); err != nil {
		return Answer{}, err
	}
	ans.Sources = sortedSourceIDs(readSources)
	return ans, nil
}

func honestEmpty() Answer {
	return Answer{Found: false, Text: honestEmptyText}
}

type pageContext struct {
	Subject string `json:"subject"`
	Title   string `json:"title"`
	Body    string `json:"body"`
}

func (a *Asker) contexts(ctx context.Context, hits []retrieve.Hit) []pageContext {
	out := make([]pageContext, 0, len(hits))
	for _, hit := range hits {
		pc := pageContext{
			Subject: hit.SubjectID,
			Title:   hit.Title,
			Body:    hit.Snippet,
		}
		if a.pages != nil && hit.PageID != "" {
			if page, err := a.pages.Get(ctx, hit.PageID); err == nil {
				pc.Subject = page.SubjectID
				pc.Title = page.Title
				pc.Body = page.Body
			} else if err != sql.ErrNoRows {
				pc.Body = hit.Snippet
			}
		}
		out = append(out, pc)
	}
	return out
}

func askPrompt(question string, ctxs []pageContext) string {
	raw, _ := json.Marshal(ctxs)
	return "Answer the question using only the supplied wiki context. " +
		"Return only JSON with found, text, and citations. " +
		"Each citation must use an exact subject and title from the context. " +
		"If the context does not answer the question, return found=false.\n\n" +
		"Question: " + question + "\n\nContext: " + string(raw)
}

type readSourceInput struct {
	JobID string `json:"job_id"`
}

func (a *Asker) tools(owner string, readSources map[string]struct{}) []agentkit.Tool {
	if a.src == nil {
		return nil
	}
	return []agentkit.Tool{
		agentkit.NewTool("read_source", "Read original source text for a cited wiki job id.", func(ctx context.Context, in readSourceInput) (string, error) {
			jobID := strings.TrimSpace(in.JobID)
			if jobID == "" {
				return "", fmt.Errorf("job_id is required")
			}
			if a.readSourceCap > 0 && len(readSources) >= a.readSourceCap {
				if _, ok := readSources[jobID]; !ok {
					return "", fmt.Errorf("read_source cap exceeded")
				}
			}
			text, err := a.src.ReadSource(ctx, owner, jobID)
			if err != nil {
				return "", err
			}
			readSources[jobID] = struct{}{}
			return text, nil
		}),
	}
}

type answerJSON struct {
	Found     bool       `json:"found"`
	Text      string     `json:"text"`
	Citations []Citation `json:"citations"`
}

func parseAnswer(text string) (Answer, error) {
	var in answerJSON
	if err := json.Unmarshal([]byte(llm.ExtractJSON(text)), &in); err != nil {
		return Answer{}, fmt.Errorf("ask: parse answer JSON: %w", err)
	}
	return Answer{
		Found:     in.Found,
		Text:      strings.TrimSpace(in.Text),
		Citations: in.Citations,
	}, nil
}

func validateCitations(citations []Citation, ctxs []pageContext) error {
	if len(citations) == 0 {
		return fmt.Errorf("ask: found answer requires at least one citation")
	}
	allowed := make(map[Citation]struct{}, len(ctxs))
	for _, ctx := range ctxs {
		allowed[Citation{Subject: ctx.Subject, Title: ctx.Title}] = struct{}{}
	}
	for _, citation := range citations {
		if _, ok := allowed[citation]; !ok {
			return fmt.Errorf("ask: citation not in retrieved context: %+v", citation)
		}
	}
	return nil
}

func messageText(message agentkit.Message) string {
	var b strings.Builder
	for _, block := range message.Blocks {
		if text, ok := block.(agentkit.TextBlock); ok {
			b.WriteString(text.Text)
		}
	}
	return b.String()
}

func sortedSourceIDs(sources map[string]struct{}) []string {
	out := make([]string, 0, len(sources))
	for source := range sources {
		out = append(out, source)
	}
	for i := 1; i < len(out); i++ {
		for j := i; j > 0 && out[j] < out[j-1]; j-- {
			out[j], out[j-1] = out[j-1], out[j]
		}
	}
	return out
}
