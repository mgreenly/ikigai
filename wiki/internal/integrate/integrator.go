package integrate

import "context"

// Integrator is the interface the document-pass stub, the cron/no-op stub, the
// real document pass (P7a), and the real compile (P8) ALL satisfy: given a claimed
// unit of work, run it and produce a Manifest. The shared resolve→merge→commit
// pipeline and the end-of-run transaction consume that Manifest, never
// integrator-specific data — which is exactly what lets merge "not tell which
// integrator ran" (P8).
//
// The contract is deliberately minimal: the worker has already claimed the unit
// (the in-flight TryLock) and inserted the `running` run row; Integrate does the
// LLM/compile work with NO lock held and returns the Manifest the end-of-run
// transaction will atomically commit. An error means the run failed cleanly — the
// transaction is never committed and the causing row stays pending (design §4.5).
type Integrator interface {
	// Job is the stable job name written to runs.job and used as the cron/lint
	// TryLock key (design §4.5: 'document-pass' | 'crm-digest' | 'lint-dups' | …).
	Job() string

	// Integrate runs the claimed unit and produces its Manifest. unit identifies
	// the claimed work (the causing inbox row, plus — under Framing 2 — the bound
	// digest entry). It does NOT touch the DB for the terminal write; the caller's
	// end-of-run transaction owns that.
	Integrate(ctx context.Context, unit Unit) (*Manifest, error)
}

// Unit is the claimed unit of work handed to an Integrator. For the document pass
// it is one inbox row (CausedBy = that row's id). For a digest under the locked
// Framing 2 (P1) it is a (cron-row, entry) pair: CausedBy is the cron row's inbox
// id and Entry names the bound digest entry. Lint jobs key on Job + CausedBy.
type Unit struct {
	// CausedBy is the inbox id of the causing row (→ runs.caused_by). For a
	// document pass it is the document row; for a digest it is the cron row.
	CausedBy string
	// Entry is the bound digest entry name under Framing 2 (empty for the document
	// pass and lint). It distinguishes the (cron-row, entry) claimable unit (P1).
	Entry string
}
