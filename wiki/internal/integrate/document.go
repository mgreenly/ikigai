package integrate

import (
	"context"
	"fmt"
	"time"
)

// Document is the REAL document-pass Integrator (P7a): it replaces the P4 Stub
// with the same Integrator interface, emitting the same Manifest, so the spine is
// unchanged (the swap is mechanical, exactly as P4 set up and tested). It runs the
// full document pass — extract → resolve → assemble (the manifest) → merge —
// against the causing inbox row, then hands the populated Manifest to the
// worker's end-of-run transaction (internal/run), which atomically writes pages,
// registry inserts, dup_flags, stale_notes, and the pages_fts sync.
//
// Document does NOT touch the DB for the terminal write (the Integrator contract):
// it produces the Manifest; the caller's end-of-run transaction owns the commit.
// The base pages.version slot is populated by Merge at merge-read time (design §3
// "the version merge read"), so it is present for P7b's optimistic-commit guard.
type Document struct {
	src    documentSource
	ex     *Extractor
	res    *Resolver
	asm    *Assembler
	merger *Merger
}

// DocumentRow is the causing inbox row the document pass reads: the header fields
// extract frames its context on, plus the payload-locating fields the source uses
// to fetch the bytes. The integrator never inspects the inbox threshold — it asks
// the source for the bytes.
type DocumentRow struct {
	ID         string
	Source     string
	Title      string
	Tags       []string
	ReceivedAt time.Time
}

// documentSource fetches the causing inbox row's header + payload bytes for a
// document pass. Declared as an interface so the integrator is unit-testable
// without a live inbox store; the composition root wires a real inbox-backed
// implementation.
type documentSource interface {
	// Document returns the causing row's header and its decoded payload bytes.
	Document(ctx context.Context, inboxID string) (DocumentRow, []byte, error)
}

// NewDocument builds the real document-pass integrator from its stages. All stages
// are injected (each over its config-injected triple) so the document pass is the
// composition of the per-site functions the eval harness scores — never a fork.
func NewDocument(src documentSource, ex *Extractor, res *Resolver, asm *Assembler, merger *Merger) *Document {
	return &Document{src: src, ex: ex, res: res, asm: asm, merger: merger}
}

// Job is the document-pass job name (runs.job; the TryLock key for a document row
// is the row id, but the job name is the stable provenance label — design §4.5).
func (d *Document) Job() string { return "document-pass" }

// Integrate runs the document pass for one claimed inbox row and returns its
// Manifest. It performs NO terminal write — the worker's end-of-run transaction
// commits the Manifest atomically. An error means the run failed cleanly (the
// transaction is never committed; the causing row stays pending — design §4.5).
func (d *Document) Integrate(ctx context.Context, unit Unit) (*Manifest, error) {
	row, payload, err := d.src.Document(ctx, unit.CausedBy)
	if err != nil {
		return nil, fmt.Errorf("document: load row %q: %w", unit.CausedBy, err)
	}

	header := DocumentHeader{
		Source:     row.Source,
		Title:      row.Title,
		Tags:       row.Tags,
		ReceivedAt: row.ReceivedAt,
	}

	// extract → []Subject (extracted fields, claims cited to the one causing row).
	subjects, err := d.ex.Extract(ctx, header, string(payload), unit.CausedBy)
	if err != nil {
		return nil, fmt.Errorf("document: extract: %w", err)
	}

	// resolve → per-subject mechanical resolution outcome (zero LLM).
	resolutions, err := d.res.Resolve(ctx, subjects)
	if err != nil {
		return nil, fmt.Errorf("document: resolve: %w", err)
	}

	// assemble → the Manifest (match runs on shortlists; dup_pairs folded in).
	manifest, err := d.asm.Assemble(ctx, resolutions)
	if err != nil {
		return nil, fmt.Errorf("document: assemble: %w", err)
	}

	// merge → fold claims into prose pages; record base versions + superseded +
	// stale notes onto the manifest (the end-of-run transaction writes them).
	if _, err := d.merger.Merge(ctx, manifest); err != nil {
		return nil, fmt.Errorf("document: merge: %w", err)
	}

	return manifest, nil
}

// ReMerge re-runs the MERGE STAGE ONLY for an existing manifest after an
// optimistic-commit conflict (design §3: "re-run merge only for that page" — the
// lost-update arm, P7b). Extract and resolve outputs cannot go stale — claims and
// identities don't change because another run committed a page; only the prose
// does — so the conflict loop re-enters merge alone, never extract/resolve. Merge
// re-reads each target page's CURRENT version into the manifest's per-page
// BaseVersion slot (so the next commit's version guard checks against the fresh
// value) and re-folds against the fresh page bodies. The subjectID names the page
// that conflicted; the whole merge stage re-runs (the merge prompt operates over
// the manifest), which is a faithful superset of "that page only" — it re-reads the
// fresh version for the conflicting page and never re-extracts or re-resolves.
//
// It is exposed so the worker's conflict loop (internal/worker) can re-merge an
// integrator-agnostically without the run/commit layer importing the merge stage.
func (d *Document) ReMerge(ctx context.Context, m *Manifest, subjectID string) error {
	if _, err := d.merger.Merge(ctx, m); err != nil {
		return fmt.Errorf("document: re-merge after conflict (subject %q): %w", subjectID, err)
	}
	return nil
}

// ReResolve re-runs the RESOLVE → MATCH → MERGE STAGES for the ONE subject that hit
// the duplicate-mint conflict (design §3, P7b2: "restart at resolve for the colliding
// subject only"). The subjectID is the manifest subject id that collided — a
// freshly-minted subject whose alias insert hit UNIQUE(type, norm) against a
// concurrent run that minted the same subject first. EXTRACT IS NEVER RE-ENTERED:
// nothing another run did invalidates what THIS document said; only the identity
// resolution can change, because the lookup now hits the winner's freshly-committed
// aliases (so this run's subject resolves ONTO the winner instead of minting a
// duplicate).
//
// It locates the conflicting subject in the manifest by its (now-stale) minted
// SubjectID, re-resolves that one subject's extracted slice (its type/name/aliases/
// claims) through the SAME resolver+assembler the first pass used — which, with the
// winner now in the registry, yields OutcomeResolved onto the winner (or, if the
// collision was a same-norm-but-genuinely-different case match disambiguates, a fresh
// id again). The re-resolved subject's annotations (SubjectID, TargetPage, claims)
// REPLACE the conflicting entry in place, preserving manifest order and every other
// subject's already-merged content. Merge then re-folds for the whole manifest (its
// target page now points at the winner), re-reading the fresh base version, so the
// next commit's version guard and §6.1 gate run against the right page.
//
// It is exposed so the worker's conflict loop can re-resolve integrator-agnostically
// without the run/commit layer importing the resolve/assemble/merge stages.
func (d *Document) ReResolve(ctx context.Context, m *Manifest, subjectID string) error {
	idx := -1
	for i := range m.Subjects {
		if m.Subjects[i].SubjectID == subjectID {
			idx = i
			break
		}
	}
	if idx < 0 {
		return fmt.Errorf("document: re-resolve: subject %q not in manifest", subjectID)
	}

	// Re-resolve just this subject's extracted slice (NOT extract — design §3). The
	// extracted fields are carried verbatim on the manifest entry; only the identity
	// resolution is recomputed against the now-updated registry.
	orig := m.Subjects[idx]
	extracted := Subject{
		Type:    orig.Type,
		Kind:    orig.Kind,
		Name:    orig.Name,
		Aliases: orig.Aliases,
		Claims:  orig.Claims,
	}
	resolutions, err := d.res.Resolve(ctx, []Subject{extracted})
	if err != nil {
		return fmt.Errorf("document: re-resolve (subject %q): %w", subjectID, err)
	}
	resolved, err := d.asm.Assemble(ctx, resolutions)
	if err != nil {
		return fmt.Errorf("document: re-resolve assemble (subject %q): %w", subjectID, err)
	}
	if len(resolved.Subjects) != 1 {
		return fmt.Errorf("document: re-resolve produced %d subjects, want 1", len(resolved.Subjects))
	}

	// Replace the conflicting entry in place: take the freshly-resolved identity
	// annotations (SubjectID, TargetPage) but keep position and let merge re-derive
	// the page content. Carry forward any dup pairs the re-resolution surfaced.
	re := resolved.Subjects[0]
	re.PageTitle = ""
	re.PageBody = ""
	re.Superseded = nil
	re.BaseVersion = 0
	m.Subjects[idx] = re
	mergeDupPairs(m, resolved.DupPairs)

	// Re-fold (merge) the whole manifest so the re-targeted subject's page content
	// and fresh base version are recomputed; other subjects are re-read at their
	// current versions, which is correct (the commit guards each independently).
	if _, err := d.merger.Merge(ctx, m); err != nil {
		return fmt.Errorf("document: re-resolve merge (subject %q): %w", subjectID, err)
	}
	return nil
}

// mergeDupPairs folds new canonical-order dup pairs into the manifest's DupPairs,
// de-duplicating against what is already there (the re-resolution's match arm may
// surface a fresh pair).
func mergeDupPairs(m *Manifest, pairs []DupPair) {
	seen := make(map[DupPair]struct{}, len(m.DupPairs))
	for _, p := range m.DupPairs {
		seen[p] = struct{}{}
	}
	for _, p := range pairs {
		if _, ok := seen[p]; ok {
			continue
		}
		seen[p] = struct{}{}
		m.DupPairs = append(m.DupPairs, p)
	}
}
