package gen

import "wiki/internal/eval"

// The four identity-corpus consumers (eval design "Shared corpora": one truth,
// four consumers): match, dup_judge, candidates, sweep. Each reads the shared
// identityCorpus so the gold ids stay consistent across all four sites.

// --- match ---
//
// Input shape: integrate.matchInput (incoming subject + candidate shortlist with
// each candidate's body excerpt). Gold shape: matchGold ({same, dup_pairs}).
// failure_tag vocabulary: blunt_same, blunt_no_match, identical_name_diff_thing,
// alias_same (different name, same thing), wrong_candidate_merge.

// matchCand is the candidate shortlist entry the match input pins (mirrors the
// adapter's matchInput.candidates).
type matchCand struct {
	SubjectID     string   `json:"subject_id"`
	Type          string   `json:"type"`
	CanonicalName string   `json:"canonical_name"`
	Aliases       []string `json:"aliases"`
	Body          string   `json:"body"`
}

type matchClaim struct {
	Text  string   `json:"text"`
	Cites []string `json:"cites"`
}

type matchIn struct {
	Incoming struct {
		Type    string       `json:"type"`
		Kind    string       `json:"kind"`
		Name    string       `json:"name"`
		Aliases []string     `json:"aliases"`
		Claims  []matchClaim `json:"claims"`
	} `json:"incoming"`
	Candidates []matchCand `json:"candidates"`
}

type pair struct {
	A string `json:"a"`
	B string `json:"b"`
}

type matchGoldRec struct {
	Same     string `json:"same"`
	DupPairs []pair `json:"dup_pairs"`
}

func cand(id string) matchCand {
	s := byID(id)
	return matchCand{SubjectID: s.ID, Type: s.Type, CanonicalName: s.CanonicalName, Aliases: s.Aliases, Body: s.Body}
}

func mkMatchIn(typ, kind, name string, aliases []string, claim string, inbox string, cands ...matchCand) matchIn {
	var in matchIn
	in.Incoming.Type = typ
	in.Incoming.Kind = kind
	in.Incoming.Name = name
	in.Incoming.Aliases = aliases
	in.Incoming.Claims = []matchClaim{{Text: claim, Cites: []string{inbox}}}
	in.Candidates = cands
	return in
}

func matchCases() []eval.Case {
	apple := byID("01HXIDCORPUSAPPLEINC0000001")
	appleRec := byID("01HXIDCORPUSAPPLEREC0000002")
	ibm := byID("01HXIDCORPUSIBM000000000003")
	planet := byID("01HXIDCORPUSMERCURYPLT00004")
	elem := byID("01HXIDCORPUSMERCURYELE00005")

	return []eval.Case{
		// BLUNT: identical name + corroborating claim -> same.
		caseRecord("match", "match-0001", "blunt_same",
			mkMatchIn("entity", "org", "Apple Inc.", []string{"Apple"},
				"Apple Inc. is a technology company based in Cupertino.",
				"01HXMATCHINBOX00000000000001",
				cand(apple.ID), cand(appleRec.ID)),
			matchGoldRec{Same: apple.ID, DupPairs: []pair{}}),

		// BLUNT: obviously different type/thing, single candidate -> no_match.
		caseRecord("match", "match-0002", "blunt_no_match",
			mkMatchIn("entity", "person", "Ada Lovelace", nil,
				"Ada Lovelace was a 19th-century mathematician.",
				"01HXMATCHINBOX00000000000002",
				cand(apple.ID)),
			matchGoldRec{Same: "", DupPairs: []pair{}}),

		// SUBTLE: identical name, DIFFERENT thing (Apple Records vs the candidate
		// Apple Inc.) -> no_match. The dangerous false-merge trap.
		caseRecord("match", "match-0003", "identical_name_diff_thing",
			mkMatchIn("entity", "org", "Apple Records", nil,
				"Apple Records is the Beatles' record label.",
				"01HXMATCHINBOX00000000000003",
				cand(apple.ID)),
			matchGoldRec{Same: "", DupPairs: []pair{}}),

		// SUBTLE: different NAME, same thing (Big Blue == IBM via alias) -> same.
		caseRecord("match", "match-0004", "alias_same",
			mkMatchIn("entity", "org", "Big Blue", []string{"International Business Machines"},
				"Big Blue is a large technology and consulting company in Armonk, New York.",
				"01HXMATCHINBOX00000000000004",
				cand(ibm.ID), cand(apple.ID)),
			matchGoldRec{Same: ibm.ID, DupPairs: []pair{}}),

		// SUBTLE: identical name "Mercury", two same-name candidates of the same type
		// (planet vs element) but the incoming is the ELEMENT -> must pick the element,
		// not false-merge into the planet.
		caseRecord("match", "match-0005", "identical_name_diff_thing",
			mkMatchIn("entity", "thing", "Mercury", []string{"quicksilver"},
				"Mercury is a liquid metal with the chemical symbol Hg.",
				"01HXMATCHINBOX00000000000005",
				cand(planet.ID), cand(elem.ID)),
			matchGoldRec{Same: elem.ID, DupPairs: []pair{}}),
	}
}

// --- dup_judge ---
//
// Output shape: dupJudgeOutput ({verdict: merge|dismiss|cant_tell}). Gold:
// dupJudgeGold ({verdict, evidence_present}). The input is the two pages whose
// identity the judge weighs. failure_tag: blunt_merge, blunt_dismiss,
// evidence_present_should_merge, no_evidence_cant_tell.

type dupJudgeIn struct {
	A struct {
		SubjectID string `json:"subject_id"`
		Title     string `json:"title"`
		Body      string `json:"body"`
	} `json:"a"`
	B struct {
		SubjectID string `json:"subject_id"`
		Title     string `json:"title"`
		Body      string `json:"body"`
	} `json:"b"`
}

type dupJudgeGoldRec struct {
	Verdict         string `json:"verdict"`
	EvidencePresent bool   `json:"evidence_present"`
}

func mkDupIn(a, b idSubject) dupJudgeIn {
	var in dupJudgeIn
	in.A.SubjectID, in.A.Title, in.A.Body = a.ID, a.CanonicalName, a.Body
	in.B.SubjectID, in.B.Title, in.B.Body = b.ID, b.CanonicalName, b.Body
	return in
}

func dupJudgeCases() []eval.Case {
	apple := byID("01HXIDCORPUSAPPLEINC0000001")
	appleRec := byID("01HXIDCORPUSAPPLEREC0000002")
	ibm := byID("01HXIDCORPUSIBM000000000003")
	planet := byID("01HXIDCORPUSMERCURYPLT00004")
	elem := byID("01HXIDCORPUSMERCURYELE00005")

	// A deliberately-thin duplicate of IBM to make a blunt "these are the same"
	// merge pair (different page id, same subject, corroborating body).
	ibmDup := idSubject{
		ID: "01HXIDCORPUSIBMDUP000000008", Type: "entity", Kind: "org",
		CanonicalName: "International Business Machines",
		Body:          "International Business Machines, known as Big Blue, is a technology firm in Armonk, New York.",
	}

	// A two-line stub with no decisive evidence either way (genuinely cant_tell).
	thinA := idSubject{ID: "01HXIDCORPUSTHINA00000000009", CanonicalName: "Project Atlas",
		Body: "Project Atlas is an internal initiative."}
	thinB := idSubject{ID: "01HXIDCORPUSTHINB00000000010", CanonicalName: "Project Atlas",
		Body: "Project Atlas is a codename mentioned once in passing."}

	return []eval.Case{
		// BLUNT merge: IBM and its corroborating duplicate.
		caseRecord("dup_judge", "dup_judge-0001", "blunt_merge",
			mkDupIn(ibm, ibmDup),
			dupJudgeGoldRec{Verdict: "merge", EvidencePresent: true}),

		// BLUNT dismiss: Apple Inc. vs Apple Records — same name, plainly different.
		caseRecord("dup_judge", "dup_judge-0002", "blunt_dismiss",
			mkDupIn(apple, appleRec),
			dupJudgeGoldRec{Verdict: "dismiss", EvidencePresent: true}),

		// SUBTLE dismiss: Mercury planet vs Mercury element — identical name, decisive
		// distinguishing evidence present; punting here is laziness.
		caseRecord("dup_judge", "dup_judge-0003", "evidence_present_should_merge",
			mkDupIn(planet, elem),
			dupJudgeGoldRec{Verdict: "dismiss", EvidencePresent: true}),

		// SUBTLE cant_tell: two thin stubs with no decisive evidence — the ONLY
		// legitimate cant_tell (evidence_present=false, so abstaining is not lazy).
		caseRecord("dup_judge", "dup_judge-0004", "no_evidence_cant_tell",
			mkDupIn(thinA, thinB),
			dupJudgeGoldRec{Verdict: "cant_tell", EvidencePresent: false}),
	}
}

// --- candidates / search / sweep (retrieval lanes) ---
//
// Output shape: retrievalOutput ({results:[id...]}). Gold: retrievalGold
// ({relevant:[id...]}). The input is the query the lane runs. For the identity
// lanes (candidates, sweep) the query is over the identity corpus; for search the
// query is over the synthetic wiki.

type retrievalIn struct {
	Query string   `json:"query"`
	Corpus []string `json:"corpus"` // the candidate id universe the lane ranks
}

type retrievalGoldRec struct {
	Relevant []string `json:"relevant"`
}

func allIdentityIDs() []string {
	out := make([]string, len(identityCorpus))
	for i, s := range identityCorpus {
		out[i] = s.ID
	}
	return out
}

func candidatesCases() []eval.Case {
	apple := "01HXIDCORPUSAPPLEINC0000001"
	appleRec := "01HXIDCORPUSAPPLEREC0000002"
	planet := "01HXIDCORPUSMERCURYPLT00004"
	elem := "01HXIDCORPUSMERCURYELE00005"
	fred := "01HXIDCORPUSFREDMERCURY0006"
	univ := allIdentityIDs()

	return []eval.Case{
		// BLUNT: an exact-name query must surface its subject (recall is king — a miss
		// mints a permanent duplicate).
		caseRecord("candidates", "candidates-0001", "missed_exact_name",
			retrievalIn{Query: "Apple Inc.", Corpus: univ},
			retrievalGoldRec{Relevant: []string{apple}}),

		// SUBTLE: an ambiguous "Apple" query must surface BOTH apple subjects (the
		// shortlist feeds match; missing either is a missed candidate).
		caseRecord("candidates", "candidates-0002", "missed_homonym",
			retrievalIn{Query: "Apple", Corpus: univ},
			retrievalGoldRec{Relevant: []string{apple, appleRec}}),

		// SUBTLE: "Mercury" is a three-way homonym — all three must be candidates.
		caseRecord("candidates", "candidates-0003", "missed_homonym",
			retrievalIn{Query: "Mercury", Corpus: univ},
			retrievalGoldRec{Relevant: []string{planet, elem, fred}}),
	}
}

func sweepCases() []eval.Case {
	univ := allIdentityIDs()
	// The sweep discovers semantic duplicate PAIRS; gold "relevant" entries are pair
	// keys (a<NUL>b canonical order is the scorer's, but the dataset names the pair as
	// a single id token for the recall set — we encode the pair as "idA|idB").
	ibmPair := "01HXIDCORPUSIBM000000000003|01HXIDCORPUSIBMDUP000000008"

	return []eval.Case{
		// BLUNT: an obvious duplicate pair (IBM and its alias-twin) must be swept.
		caseRecord("sweep", "sweep-0001", "missed_obvious_pair",
			retrievalIn{Query: "near-duplicate organizations", Corpus: append(univ, "01HXIDCORPUSIBMDUP000000008")},
			retrievalGoldRec{Relevant: []string{ibmPair}}),

		// SUBTLE: NO true duplicate pair exists among distinct same-name homonyms;
		// gold is empty (sweeping Mercury-planet with Mercury-element would be a false
		// pair). A perfect lane returns nothing -> recall 1, zero missed pairs.
		caseRecord("sweep", "sweep-0002", "false_pair_homonym",
			retrievalIn{Query: "Mercury duplicates", Corpus: univ},
			retrievalGoldRec{Relevant: []string{}}),
	}
}
