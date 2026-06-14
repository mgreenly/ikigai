package integrate

import (
	"reflect"
	"testing"
)

// TestManifestCompleteness is the table-driven, ENUMERATION-DERIVED completeness
// gate the plan's "Manifest canonicity" + "Phase completion is a checklist"
// standing rules require: its cases ARE the Manifest canonicity enumeration —
// one row per field above, paired with its declared consumer — and it asserts, by
// REFLECTION over the Go types, that every named field is present, failing the
// phase if any is absent. This is the in-memory mirror of the §12 schema test:
// checked against the EXTERNAL spec (the enumeration), never against P4's own
// reading. The round-trip test (run package) cannot catch an omitted field — the
// stub never populates a field the type lacks — so completeness is THIS test's
// job, not a P7b-time audit.
//
// A field discovered missing later is added to the Manifest canonicity
// enumeration FIRST, then to this table and the type together — so the contract
// and its checker never diverge.
func TestManifestCompleteness(t *testing.T) {
	// (struct-zero-value, "Field.Path", declared consumer) — verbatim from the
	// Manifest canonicity enumeration.
	cases := []struct {
		root      any
		field     string
		consumer  string
	}{
		// subjects[], addressable by subject_id — extract/compile emit; merge/commit read.
		{Manifest{}, "Subjects", "spine / end-of-run transaction (the addressable subjects)"},
		{Subject{}, "Type", "merge routing (§4.2/§5)"},
		{Subject{}, "Kind", "merge routing (§4.2/§5)"},
		{Subject{}, "Name", "merge identity-establishing lead (§4.2/§5)"},
		{Subject{}, "Aliases", "merge lead + resolve registry lookup (§4.2/§5)"},
		// generalized {text, cites[]} claim shape per subject.
		{Subject{}, "Claims", "P7a merge + P8 compile (§4.3/§5)"},
		{Claim{}, "Text", "merge/compile claim prose (§4.3/§5)"},
		{Claim{}, "Cites", "merge/compile per-claim cites (§4.3/§5)"},
		// resolution annotations on each subject entry.
		{Subject{}, "SubjectID", "P6b2 first producer (§4.4); the addressing key (§3)"},
		{Subject{}, "TargetPage", "P6b2 write set = subjects' target pages (§4.4)"},
		// per-subject base version slot.
		{Subject{}, "BaseVersion", "P7b optimistic-commit WHERE subject=? AND version=? (§3)"},
		// per-subject occurred_at — first-writer-wins, events only.
		{Subject{}, "OccurredAt", "P7a commit (§4.1)"},
		// per-page superseded — the §6.1 dropped-citation gate.
		{Subject{}, "Superseded", "P7a §6.1 citation-preservation gate (§6.1)"},
		// stale_notes[] — merge's fold side channel, written in the end-of-run commit.
		{Manifest{}, "StaleNotes", "P7a end-of-run transaction (§6 / §12 #4)"},
		{StaleNote{}, "Subject", "stale_notes.subject (§12 #4)"},
		{StaleNote{}, "Note", "stale_notes.note (§12 #4)"},
		{StaleNote{}, "Cites", "stale_notes.cites (§12 #4)"},
		// dup_pairs[] — candidate duplicate pairs, canonical order.
		{Manifest{}, "DupPairs", "P7a end-of-run transaction → FlagDup (§3/§4.3/§4.5)"},
		{DupPair{}, "SubjectA", "dup_flags.subject_a, smaller ULID (§3)"},
		{DupPair{}, "SubjectB", "dup_flags.subject_b, larger ULID (§3)"},
	}

	for _, tc := range cases {
		rt := reflect.TypeOf(tc.root)
		if _, ok := rt.FieldByName(tc.field); !ok {
			t.Errorf("Manifest contract incomplete: %s.%s is MISSING (consumer: %s) — "+
				"add it to the type, per the Manifest canonicity standing rule",
				rt.Name(), tc.field, tc.consumer)
		}
	}
}
