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
