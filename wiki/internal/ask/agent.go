// Package ask is the wiki's agentic synthesis pass (Task 6.2 — wiki_ask). Like
// ingest (Task 4.1) and lint (Task 5.2) it reuses the exact agent/job machinery —
// the agentkit provider client, the agent tool-use loop, the agentkit/job runner,
// and the wiki jobstore — and rides the SAME async lifecycle (returns a job id;
// poll it with wiki_job_status). It differs from ingest/lint only in three things:
//
//  1. its TOOLSET is READ-ONLY for navigation (read + glob + grep), plus the ONE
//     write path needed to FILE the synthesized answer back as a synthesis page
//     (write under synthesis/). It has NO edit and NO bash — wiki_ask reads the
//     curated tree and reasons; it never amends existing pages or shells out;
//  2. its SYSTEM PROMPT is the ask/synthesis framing (index-first navigation →
//     read pages → synthesize a CITED answer → file it back as a synthesis page),
//     not the integration- or lint-pass framing; and
//  3. it operates over the EXISTING page tree — there is no new raw doc, no
//     provenance stamp, no wiki_ingest row. Its only write is the synthesis page
//     it files back so explorations COMPOUND (a subsequent wiki_search finds it).
//
// Single-flight: wiki_ask WRITES (it files a synthesis page and re-indexes), so it
// shares ingest's per-(owner, collection) flight key (ingest.FlightKey). An ask
// while an ingest or lint runs — or vice-versa — is rejected with
// job.ErrFlightInUse: only one write-pass touches a given wiki at a time.
package ask

import (
	"agentkit/provider"
	"agentkit/tools/glob"
	"agentkit/tools/grep"
	"agentkit/tools/read"
	"agentkit/tools/write"

	wikischema "wiki/internal/store/schema"
)

// DefaultModel is the ask agent's model when WIKI_ASK_MODEL is unset. It mirrors
// the ingest/lint default (a mid-tier model with a per-job cost ceiling from
// config, not hardcoded — PLAN Decision 3); main.go threads the resolved value
// here via the shared Config.
const DefaultModel = "claude-sonnet-4-6"

// DefaultMaxTokens is the per-job output-token ceiling when WIKI_ASK_MAX_TOKENS
// is unset — the cost knob (PLAN Decision 3), a default rather than a hardcoded
// constant in the agent. main.go overrides it from env.
const DefaultMaxTokens = 8192

// askToolset is the surface the ask agent is allowed: read (consult index.md +
// pages), glob+grep (index-first DISCOVERY — find the pages relevant to the
// question across the tree) and write (file the synthesized answer back as ONE
// synthesis page under synthesis/). NO edit — ask must not amend existing curated
// pages (it only files a NEW synthesis page); NO bash — ask never shells out, and
// the smaller surface is the security floor before OS-level confinement (Phase 7).
// All file paths are confined to the owner+collection root via the agent loop's
// sandboxRoot argument; glob/grep are confined the same way.
func askToolset() []provider.Tool {
	return []provider.Tool{
		{Name: read.Name, InputSchema: read.InputSchema},
		{Name: glob.Name, InputSchema: glob.InputSchema},
		{Name: grep.Name, InputSchema: grep.InputSchema},
		{Name: write.Name, InputSchema: write.InputSchema},
	}
}

// systemPrompt builds the ask agent's system prompt: the wiki's embedded schema
// doc (the type set, frontmatter conventions, index-first navigation, the four
// invariants — SCHEMA.md) followed by the ask/synthesis instructions. The schema
// doc is the single source of truth for the conventions; this function only
// appends the per-run framing.
func systemPrompt() string {
	return wikischema.Doc() + "\n\n" + askInstructions
}

// askInstructions is the ask/synthesis framing appended to the schema doc. It
// states the GOALS query job (index-first navigation → read pages → synthesize a
// CITED answer → file it back as a synthesis page so explorations compound) in
// operational terms and binds it to the four invariants: ask READS and SYNTHESIZES
// — it never mutates an existing page, and its one write is a NEW synthesis page.
// Kept tight and faithful to SCHEMA.md / GOALS (the agent has already read the
// schema above).
const askInstructions = `## Your task right now: answer a question from this wiki

A user has asked a question. Answer it using ONLY what this wiki already contains,
then file your answer back so future questions compound. All your file paths are
RELATIVE to the wiki's collection root — you are already confined there; never use
absolute paths or ` + "`..`" + `.

Work in this order:

1. READ ` + "`index.md`" + ` first to orient (the catalog / navigation entry point).
   It may not exist yet — that's fine.
2. NAVIGATE index-first to the relevant pages. Use Glob (e.g.
   ` + "`concepts/*.md`, `entities/*.md`, `**/*.md`" + `) to list pages and Grep to
   locate the ones that bear on the question, then READ them. Prefer the curated
   pages; you do not need to open ` + "`raw/`" + `.
3. SYNTHESIZE a direct, accurate answer from what you read. Every claim must be
   CITED: reference the wiki page(s) it came from (by their relative path, e.g.
   ` + "`concepts/otters.md`" + `). If the wiki does not contain enough to answer,
   say so plainly rather than inventing — answer ONLY from the wiki.
4. FILE your answer back as a NEW ` + "`synthesis`" + ` page under
   ` + "`synthesis/`" + ` (the compounding artifact). Give it a short, descriptive
   slug filename (e.g. ` + "`synthesis/what-are-otters.md`" + `). Open it with
   ` + "`---`" + `-fenced frontmatter: ` + "`type: synthesis`" + `, a ` + "`title:`" + `,
   a ` + "`collection:`" + `, and a ` + "`source:`" + ` that lists the pages you cited.
   The body is the cited answer.
5. Finish your turn with the answer itself as plain text (the same synthesized,
   cited answer you filed) so the caller receives it directly.

Honor the four invariants WITHOUT EXCEPTION — ask reads and synthesizes; it does
NOT mutate existing knowledge:
- Provenance: cite every page your answer draws on; the synthesis page records
  those pages in its ` + "`source:`" + ` frontmatter.
- Immutable raw: NEVER write into ` + "`raw/`" + `.
- Flag, don't overwrite: do not edit or clobber any existing curated page — your
  only write is the new synthesis page. If pages contradict, say so in the answer.
- Append, don't destroy: filing a synthesis page only ADDS; it supersedes nothing.

Do not ask the user clarifying questions — this runs unattended. Answer from the
wiki, file the synthesis page, and finish with the answer.`

// userMessage is the per-run user turn handed to the ask agent: it carries the
// question and names the scope. The schema doc + askInstructions in the system
// prompt carry the how (index-first navigation, cite, file back).
func userMessage(owner, collection, question string) string {
	return "Answer this question from the wiki (owner " + owner + ", collection " +
		collection + "), citing the pages you use, then file your answer back as a " +
		"synthesis page under synthesis/.\n\nQuestion: " + question
}
