// Package integrate holds the two cross-phase types every integrator hands off
// through — the Manifest and the Integrator interface — frozen here in P4
// BEFORE any real integrator exists. The worker spine (internal/worker) and the
// end-of-run transaction (internal/run) consume a Manifest, never
// integrator-specific data; that is the whole "swap is mechanical" bet of the
// plan's Sizing principle. P6a–P8 then fill behavior into these fixed shapes
// instead of re-deriving the seam.
//
// The Manifest is the in-memory work order, NEVER persisted (the run id is its
// durable identity) and empty-capable (a stub emits a minimal one). Its complete
// field set is the authoritative "Manifest canonicity" enumeration in
// docs/wiki-redesign-plan.md — this type TRANSCRIBES that enumeration; no phase
// re-derives the contract from the design's scattered prose. A field a later
// phase needs is added to that enumeration first (and the producers updated), so
// the contract and its consumers never diverge. The companion
// manifest_completeness_test.go asserts, by reflection over this type, that every
// named field is present — the in-memory mirror of the §12 schema test.
package integrate

// SubjectType is the closed three-type taxonomy (design §4.1): every subject is
// an entity, an event, or a concept. Merge reads it to route; the value is also
// the aliases.type / subjects.type the registry keys on.
type SubjectType = string

const (
	TypeEntity  SubjectType = "entity"
	TypeEvent   SubjectType = "event"
	TypeConcept SubjectType = "concept"
)

// Claim is the generalized {text, cites[]} claim shape carried on each subject
// (Manifest canonicity, §4.3/§5). The document pass fills Cites with the one
// inbox row id; compile fills per-claim cites. Merge (P7a) and compile (P8) read
// it.
type Claim struct {
	// Text is the claim's prose, as extract/compile emitted it.
	Text string
	// Cites are the inbox row id(s) the claim is sourced from (one for a
	// document-pass claim; per-claim for compile).
	Cites []string
}

// StaleNote is one staleness observation merge produces while folding (Manifest
// canonicity / §6 / §12 #4): the carrier through which merge signals which
// stale_notes the end-of-run transaction must write IN ITS EXISTING COMMIT, so
// the transaction owns the write rather than reaching into merge's side effects.
// The id/run_id/status columns are filled at write time per §12 #4; this struct
// carries only what merge knows.
type StaleNote struct {
	// Subject is the subject id whose page looks stale.
	Subject string
	// Note is what the writer observed (a sentence or two).
	Note string
	// Cites are the inbox id(s) of the new evidence that makes the repair legal.
	Cites []string
}

// DupPair is a candidate duplicate subject pair to flag, in canonical order
// (smaller ULID first — Manifest canonicity / §3 / eval obligation 3). Resolve's
// many-ids arm surfaces these (P6b) and match's side channel reports them
// (P6b2); P6b2 assembles them into the Manifest and P7a's end-of-run transaction
// inserts them into dup_flags via FlagDup.
type DupPair struct {
	// SubjectA is the smaller ULID of the candidate pair.
	SubjectA string
	// SubjectB is the larger ULID of the candidate pair.
	SubjectB string
}

// Subject is one extracted/compiled subject, INDIVIDUALLY ADDRESSABLE by
// SubjectID so a conflict can re-enter the existing stage functions for ONE
// subject without reshaping the type (Manifest canonicity / §4.1 — one page per
// subject, so every per-page slot hangs off the subject entry, never a
// manifest-global scalar). It carries (a) the extracted fields and (b) the
// resolution annotations merge fills in.
type Subject struct {
	// --- (a) extracted fields (extract/compile emit; merge reads) ---

	// Type routes the subject (merge reads it); one of TypeEntity/TypeEvent/
	// TypeConcept.
	Type SubjectType
	// Kind is the freeform, prompt-anchored subtype (e.g. "person", "org").
	Kind string
	// Name is the subject's primary name (merge reads it for the
	// identity-establishing lead — §4.2/§5).
	Name string
	// Aliases are the other names this subject is known by (merge reads them for
	// the lead; resolve normalizes them for the registry lookup).
	Aliases []string
	// Claims are the subject's generalized {text, cites[]} claims (§4.3/§5).
	Claims []Claim

	// --- (b) resolution annotations (resolve/merge fill; the commit reads) ---

	// PageTitle and PageBody are the rewritten prose page merge produces for this
	// subject's target page (Manifest canonicity / §4.4 "rewritten prose pages").
	// Merge's read+write-page work is CAPTURED into these slots rather than written
	// directly, so the end-of-run transaction owns the only write and there are zero
	// mid-run partial writes (§4.5). Populated by merge in P7a; read by P7a's
	// end-of-run transaction (the page upsert + the pages_fts sync). One page per
	// subject (§4.1), so the content lives here, never as a manifest-global scalar.
	PageTitle string
	PageBody  string

	// SubjectID is the resolved subject id (an existing registry id, or a freshly
	// minted ULID for a new subject). First read by P6b2, the first producer
	// (§4.4). It is also the addressing key for the whole entry.
	SubjectID string
	// TargetPage is the page this subject's knowledge lands on — exactly the
	// write-set member for this subject (§4.4). One page per subject, so it lives
	// here, not as a manifest-global scalar.
	TargetPage string
	// BaseVersion is the pages.version the page was read at — the per-subject base
	// version slot (Manifest canonicity / design §3 "the manifest records the
	// version merge read"). Populated at merge-read time in P7a; read by P7b's
	// per-page optimistic-commit `WHERE subject=? AND version=?` guard.
	BaseVersion int
	// OccurredAt is the per-subject first-writer-wins world-time prefix, EVENTS
	// ONLY (a property of each event subject — §4.1 subjects.occurred_at). Read by
	// P7a's commit. Empty for non-event subjects.
	OccurredAt string
	// Superseded carries the per-page dropped-citation declarations the §6.1 gate
	// checks (Manifest canonicity / §6.1), per page on the subject entry so P7b's
	// re-run can re-validate the gate against the re-run's fresh Superseded.
	// Populated by merge, read by P7a; on a P7b conflict re-merge it is re-derived
	// from the new merge output, never the stale original.
	Superseded []string
}

// Manifest is the in-memory work order the integrator produces and the end-of-run
// transaction consumes (Manifest canonicity). It is NEVER persisted (the run id
// is its durable identity) and empty-capable (a stub emits a minimal one). Every
// field below is named in the authoritative enumeration with its declared
// consumer; the table-driven completeness test asserts none is missing.
type Manifest struct {
	// Subjects are the extracted/compiled subjects, individually addressable by
	// SubjectID (Manifest canonicity / §4.1). Each carries its extracted fields
	// plus its resolution annotations.
	Subjects []Subject

	// StaleNotes is the stale-note set merge produces while folding — the carrier
	// through which merge signals which stale_notes the end-of-run transaction
	// must write in its existing commit (Manifest canonicity / §6 / §12 #4).
	StaleNotes []StaleNote

	// DupPairs are the candidate duplicate subject pairs to flag, each in
	// canonical order (Manifest canonicity / §3 / §4.3 / §4.5). Assembled by P6b2,
	// read by P7a's end-of-run transaction, which inserts them via FlagDup.
	DupPairs []DupPair
}

// WriteSet returns the manifest's write-set pages — exactly the target pages
// named by Subjects (each addressable by subject id), NOT a separate parallel
// structure (Manifest canonicity / §4.4 "the manifest's pages, exactly"). P6b2
// populates the write set as exactly the subjects' target pages; P7a's merge
// write set is that set verbatim. Returned derived (never stored) so the two can
// never diverge.
func (m *Manifest) WriteSet() []string {
	pages := make([]string, 0, len(m.Subjects))
	for _, s := range m.Subjects {
		if s.TargetPage != "" {
			pages = append(pages, s.TargetPage)
		}
	}
	return pages
}
