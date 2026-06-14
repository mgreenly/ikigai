package gen

import "wiki/internal/eval"

// The standalone-set sites (eval design "Plus standalone sets for extract,
// compile, merge ... canonical-name pick"). Each authors its own input/gold; no
// shared corpus.

// --- extract ---
//
// Input: a single document (the real extract call site consumes a document body +
// its inbox id). Gold: setAlignPayload ({subjects:[{type,name,claims,occurred_at}]})
// — the reference subject + claim set. failure_tag vocabulary: blunt_single,
// multi_subject, over_extract_trap (a doc that tempts a spurious extra subject),
// pronoun_self_containment.

type extractIn struct {
	InboxID string `json:"inbox_id"`
	Body    string `json:"body"`
}

type goldClaim struct {
	Text  string   `json:"text"`
	Cites []string `json:"cites"`
}
type goldSubject struct {
	Type       string      `json:"type"`
	Name       string      `json:"name"`
	OccurredAt string      `json:"occurred_at,omitempty"`
	Claims     []goldClaim `json:"claims"`
}
type goldSubjects struct {
	Subjects []goldSubject `json:"subjects"`
}

func extractCases() []eval.Case {
	return []eval.Case{
		// BLUNT: one clear subject, one claim.
		caseRecord("extract", "extract-0001", "blunt_single",
			extractIn{InboxID: "01HXEXTRACTINBOX000000001",
				Body: "Zephyr Airlines is a regional carrier based in Denver, Colorado."},
			goldSubjects{Subjects: []goldSubject{
				{Type: "entity", Name: "Zephyr Airlines", Claims: []goldClaim{
					{Text: "Zephyr Airlines is a regional carrier based in Denver, Colorado.",
						Cites: []string{"01HXEXTRACTINBOX000000001"}},
				}},
			}}),

		// SUBTLE: two distinct subjects in one document (org + its CEO person).
		caseRecord("extract", "extract-0002", "multi_subject",
			extractIn{InboxID: "01HXEXTRACTINBOX000000002",
				Body: "Acme Corporation makes hydraulic presses. Its chief executive is Dana Reyes."},
			goldSubjects{Subjects: []goldSubject{
				{Type: "entity", Name: "Acme Corporation", Claims: []goldClaim{
					{Text: "Acme Corporation makes hydraulic presses.", Cites: []string{"01HXEXTRACTINBOX000000002"}},
				}},
				{Type: "entity", Name: "Dana Reyes", Claims: []goldClaim{
					{Text: "Dana Reyes is the chief executive of Acme Corporation.", Cites: []string{"01HXEXTRACTINBOX000000002"}},
				}},
			}}),

		// SUBTLE / dangerous: a document that mentions a thing in passing without
		// asserting a standalone fact — extracting it as a separate subject is
		// OVER-EXTRACTION. Gold has only the one real subject.
		caseRecord("extract", "extract-0003", "over_extract_trap",
			extractIn{InboxID: "01HXEXTRACTINBOX000000003",
				Body: "Orchids prefer indirect light. (Watering schedules vary, but that is " +
					"a separate topic not detailed here.)"},
			goldSubjects{Subjects: []goldSubject{
				{Type: "entity", Name: "Orchids", Claims: []goldClaim{
					{Text: "Orchids prefer indirect light.", Cites: []string{"01HXEXTRACTINBOX000000003"}},
				}},
			}}),
	}
}

// --- compile ---
//
// Input: a pile of event records (the real compile call site folds events into
// subjects). Gold: setAlignPayload with the compiled subjects, each event subject
// carrying occurred_at and per-claim cites (the compile deltas the scorer reads).
// failure_tag: blunt_single_event, multi_event_fold, occurred_at_fidelity.

type eventRec struct {
	InboxID    string `json:"inbox_id"`
	OccurredAt string `json:"occurred_at"`
	Text       string `json:"text"`
}
type compileIn struct {
	Events []eventRec `json:"events"`
}

func compileCases() []eval.Case {
	return []eval.Case{
		// BLUNT: a single event compiles to one event subject with its occurred_at.
		caseRecord("compile", "compile-0001", "blunt_single_event",
			compileIn{Events: []eventRec{
				{InboxID: "01HXCOMPILEINBOX00000001", OccurredAt: "2024-10",
					Text: "Acme reported Q3 revenue up 5 percent."},
			}},
			goldSubjects{Subjects: []goldSubject{
				{Type: "event", Name: "Acme Q3 earnings", OccurredAt: "2024-10", Claims: []goldClaim{
					{Text: "Acme reported Q3 revenue up 5 percent.", Cites: []string{"01HXCOMPILEINBOX00000001"}},
				}},
			}}),

		// SUBTLE: two events about the same subject fold, each claim keeping ITS own
		// cite (per-claim citation fidelity is the compile delta).
		caseRecord("compile", "compile-0002", "multi_event_fold",
			compileIn{Events: []eventRec{
				{InboxID: "01HXCOMPILEINBOX00000002", OccurredAt: "2024-10",
					Text: "Acme reported Q3 revenue up 5 percent."},
				{InboxID: "01HXCOMPILEINBOX00000003", OccurredAt: "2024-10",
					Text: "Acme raised its full-year guidance."},
			}},
			goldSubjects{Subjects: []goldSubject{
				{Type: "event", Name: "Acme Q3 earnings", OccurredAt: "2024-10", Claims: []goldClaim{
					{Text: "Acme reported Q3 revenue up 5 percent.", Cites: []string{"01HXCOMPILEINBOX00000002"}},
					{Text: "Acme raised its full-year guidance.", Cites: []string{"01HXCOMPILEINBOX00000003"}},
				}},
			}}),
	}
}

// --- merge ---
//
// Input: a manifest slice (built directly, skipping extract — eval design: "merge's
// inputs are internal", manifests built directly). Gold: mergeGold ({write_set,
// old_bodies, must_survive, source_text, lead_name}). failure_tag:
// blunt_new_page, fold_into_existing, cite_preservation_trap (a fold that must
// keep an old citation).

type mergeSubjectIn struct {
	SubjectID string      `json:"subject_id"`
	Type      string      `json:"type"`
	Name      string      `json:"name"`
	Claims    []goldClaim `json:"claims"`
	BasePage  string      `json:"base_page"` // existing page body (empty for a new page)
}
type mergeIn struct {
	Subjects []mergeSubjectIn `json:"subjects"`
}
type mergeGoldRec struct {
	WriteSet    []string          `json:"write_set"`
	OldBodies   map[string]string `json:"old_bodies"`
	MustSurvive []string          `json:"must_survive"`
	SourceText  string            `json:"source_text"`
	LeadName    string            `json:"lead_name"`
}

func mergeCases() []eval.Case {
	return []eval.Case{
		// BLUNT: a brand-new subject -> one new page, no prior citations to preserve.
		caseRecord("merge", "merge-0001", "blunt_new_page",
			mergeIn{Subjects: []mergeSubjectIn{
				{SubjectID: "01HXMERGESUBJZEPHYR000001", Type: "entity", Name: "Zephyr Airlines",
					Claims: []goldClaim{{Text: "Zephyr Airlines is a regional carrier based in Denver.",
						Cites: []string{"01HXMERGEINBOX0000000001"}}}},
			}},
			mergeGoldRec{
				WriteSet:    []string{"01HXMERGESUBJZEPHYR000001"},
				OldBodies:   map[string]string{},
				MustSurvive: []string{"01HXMERGEINBOX0000000001"},
				SourceText:  "Zephyr Airlines is a regional carrier based in Denver.",
				LeadName:    "Zephyr Airlines",
			}),

		// SUBTLE / dangerous: fold a new claim into an existing page that already cites
		// [old]; the merge must KEEP [old] (the §6.1 citation-preservation trap) unless
		// it declares it superseded.
		caseRecord("merge", "merge-0002", "cite_preservation_trap",
			mergeIn{Subjects: []mergeSubjectIn{
				{SubjectID: "01HXMERGESUBJACME0000002", Type: "entity", Name: "Acme Corporation",
					Claims: []goldClaim{{Text: "Acme expanded to Detroit in 1960.",
						Cites: []string{"01HXMERGEINBOX0000000003"}}},
					BasePage: "Acme Corporation is a manufacturer founded in 1947 [01HXMERGEINBOX0000000002]."},
			}},
			mergeGoldRec{
				WriteSet: []string{"01HXMERGESUBJACME0000002"},
				OldBodies: map[string]string{
					"01HXMERGESUBJACME0000002": "Acme Corporation is a manufacturer founded in 1947 [01HXMERGEINBOX0000000002].",
				},
				MustSurvive: []string{"01HXMERGEINBOX0000000002", "01HXMERGEINBOX0000000003"},
				SourceText:  "Acme Corporation is a manufacturer founded in 1947. Acme expanded to Detroit in 1960.",
				LeadName:    "Acme Corporation",
			}),
	}
}

// --- canonical_name (degenerate kind-2) ---
//
// Input: the observed name variants. Output: {name}. Gold: {name} (the
// conventional pick). failure_tag: prefer_formal, prefer_owner_casing.

type canonicalNameIn struct {
	Variants []string `json:"variants"`
}
type nameRec struct {
	Name string `json:"name"`
}

func canonicalNameCases() []eval.Case {
	return []eval.Case{
		// BLUNT: prefer the full formal form over an abbreviation.
		caseRecord("canonical_name", "canonical_name-0001", "prefer_formal",
			canonicalNameIn{Variants: []string{"IBM", "International Business Machines", "ibm"}},
			nameRec{Name: "International Business Machines"}),

		// SUBTLE: prefer the subject's own preferred casing over a lowercased variant.
		caseRecord("canonical_name", "canonical_name-0002", "prefer_owner_casing",
			canonicalNameIn{Variants: []string{"apple inc", "Apple Inc.", "APPLE"}},
			nameRec{Name: "Apple Inc."}),
	}
}
