package gen

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"

	"wiki/internal/config"
	"wiki/internal/eval"
)

// Generation 1 — the first, deliberately blunt-leaning generation (eval design:
// later generations weight toward subtle; gen-1 establishes the regression floor).
const gen1 = 1

// promptArtifact is the prompt file a site's bundle names. For the LLM call sites
// it is the config-default production prompt (eval design q3: "the production
// prompt is just a prompt artifact" — evaluate-production and evaluate-a-candidate
// are the same operation against different bundles). For the retrieval lanes
// (candidates / search / sweep), whose "prompt" is degenerate (they are FTS/vector
// queries, not an LLM system prompt), the artifact is a thin descriptor so the
// bundle still names one and the layout is uniform.
type promptArtifact struct {
	name string // the file name under prompts/ (e.g. "v1.txt")
	body string // the prompt text
}

// siteGenerator authors one site's gen-1 dataset and names its prompt artifact.
type siteGenerator struct {
	site   string
	prompt promptArtifact
	cases  func() []eval.Case
}

// Generators is the canonical per-site generator registry (eval design "P15 builds
// one generator per site"). Order is the ten registry sites; the four
// identity-corpus consumers and the two synthetic-wiki consumers share their
// corpus via the gen package's shared vars, which is the structural saving.
func Generators() []siteGenerator {
	return []siteGenerator{
		{site: "extract", prompt: prod("v1.txt", config.DefaultExtractPrompt), cases: extractCases},
		{site: "compile", prompt: prod("v1.txt", config.DefaultCompilePrompt), cases: compileCases},
		{site: "match", prompt: prod("v1.txt", config.DefaultMatchPrompt), cases: matchCases},
		{site: "dup_judge", prompt: prod("v1.txt", config.DefaultLintDupJudgePrompt), cases: dupJudgeCases},
		{site: "canonical_name", prompt: prod("v1.txt", defaultCanonicalNamePrompt), cases: canonicalNameCases},
		{site: "candidates", prompt: retrievalPrompt("candidates"), cases: candidatesCases},
		{site: "search", prompt: retrievalPrompt("search"), cases: searchCases},
		{site: "sweep", prompt: retrievalPrompt("sweep"), cases: sweepCases},
		{site: "merge", prompt: prod("v1.txt", config.DefaultMergePrompt), cases: mergeCases},
		{site: "ask", prompt: prod("v1.txt", config.DefaultAskPrompt), cases: askCases},
	}
}

func prod(name, body string) promptArtifact { return promptArtifact{name: name, body: body} }

// retrievalPrompt is the thin descriptor artifact for a degenerate retrieval lane
// (no LLM system prompt — the "prompt" the bundle pins is the lane name plus the
// note that the real swept knobs are config values: FTS thresholds, RRF k, embed
// model/dims — eval obligation 2).
func retrievalPrompt(lane string) promptArtifact {
	return promptArtifact{
		name: "v1.txt",
		body: "retrieval lane: " + lane + "\n" +
			"This lane is not LLM-prompted; the swept knobs are config values\n" +
			"(candidate FTS thresholds, per-lane sweep thresholds, RRF k, embed model/dims).\n",
	}
}

// defaultCanonicalNamePrompt is a thin authored prompt for the low-stakes
// canonical-name pick (no config default exists yet — it is a deferred Open Item).
// It carries the convention the agreement scorer checks: prefer the most complete,
// formal canonical form.
const defaultCanonicalNamePrompt = `You pick the single canonical display name for a subject given its observed
name variants. Return ONLY JSON {"name": "..."}.

Convention: prefer the most complete, formal form (full legal/proper name over an
abbreviation or lowercase variant); preserve the subject's own preferred casing;
do not invent a name not present among the variants.
`

// caseRecord builds one eval.Case with site/generation set and input/gold marshaled
// to json.RawMessage (the loader returns them raw; each scorer unmarshals its own
// shape).
func caseRecord(site, caseID, failureTag string, input, gold any) eval.Case {
	return eval.Case{
		CaseID:     caseID,
		Site:       site,
		Generation: gen1,
		FailureTag: failureTag,
		Input:      mustJSON(input),
		Gold:       mustJSON(gold),
	}
}

func mustJSON(v any) json.RawMessage {
	b, err := json.Marshal(v)
	if err != nil {
		panic(fmt.Sprintf("gen: marshal: %v", err))
	}
	return b
}

// WriteAll writes every site's gen-1 bundle into the testsets tree rooted at root
// (e.g. "wiki/testsets"), in the eval-design q3 layout:
//
//	testsets/<site>/datasets/gen-1.json   (the dataset records)
//	testsets/<site>/prompts/v1.txt        (the prompt artifact)
//	testsets/<site>/bundles/gen-1.json    ({dataset, prompt})
//
// It is idempotent: re-running overwrites with byte-identical content (the
// generators are deterministic), so the committed bundles are reproducible.
func WriteAll(root string) error {
	for _, g := range Generators() {
		if err := writeSite(root, g); err != nil {
			return fmt.Errorf("gen %s: %w", g.site, err)
		}
	}
	return nil
}

func writeSite(root string, g siteGenerator) error {
	siteDir := filepath.Join(root, g.site)
	for _, sub := range []string{"datasets", "prompts", "bundles"} {
		if err := os.MkdirAll(filepath.Join(siteDir, sub), 0o755); err != nil {
			return err
		}
	}

	// Dataset (pretty-printed JSON array so the committed artifact is reviewable).
	ds, err := json.MarshalIndent(g.cases(), "", "  ")
	if err != nil {
		return err
	}
	ds = append(ds, '\n')
	if err := os.WriteFile(filepath.Join(siteDir, "datasets", "gen-1.json"), ds, 0o644); err != nil {
		return err
	}

	// Prompt artifact.
	if err := os.WriteFile(filepath.Join(siteDir, "prompts", g.prompt.name), []byte(g.prompt.body), 0o644); err != nil {
		return err
	}

	// Bundle naming the dataset + the prompt (eval design q3).
	b := eval.Bundle{
		Dataset: "datasets/gen-1.json",
		Prompt:  "prompts/" + g.prompt.name,
	}
	bb, err := json.MarshalIndent(b, "", "  ")
	if err != nil {
		return err
	}
	bb = append(bb, '\n')
	return os.WriteFile(filepath.Join(siteDir, "bundles", "gen-1.json"), bb, 0o644)
}

// SiteNames returns the generated site names in registry order (used by the
// adversarial-verification test to assert every site has a generator).
func SiteNames() []string {
	gs := Generators()
	out := make([]string, len(gs))
	for i, g := range gs {
		out[i] = g.site
	}
	sort.Strings(out)
	return out
}
