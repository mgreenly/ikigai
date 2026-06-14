package integrate

import (
	"context"
	"encoding/json"
	"errors"
	"testing"

	"agentkit/provider"

	"wiki/internal/config"
	"wiki/internal/page"
)

// digestSiteMock dispatches a canned response by call-site name so compile → match
// → merge run end-to-end with no key or network (the unit gate mocks every LLM).
type digestSiteMock struct {
	resp map[string]string
}

func (m *digestSiteMock) Structured(_ context.Context, site config.CallSite, _ json.RawMessage, _ []provider.Message) (string, error) {
	return m.resp[site.Name], nil
}

// fakeEventSource hands the digest a canned pending event pile for an entry.
type fakeEventSource struct {
	events map[string][]EventRow
	err    error
}

func (f fakeEventSource) Events(_ context.Context, entry string) ([]EventRow, error) {
	if f.err != nil {
		return nil, f.err
	}
	return f.events[entry], nil
}

func newDigest(t *testing.T, src eventSource, mock *digestSiteMock) *Digest {
	t.Helper()
	cmp := NewCompiler(mock, config.CallSite{Name: "compile", Prompt: config.DefaultCompilePrompt, Model: "claude-sonnet-4-6", Effort: "medium"})
	res := NewResolver(&fakeRegistry{}, 5) // empty registry → create
	matcher := NewMatcher(mock, &fakeExcerptReader{}, config.CallSite{Name: "match", Model: "claude-sonnet-4-6"}, 600)
	asm := NewAssembler(matcher, func() string { return "01SUBJDEAL" })
	merger := NewMerger(mock, &fakePageReader{}, mergeSite())
	return NewDigest("crm-digest", src, cmp, res, asm, merger)
}

// TestDigestIntegrateHappyPath proves the digest pass reuses the document pass's
// resolve→assemble→merge pipeline verbatim, swapping only compile for extract: a
// pile of events compiles to a subject, resolves as a create (empty registry),
// matches nothing, and merges into a prose page on the manifest — the same Manifest
// shape merge produces for the document pass.
func TestDigestIntegrateHappyPath(t *testing.T) {
	src := fakeEventSource{events: map[string][]EventRow{
		"crm-digest": {
			{ID: "01EVTA", Source: "crm:deal.stage_changed", Payload: []byte(`{"to":"negotiation"}`)},
			{ID: "01EVTB", Source: "crm:deal.stage_changed", Payload: []byte(`{"to":"closed_won"}`)},
		},
	}}
	mock := &digestSiteMock{resp: map[string]string{
		"compile": `{"subjects":[{"type":"event","kind":"deal","name":"Globex renewal closed","aliases":[],` +
			`"occurred_at":"2024-05-23","claims":[{"text":"The Globex renewal closed on 2024-05-23.","cites":["01EVTA","01EVTB"]}]}]}`,
		"merge": `{"pages":[{"subject":"01SUBJDEAL","title":"Globex renewal closed",` +
			`"body":"The Globex renewal closed on 2024-05-23. [01EVTA] [01EVTB]","superseded":[]}]}`,
	}}
	d := newDigest(t, src, mock)

	if d.Job() != "crm-digest" {
		t.Fatalf("Job() = %q, want crm-digest", d.Job())
	}

	m, err := d.Integrate(context.Background(), Unit{CausedBy: "01CRONROW", Entry: "crm-digest"})
	if err != nil {
		t.Fatalf("Integrate: %v", err)
	}
	if len(m.Subjects) != 1 {
		t.Fatalf("subjects = %d, want 1", len(m.Subjects))
	}
	s := m.Subjects[0]
	if s.SubjectID != "01SUBJDEAL" || s.TargetPage != "01SUBJDEAL" {
		t.Fatalf("resolution annotations wrong: %+v", s)
	}
	if s.OccurredAt != "2024-05-23" {
		t.Errorf("occurred_at = %q, want 2024-05-23 (resolved in compile)", s.OccurredAt)
	}
	// Per-claim cites survived compile → manifest (obligation 3).
	if len(s.Claims) != 1 || len(s.Claims[0].Cites) != 2 {
		t.Errorf("per-claim cites lost: %+v", s.Claims)
	}
	if s.PageBody == "" {
		t.Error("merge did not produce a page body on the manifest")
	}
}

// TestDigestEmptyPileIsCleanNoOp proves an entry with no pending events yields an
// empty manifest (a clean no-op commit), never an error — the cron completion-time
// join still stamps the cron row.
func TestDigestEmptyPileIsCleanNoOp(t *testing.T) {
	src := fakeEventSource{events: map[string][]EventRow{}}
	d := newDigest(t, src, &digestSiteMock{resp: map[string]string{}})
	m, err := d.Integrate(context.Background(), Unit{CausedBy: "01CRONROW", Entry: "crm-digest"})
	if err != nil {
		t.Fatalf("empty pile should be a clean no-op: %v", err)
	}
	if len(m.Subjects) != 0 || len(m.DupPairs) != 0 {
		t.Fatalf("empty pile must yield an empty manifest: %+v", m)
	}
}

// TestDigestEventSourceError surfaces a source failure as a clean integrate error
// (the run fails cleanly; the cron row stays pending — the retry authorization).
func TestDigestEventSourceError(t *testing.T) {
	src := fakeEventSource{err: errors.New("boom")}
	d := newDigest(t, src, &digestSiteMock{resp: map[string]string{}})
	if _, err := d.Integrate(context.Background(), Unit{CausedBy: "01CRONROW", Entry: "crm-digest"}); err == nil {
		t.Fatal("event-source error must surface as a clean integrate failure")
	}
}

// TestDigestImplementsIntegrator proves the digest satisfies the SAME P4 Integrator
// interface (so merge cannot tell which integrator ran) plus the conflict-loop
// capabilities (ReMerger/ReResolver), exactly as the document pass does.
func TestDigestImplementsIntegrator(t *testing.T) {
	var d *Digest
	var _ Integrator = d
	var _ interface {
		ReMerge(context.Context, *Manifest, string) error
	} = d
	var _ interface {
		ReResolve(context.Context, *Manifest, string) error
	} = d
	_ = page.Candidate{} // keep the page import meaningful for future candidate tests
}
