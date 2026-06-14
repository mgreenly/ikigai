package run

import (
	"context"
	"encoding/json"
	"testing"
	"time"

	"agentkit/provider"

	"wiki/internal/config"
	"wiki/internal/integrate"
	"wiki/internal/page"
)

// siteMockCaller is the unit gate's mocked LLM for the full document pass: it
// dispatches a canned response by call-site name, so extract → match → merge run
// end-to-end with no key or network (the unit gate mocks every LLM from P6a on).
type siteMockCaller struct {
	resp map[string]string
}

func (m *siteMockCaller) Structured(_ context.Context, site config.CallSite, _ json.RawMessage, _ []provider.Message) (string, error) {
	return m.resp[site.Name], nil
}

// fakeDocSource hands the document integrator a canned causing row + payload.
type fakeDocSource struct {
	row     integrate.DocumentRow
	payload []byte
}

func (f fakeDocSource) Document(_ context.Context, _ string) (integrate.DocumentRow, []byte, error) {
	return f.row, f.payload, nil
}

func site(name string) config.CallSite {
	return config.CallSite{Name: name, Prompt: "placeholder", Model: "claude-sonnet-4-6", Effort: ""}
}

// TestDocumentPassHappyPathE2E drives the full ingest_text → pages happy path
// through the real spine (worker end-of-run transaction) with mocked LLMs: a
// brand-new subject is extracted, resolved as a create (empty registry), merged
// into a prose page, and committed. It asserts every P7a deliverable: the page
// row + FTS sync, the registry subject + alias, the per-page base version slot,
// and the provenance chain (page body cites the inbox id).
func TestDocumentPassHappyPathE2E(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "01HXDOC", "document", "mcp:x")

	caller := &siteMockCaller{resp: map[string]string{
		"extract": `{"subjects":[{"type":"entity","kind":"org","name":"Acme","aliases":["acme corp"],` +
			`"claims":[{"text":"Acme builds rockets."}]}]}`,
		// No candidates in an empty registry → create (match never called). The merge
		// response names the deterministic minted subject id below.
		"merge": `{"pages":[{"subject":"01SUBJACME","title":"Acme",` +
			`"body":"Acme builds rockets. [01HXDOC]"}]}`,
	}}

	store := page.NewStore(conn)
	ex := integrate.NewExtractor(caller, site("extract"))
	res := integrate.NewResolver(store, 5)
	matcher := integrate.NewMatcher(caller, store, site("match"), 600)
	// Deterministic minted id so we can predict the page subject.
	asm := integrate.NewAssembler(matcher, func() string { return "01SUBJACME" })
	merger := integrate.NewMerger(caller, store, site("merge"))

	doc := integrate.NewDocument(fakeDocSource{
		row: integrate.DocumentRow{
			ID: "01HXDOC", Source: "mcp", Title: "Acme memo",
			ReceivedAt: time.Date(2024, 1, 2, 0, 0, 0, 0, time.UTC),
		},
		payload: []byte("Acme builds rockets."),
	}, ex, res, asm, merger)

	// Run the integrator and commit through the real end-of-run transaction.
	runID, err := s.Begin(context.Background(), doc.Job(), "01HXDOC")
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	m, err := doc.Integrate(context.Background(), integrate.Unit{CausedBy: "01HXDOC"})
	if err != nil {
		t.Fatalf("integrate: %v", err)
	}
	// The per-page base version slot is populated (the value P7b's guard consumes).
	if m.Subjects[0].BaseVersion != 0 {
		t.Fatalf("BaseVersion = %d, want 0 for a new page", m.Subjects[0].BaseVersion)
	}
	if err := s.Commit(context.Background(), runID, "01HXDOC", m, true); err != nil {
		t.Fatalf("commit: %v", err)
	}

	// The page row exists with the merged body and cites the inbox id (provenance).
	var body string
	if err := conn.QueryRow(`SELECT body FROM pages WHERE subject='01SUBJACME'`).Scan(&body); err != nil {
		t.Fatalf("page row: %v", err)
	}
	if body != "Acme builds rockets. [01HXDOC]" {
		t.Fatalf("page body = %q", body)
	}

	// The registry subject + alias exist.
	var canonical string
	if err := conn.QueryRow(`SELECT canonical_name FROM subjects WHERE id='01SUBJACME'`).Scan(&canonical); err != nil {
		t.Fatalf("subject row: %v", err)
	}
	var nAlias int
	conn.QueryRow(`SELECT COUNT(1) FROM aliases WHERE subject_id='01SUBJACME'`).Scan(&nAlias)
	if nAlias != 2 { // normalize("Acme") + normalize("acme corp")
		t.Fatalf("alias count = %d, want 2", nAlias)
	}

	// pages_fts is consistent: a MATCH over the new page returns it.
	var nFTS int
	conn.QueryRow(`SELECT COUNT(1) FROM pages_fts WHERE body MATCH 'rockets'`).Scan(&nFTS)
	if nFTS != 1 {
		t.Fatalf("fts match = %d, want 1 (page not synced)", nFTS)
	}

	// Provenance chain: the inbox row is stamped by the succeeding run.
	if got := stampOf(t, conn, "01HXDOC"); got != runID {
		t.Fatalf("inbox stamp = %q, want %q", got, runID)
	}
	if got := statusOf(t, conn, runID); got != StatusSucceeded {
		t.Fatalf("run status = %q, want succeeded", got)
	}
}

// TestCommitWritesDupFlagsAndStaleNotes proves the end-of-run transaction folds
// the manifest's dup_pairs into dup_flags (via FlagDup, canonical order) and writes
// the manifest's stale_notes — both in the one commit.
func TestCommitWritesDupFlagsAndStaleNotes(t *testing.T) {
	s, conn := newStore(t)
	insertInbox(t, conn, "01HXDOC", "document", "mcp:x")
	runID, _ := s.Begin(context.Background(), "document-pass", "01HXDOC")

	m := &integrate.Manifest{
		Subjects: []integrate.Subject{{
			Type: integrate.TypeEntity, Name: "Acme", Aliases: []string{"acme"},
			SubjectID: "01SUBJA", TargetPage: "01SUBJA",
			PageTitle: "Acme", PageBody: "Acme builds rockets. [01HXDOC]",
			Claims: []integrate.Claim{{Text: "c", Cites: []string{"01HXDOC"}}},
		}},
		DupPairs:   []integrate.DupPair{{SubjectA: "01ZZZ", SubjectB: "01AAA"}}, // mis-ordered on purpose
		StaleNotes: []integrate.StaleNote{{Subject: "01NB", Note: "stale", Cites: []string{"01HXDOC"}}},
	}
	if err := s.Commit(context.Background(), runID, "01HXDOC", m, true); err != nil {
		t.Fatalf("commit: %v", err)
	}

	var a, b string
	if err := conn.QueryRow(`SELECT subject_a, subject_b FROM dup_flags`).Scan(&a, &b); err != nil {
		t.Fatalf("dup_flags: %v", err)
	}
	if a != "01AAA" || b != "01ZZZ" {
		t.Fatalf("dup pair = (%q,%q), want canonical (01AAA,01ZZZ)", a, b)
	}

	var subject, runRef, status string
	if err := conn.QueryRow(
		`SELECT subject, run_id, status FROM stale_notes`,
	).Scan(&subject, &runRef, &status); err != nil {
		t.Fatalf("stale_notes: %v", err)
	}
	if subject != "01NB" || runRef != runID || status != "open" {
		t.Fatalf("stale note = (%q,%q,%q)", subject, runRef, status)
	}
}
