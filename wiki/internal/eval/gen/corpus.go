// Package gen builds the per-site evaluation test sets (plan P15, design
// docs/wiki-evaluation-design.md). It is Part II's data-authoring half: one
// generator per inference site emits a versioned (dataset + prompt) bundle in the
// q3 storage layout (testsets/<site>/{datasets,prompts,bundles}/), with the
// shared synthetic corpora as the structural saving — one identity corpus feeds
// match + dup_judge + candidates + sweep, and one synthetic wiki feeds ask +
// search (eval design "Shared corpora", "What P13-P16 inherit").
//
// The goldens are synthetic-FIRST (eval design q1: synthetic authorship is the
// spine for every site; real data only validates, never seeds — and the live int
// box that would supply that real anchor does not yet exist, so gen-1 is the
// documented synthetic path). Each generator spans BLUNT -> SUBTLE case shapes;
// generations are the only escalation mechanism, so every gen-1 case carries a
// per-site failure_tag drawn from an enumerated dangerous-direction vocabulary,
// NOT a difficulty label (eval design "Dataset record format").
//
// The committed bundles under wiki/testsets/ ARE the deliverable; this package
// produces them deterministically (cmd/wiki-eval-gen writes them) and the
// adversarial verification pass (gen_test.go) re-scores every gen-1 case against
// its own gold to prove the dataset is internally consistent and that each
// dangerous case actually exercises its tagged axis — the redesign's own
// LLM-author-then-adversarial-verify pattern, made a deterministic offline check.
package gen

// The shared synthetic IDENTITY CORPUS (eval design "Shared corpora": one truth,
// four consumers — match, dup_judge, candidates, sweep). Each entry is one
// existing subject in a hypothetical knowledge base, with a stable ULID-shaped id,
// its canonical name, aliases, type/kind, and a prose body. The corpus is
// authored to contain the classic identity traps: same-name-different-thing
// (Apple Inc. vs Apple Records, Mercury the planet vs Mercury the element vs
// Freddie Mercury), and different-name-same-thing (Big Blue == IBM).

// idSubject is one subject in the shared identity corpus.
type idSubject struct {
	ID            string
	Type          string
	Kind          string
	CanonicalName string
	Aliases       []string
	Body          string
}

// identityCorpus is the single shared truth the four identity-consuming sites
// draw from. IDs are stable across generations (eval design q3: a case_id /
// subject survives generations so it can be tracked / refreshed).
var identityCorpus = []idSubject{
	{
		ID: "01HXIDCORPUSAPPLEINC0000001", Type: "entity", Kind: "org",
		CanonicalName: "Apple Inc.", Aliases: []string{"apple", "apple inc", "apple computer"},
		Body: "Apple Inc. is a technology company headquartered in Cupertino, California. " +
			"It designs the iPhone, the Mac, and the iPad. Tim Cook is its chief executive officer.",
	},
	{
		ID: "01HXIDCORPUSAPPLEREC0000002", Type: "entity", Kind: "org",
		CanonicalName: "Apple Records", Aliases: []string{"apple records", "apple corps"},
		Body: "Apple Records is a record label founded by the Beatles in 1968 in London. " +
			"It released albums by the Beatles and James Taylor.",
	},
	{
		ID: "01HXIDCORPUSIBM000000000003", Type: "entity", Kind: "org",
		CanonicalName: "IBM", Aliases: []string{"ibm", "international business machines", "big blue"},
		Body: "IBM, International Business Machines Corporation, is a technology and consulting " +
			"company nicknamed Big Blue. It is headquartered in Armonk, New York.",
	},
	{
		ID: "01HXIDCORPUSMERCURYPLT00004", Type: "entity", Kind: "thing",
		CanonicalName: "Mercury (planet)", Aliases: []string{"mercury", "planet mercury"},
		Body: "Mercury is the smallest planet in the Solar System and the closest to the Sun. " +
			"It has no natural satellites and a heavily cratered surface.",
	},
	{
		ID: "01HXIDCORPUSMERCURYELE00005", Type: "entity", Kind: "thing",
		CanonicalName: "Mercury (element)", Aliases: []string{"mercury", "quicksilver", "hg"},
		Body: "Mercury is a chemical element with the symbol Hg and atomic number 80. " +
			"It is the only metal that is liquid at standard temperature and pressure.",
	},
	{
		ID: "01HXIDCORPUSFREDMERCURY0006", Type: "entity", Kind: "person",
		CanonicalName: "Freddie Mercury", Aliases: []string{"freddie mercury", "farrokh bulsara"},
		Body: "Freddie Mercury was the lead vocalist of the rock band Queen. " +
			"He was born Farrokh Bulsara and was known for his four-octave vocal range.",
	},
	{
		ID: "01HXIDCORPUSTIMCOOK00000007", Type: "entity", Kind: "person",
		CanonicalName: "Tim Cook", Aliases: []string{"tim cook", "timothy cook"},
		Body: "Tim Cook is the chief executive officer of Apple Inc. " +
			"He succeeded Steve Jobs as CEO in 2011.",
	},
}

// byID returns the corpus subject with the given id (the generators index into the
// shared truth by id so the gold ids stay consistent across consumers).
func byID(id string) idSubject {
	for _, s := range identityCorpus {
		if s.ID == id {
			return s
		}
	}
	panic("gen: unknown identity-corpus id " + id)
}

// The shared synthetic WIKI (eval design "Shared corpora": one synthetic wiki of
// pages with known facts, gaps, and contradictions -> ask + search). Each page is
// a prose knowledge-base page with a stable id; the ask/search generators author
// questions whose gold supporting set points into these pages, plus GAP questions
// whose answer is deliberately absent (the abstention / fabrication test).

// wikiPage is one page in the shared synthetic wiki.
type wikiPage struct {
	ID    string
	Title string
	Body  string
}

// syntheticWiki is the single shared page set ask + search draw from. It carries
// known facts (answerable), a deliberate GAP (no page states the CEO's salary), and
// a CONTRADICTION (two pages disagree on a founding year) so the ask scorer's
// contradiction-surfacing and abstention axes have real material.
var syntheticWiki = []wikiPage{
	{
		ID: "01HXWIKIPAGEACME00000000001", Title: "Acme Corporation",
		Body: "Acme Corporation [01HXWIKIINBOXACME0000000001] is a manufacturing company " +
			"founded in 1947 in Toledo, Ohio. Its current chief executive is Dana Reyes " +
			"[01HXWIKIINBOXACME0000000002]. Acme makes industrial fasteners and hydraulic presses.",
	},
	{
		ID: "01HXWIKIPAGEACMEHIST0000002", Title: "History of Acme Corporation",
		Body: "Records suggest Acme Corporation [01HXWIKIINBOXACMEHIST00001] was incorporated " +
			"in 1949, two years after it began trading. The company expanded to Detroit in 1960.",
	},
	{
		ID: "01HXWIKIPAGEZEPHYR000000003", Title: "Zephyr Airlines",
		Body: "Zephyr Airlines [01HXWIKIINBOXZEPHYR000001] is a regional carrier based in Denver. " +
			"It operates 14 routes across the Rocky Mountain region and was founded in 2003.",
	},
	{
		ID: "01HXWIKIPAGEORCHID000000004", Title: "Orchid cultivation",
		Body: "Orchids [01HXWIKIINBOXORCHID000001] are flowering plants that prefer indirect light " +
			"and well-draining bark medium. Most household orchids are Phalaenopsis hybrids.",
	},
}

// wikiPageByID returns the synthetic-wiki page with the given id.
func wikiPageByID(id string) wikiPage {
	for _, p := range syntheticWiki {
		if p.ID == id {
			return p
		}
	}
	panic("gen: unknown synthetic-wiki page id " + id)
}
