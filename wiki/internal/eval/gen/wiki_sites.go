package gen

import "wiki/internal/eval"

// The two synthetic-wiki consumers (eval design "Shared corpora": one synthetic
// wiki -> ask + search). Both draw from syntheticWiki; the ask generator also
// authors the GAP questions (no supporting page) that test abstention/fabrication,
// and the cross-page CONTRADICTION question (eval design: contradiction-surfacing).

// --- ask ---
//
// Input: a question + the synthetic-wiki id space the retriever runs over. Gold:
// askGold ({answer, supporting, should_abstain, spans, page_bodies}). failure_tag:
// blunt_answerable, gap_abstain (the fabrication trap), contradiction_surface,
// retrieval_dependency.

type askIn struct {
	Question string   `json:"question"`
	WikiIDs  []string `json:"wiki_ids"` // the page id universe ask retrieves over
}

type askGoldRec struct {
	Answer        string            `json:"answer"`
	Supporting    []string          `json:"supporting"`
	ShouldAbstain bool              `json:"should_abstain"`
	Spans         map[string]string `json:"spans"`
	PageBodies    map[string]string `json:"page_bodies"`
}

func allWikiIDs() []string {
	out := make([]string, len(syntheticWiki))
	for i, p := range syntheticWiki {
		out[i] = p.ID
	}
	return out
}

// bodyMap returns the page_bodies map the faithfulness check reads for the named
// pages (eval design askGold.page_bodies).
func bodyMap(ids ...string) map[string]string {
	m := map[string]string{}
	for _, id := range ids {
		m[id] = wikiPageByID(id).Body
	}
	return m
}

func askCases() []eval.Case {
	acme := "01HXWIKIPAGEACME00000000001"
	acmeHist := "01HXWIKIPAGEACMEHIST0000002"
	zephyr := "01HXWIKIPAGEZEPHYR000000003"
	univ := allWikiIDs()

	return []eval.Case{
		// BLUNT answerable: a fact stated plainly on one page.
		caseRecord("ask", "ask-0001", "blunt_answerable",
			askIn{Question: "Where is Zephyr Airlines based?", WikiIDs: univ},
			askGoldRec{
				Answer:     "Zephyr Airlines is based in Denver.",
				Supporting: []string{zephyr},
				Spans:      map[string]string{zephyr: "based in Denver"},
				PageBodies: bodyMap(zephyr),
			}),

		// DANGEROUS gap: no page states Acme's CEO salary -> the right behaviour is to
		// ABSTAIN. Answering is FABRICATION, the headline sin.
		caseRecord("ask", "ask-0002", "gap_abstain",
			askIn{Question: "What is the salary of Acme's chief executive?", WikiIDs: univ},
			askGoldRec{
				ShouldAbstain: true,
				PageBodies:    bodyMap(acme),
			}),

		// SUBTLE contradiction: the two Acme pages disagree on the founding year (1947
		// vs incorporated 1949) — a faithful answer surfaces the contradiction rather
		// than silently picking one. Both pages support; abstention is not required.
		caseRecord("ask", "ask-0003", "contradiction_surface",
			askIn{Question: "When was Acme Corporation founded?", WikiIDs: univ},
			askGoldRec{
				Answer:     "Sources disagree: one page says Acme was founded in 1947, another says it was incorporated in 1949.",
				Supporting: []string{acme, acmeHist},
				Spans:      map[string]string{acme: "founded in 1947", acmeHist: "incorporated"},
				PageBodies: bodyMap(acme, acmeHist),
			}),
	}
}

// --- search ---
//
// Input: a query + the synthetic-wiki id space. Gold: retrievalGold
// ({relevant:[page id...]}). failure_tag: blunt_keyword, semantic_no_keyword (a
// query whose relevant page shares no surface keyword — the hybrid lane's reason
// to exist), missed_relevant.

func searchCases() []eval.Case {
	acme := "01HXWIKIPAGEACME00000000001"
	acmeHist := "01HXWIKIPAGEACMEHIST0000002"
	zephyr := "01HXWIKIPAGEZEPHYR000000003"
	orchid := "01HXWIKIPAGEORCHID000000004"
	univ := allWikiIDs()
	_ = univ

	return []eval.Case{
		// BLUNT keyword: an exact term -> its page ranks first.
		caseRecord("search", "search-0001", "blunt_keyword",
			retrievalIn{Query: "Zephyr Airlines", Corpus: univ},
			retrievalGoldRec{Relevant: []string{zephyr}}),

		// SUBTLE: "Acme" matches BOTH Acme pages — both are relevant.
		caseRecord("search", "search-0002", "missed_relevant",
			retrievalIn{Query: "Acme Corporation history", Corpus: univ},
			retrievalGoldRec{Relevant: []string{acmeHist, acme}}),

		// SUBTLE semantic: a query with NO surface-keyword overlap with the relevant
		// page ("houseplant care" vs the orchid page) — the case the hybrid vector
		// lane exists to win; a lexical-only lane is expected to miss it.
		caseRecord("search", "search-0003", "semantic_no_keyword",
			retrievalIn{Query: "how to care for houseplants", Corpus: univ},
			retrievalGoldRec{Relevant: []string{orchid}}),
	}
}
