package integrate

import (
	"context"
	"fmt"
)

// Digest is the REAL digest-pass Integrator (P8): it reuses the document pass's
// resolve→assemble→merge pipeline verbatim, swapping only the front stage —
// COMPILE for event piles in place of EXTRACT for document bytes (design §5). It
// implements the SAME P4 Integrator interface and emits the SAME Manifest, so the
// spine and the end-of-run transaction are unchanged: merge cannot tell which
// integrator ran ("the manifest generalizes", §5). One Digest is constructed per
// configured digest entry (job name), each over the same shared stages but its own
// job identity, so the worker's cron map binds entry name → integrator.
//
// Digest does NOT touch the DB for the terminal write (the Integrator contract):
// it produces the Manifest; the caller's end-of-run transaction owns the commit.
// Under the locked Framing 2 (P1) the claimed unit is a (cron-row, entry) pair —
// Unit.CausedBy is the cron row id (→ runs.caused_by, the batch-failure
// granularity, §7), Unit.Entry names this digest entry.
type Digest struct {
	job string
	src eventSource
	cmp *Compiler
	res *Resolver
	asm *Assembler
	mrg *Merger
}

// NewDigest builds a digest-pass integrator for one configured entry (its job
// name) from the shared stages. All stages are injected (each over its
// config-injected triple) so the digest pass is the composition of the per-site
// functions the eval harness scores — never a fork. compile is the only new stage;
// resolve/assemble/merge are the document pass's, reused verbatim.
func NewDigest(job string, src eventSource, cmp *Compiler, res *Resolver, asm *Assembler, mrg *Merger) *Digest {
	return &Digest{job: job, src: src, cmp: cmp, res: res, asm: asm, mrg: mrg}
}

// Job is this digest entry's stable job name (runs.job; the cron TryLock key — at
// most one in-flight run per batch-entry name, design §5). It is the name bound in
// the jobs config (e.g. "crm-digest").
func (d *Digest) Job() string { return d.job }

// Integrate runs the digest pass for one claimed (cron-row, entry) unit and
// returns its Manifest. It performs NO terminal write — the worker's end-of-run
// transaction commits the Manifest atomically (the cron row's inbox stamp is the
// worker-local completion-time join, deferred to StampCron). An error means the
// run failed cleanly (the transaction is never committed; the cron row stays
// pending — the retry authorization, design §5/§7).
func (d *Digest) Integrate(ctx context.Context, unit Unit) (*Manifest, error) {
	// Stamp by id list, never by selector (design §5): the source resolves the
	// pending event pile ONCE here; re-evaluating the selector at commit would
	// silently drop mid-run arrivals never compiled — the worst failure class.
	events, err := d.src.Events(ctx, unit.Entry)
	if err != nil {
		return nil, fmt.Errorf("digest %q: load events: %w", unit.Entry, err)
	}
	if len(events) == 0 {
		// Nothing pending for this entry: an empty manifest is a clean no-op commit
		// (no pages, no dup pairs). The cron completion-time join still stamps the
		// cron row once all bound entries' runs succeed.
		return &Manifest{}, nil
	}

	// compile → []Subject (extract's output schema, per-claim cites + occurred_at).
	subjects, err := d.cmp.Compile(ctx, events)
	if err != nil {
		return nil, fmt.Errorf("digest %q: compile: %w", unit.Entry, err)
	}

	// resolve → per-subject mechanical resolution outcome (zero LLM) — shared.
	resolutions, err := d.res.Resolve(ctx, subjects)
	if err != nil {
		return nil, fmt.Errorf("digest %q: resolve: %w", unit.Entry, err)
	}

	// assemble → the Manifest (match runs on shortlists; dup_pairs folded in) — shared.
	manifest, err := d.asm.Assemble(ctx, resolutions)
	if err != nil {
		return nil, fmt.Errorf("digest %q: assemble: %w", unit.Entry, err)
	}

	// merge → fold claims into prose pages; record base versions + superseded +
	// stale notes onto the manifest — shared.
	if _, err := d.mrg.Merge(ctx, manifest); err != nil {
		return nil, fmt.Errorf("digest %q: merge: %w", unit.Entry, err)
	}

	return manifest, nil
}

// ReMerge re-runs the merge stage only for an existing manifest after an
// optimistic-commit lost-update conflict (design §3, P7b) — identical to the
// document pass's, because the digest reuses the same merge stage. Exposed so the
// worker's conflict loop can re-merge integrator-agnostically.
func (d *Digest) ReMerge(ctx context.Context, m *Manifest, subjectID string) error {
	if _, err := d.mrg.Merge(ctx, m); err != nil {
		return fmt.Errorf("digest: re-merge after conflict (subject %q): %w", subjectID, err)
	}
	return nil
}

// ReResolve re-runs resolve→match→merge for the one subject that hit a
// duplicate-mint conflict (design §3, P7b2) — identical mechanics to the document
// pass: compile is never re-entered (nothing another run did invalidates what the
// events said; only the identity resolution can change). Exposed so the worker's
// conflict loop can re-resolve integrator-agnostically.
func (d *Digest) ReResolve(ctx context.Context, m *Manifest, subjectID string) error {
	idx := -1
	for i := range m.Subjects {
		if m.Subjects[i].SubjectID == subjectID {
			idx = i
			break
		}
	}
	if idx < 0 {
		return fmt.Errorf("digest: re-resolve: subject %q not in manifest", subjectID)
	}

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
		return fmt.Errorf("digest: re-resolve (subject %q): %w", subjectID, err)
	}
	resolved, err := d.asm.Assemble(ctx, resolutions)
	if err != nil {
		return fmt.Errorf("digest: re-resolve assemble (subject %q): %w", subjectID, err)
	}
	if len(resolved.Subjects) != 1 {
		return fmt.Errorf("digest: re-resolve produced %d subjects, want 1", len(resolved.Subjects))
	}

	re := resolved.Subjects[0]
	re.PageTitle = ""
	re.PageBody = ""
	re.Superseded = nil
	re.BaseVersion = 0
	m.Subjects[idx] = re
	mergeDupPairs(m, resolved.DupPairs)

	if _, err := d.mrg.Merge(ctx, m); err != nil {
		return fmt.Errorf("digest: re-resolve merge (subject %q): %w", subjectID, err)
	}
	return nil
}
