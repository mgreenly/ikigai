// Package config is wiki's composition-root configuration: env → typed config,
// validated at the serve boundary and injected inward. Its load-bearing job is
// the per-call-site seam of design §10 — "every LLM call site takes its prompt,
// model, and effort from injected config, never from constants." No call site
// ever reads env or a constant; it is handed a CallSite triple resolved here.
//
// The eight config-injected LLM sites of design §10 are: extract, match, merge,
// compile, ask, and the three lint calls (dup judge, fold, stale repair). Each
// carries its own {prompt, model, effort}. The canonical-name pick and the three
// retrieval lanes are scoreable by the eval harness but are NOT separate
// config-injected sites (they ride another site's triple or carry no LLM) — they
// live in the call-site Registry (registry.go), the harness's superset checklist.
//
// Defaults here MAY be placeholders — each call-site phase (P6a onward) fills in
// its real default prompt. The validation that runs at serve is structural: a
// pinned model resolves to a known provider and accepts its effort.
package config

import (
	"fmt"
	"strconv"
	"strings"

	"agentkit/model"
)

// CallSite is the injected triple every LLM call site receives (design §10).
// The prompt, model, and effort are resolved here at the composition root and
// threaded to the site; the site never reads env or hard-codes any of the three.
// This is the unit the eval harness swaps to sweep {model × effort × prompt}.
type CallSite struct {
	// Name is the stable site identifier (e.g. "extract"); it matches a
	// registry.go entry and is used as the accounting log's call_site field.
	Name string
	// Prompt is the site's system/framing prompt. MAY be a placeholder in P2;
	// each call-site phase fills its real default.
	Prompt string
	// Model is the bare/aliased model id (e.g. "claude-sonnet-4-6", "gpt-5.5").
	Model string
	// Effort is the reasoning effort level this site runs at ("", "low",
	// "medium", "high", "xhigh", "max"); validated against the model's vocabulary.
	Effort string
}

// Validate confirms the site's model resolves to a known provider+model and
// accepts its effort. It does NOT assert the prompt is non-placeholder — that is
// the per-phase prompt-default gate's job (a call-site phase concern), not the
// scaffold's.
func (c CallSite) Validate() error {
	r, err := model.Resolve(c.Model)
	if err != nil {
		return fmt.Errorf("call site %q: %w", c.Name, err)
	}
	if err := model.Validate(r); err != nil {
		return fmt.Errorf("call site %q: %w", c.Name, err)
	}
	if c.Effort != "" {
		if err := model.ValidateEffort(r, c.Effort); err != nil {
			return fmt.Errorf("call site %q: %w", c.Name, err)
		}
	}
	return nil
}

// LLM holds the eight config-injected call sites of design §10. Each field is a
// CallSite; nothing else may inject a model into a site.
type LLM struct {
	Extract      CallSite
	Match        CallSite
	Merge        CallSite
	Compile      CallSite
	Ask          CallSite
	LintDupJudge CallSite // the dup judge (also emits the canonical-name pick, §6)
	LintFold     CallSite // the fold
	LintStale    CallSite // the stale repair
}

// sites returns the eight call sites in a stable order for iteration.
func (l *LLM) sites() []*CallSite {
	return []*CallSite{
		&l.Extract, &l.Match, &l.Merge, &l.Compile, &l.Ask,
		&l.LintDupJudge, &l.LintFold, &l.LintStale,
	}
}

// Validate validates every config-injected site.
func (l *LLM) Validate() error {
	for _, s := range l.sites() {
		if err := s.Validate(); err != nil {
			return err
		}
	}
	return nil
}

// Embed is the embeddings model/dims config (the lone non-chat AI knob, used by
// P11's vector lane). Kept beside LLM because it is the same composition-root
// config-injection discipline applied to the embed call.
type Embed struct {
	Model string // e.g. "text-embedding-3-large"
	Dims  int    // configurable output dimensionality
}

// Config is wiki's whole service configuration, built at the serve boundary.
type Config struct {
	// Owner is WIKI_OWNER — the box owner autonomously-ingested dropbox files are
	// filed under (must match the X-Owner-Email the dashboard injects).
	Owner string

	// InboxInlineMax is WIKI_INBOX_INLINE_MAX (default 4096): payloads at or below
	// this size are stored inline; larger ones spill to content-addressed blobs.
	InboxInlineMax int
	// IngestMaxBytes is WIKI_INGEST_MAX_BYTES (default 131072): the hard door cap;
	// a larger ingest is refused loudly (emits wiki.ingest_refused).
	IngestMaxBytes int64
	// IntegrationWorkers is WIKI_INTEGRATION_WORKERS (default 4): the worker pool.
	IntegrationWorkers int
	// RunAttemptsMax is WIKI_RUN_ATTEMPTS_MAX (default 5): the bounded-retry
	// threshold at which a failing inbox row dead-letters (design §7).
	RunAttemptsMax int

	// CandidateLimit is WIKI_CANDIDATE_LIMIT (default 5): the per-query FTS
	// candidate shortlist size resolution's zero-ids arm builds (design §4.3 "top
	// ~5"). It is an eval-harness knob (obligation 2) — a tunable, never a constant
	// at the call site — fed to integrate.NewResolver.
	CandidateLimit int

	// MatchExcerptChars is WIKI_MATCH_EXCERPT_CHARS (default 600): the number of
	// leading page-body characters included in each candidate's match excerpt
	// (design §4.3). It is an eval-harness knob (obligation 2) — a tunable, never a
	// constant at the call site — fed to integrate's manifest assembler.
	MatchExcerptChars int

	// LLM is the per-call-site injection seam (design §10).
	LLM LLM
	// Embed is the embeddings model/dims (P11's vector lane).
	Embed Embed
}

// defaults are the placeholder triples P2 ships. Each call-site phase replaces
// its site's prompt with the real default; the model/effort picks here are the
// provisional production triples (design §10 "exact models per call site"),
// overridable per the WIKI_<SITE>_{PROMPT,MODEL,EFFORT} env keys below.
//
// The placeholder prompt is intentionally a clearly-marked stub so the per-phase
// prompt-default gate (a call-site phase concern) can detect it as not-yet-real.
const placeholderPrompt = "PLACEHOLDER — real default prompt lands in this call site's phase."

// Load builds a Config from getenv (the appkit Spec.Config hook shape). It reads
// every knob, applies defaults, then validates the LLM sites at the serve
// boundary so a wrong/renamed model or a rejected effort fails startup loudly
// rather than at first call.
func Load(getenv func(string) string) (*Config, error) {
	cfg := &Config{
		Owner:              getenv("WIKI_OWNER"),
		InboxInlineMax:     envInt(getenv, "WIKI_INBOX_INLINE_MAX", 4096),
		IngestMaxBytes:     int64(envInt(getenv, "WIKI_INGEST_MAX_BYTES", 131072)),
		IntegrationWorkers: envInt(getenv, "WIKI_INTEGRATION_WORKERS", 4),
		RunAttemptsMax:     envInt(getenv, "WIKI_RUN_ATTEMPTS_MAX", 5),
		CandidateLimit:     envInt(getenv, "WIKI_CANDIDATE_LIMIT", 5),
		MatchExcerptChars:  envInt(getenv, "WIKI_MATCH_EXCERPT_CHARS", 600),
		Embed: Embed{
			Model: envStr(getenv, "WIKI_EMBED_MODEL", "text-embedding-3-large"),
			Dims:  envInt(getenv, "WIKI_EMBED_DIMS", 1024),
		},
	}

	// Each site: name, env-prefix, provisional model+effort default, and the
	// config-default prompt. A site whose owning phase has not yet landed uses the
	// shared placeholder; a landed site (extract, P6a) passes its real default.
	cfg.LLM.Extract = loadSite(getenv, "extract", "WIKI_EXTRACT", "claude-sonnet-4-6", "medium", DefaultExtractPrompt)
	cfg.LLM.Match = loadSite(getenv, "match", "WIKI_MATCH", "claude-haiku-4-5", "", DefaultMatchPrompt)
	cfg.LLM.Merge = loadSite(getenv, "merge", "WIKI_MERGE", "claude-sonnet-4-6", "high", DefaultMergePrompt)
	cfg.LLM.Compile = loadSite(getenv, "compile", "WIKI_COMPILE", "claude-sonnet-4-6", "medium", DefaultCompilePrompt)
	cfg.LLM.Ask = loadSite(getenv, "ask", "WIKI_ASK", "claude-sonnet-4-6", "medium", placeholderPrompt)
	cfg.LLM.LintDupJudge = loadSite(getenv, "lint_dup_judge", "WIKI_LINT_DUP", "claude-sonnet-4-6", "medium", placeholderPrompt)
	cfg.LLM.LintFold = loadSite(getenv, "lint_fold", "WIKI_LINT_FOLD", "claude-sonnet-4-6", "medium", placeholderPrompt)
	cfg.LLM.LintStale = loadSite(getenv, "lint_stale", "WIKI_LINT_STALE", "claude-sonnet-4-6", "medium", placeholderPrompt)

	if err := cfg.LLM.Validate(); err != nil {
		return nil, err
	}
	return cfg, nil
}

// loadSite resolves one call site's triple from its WIKI_<PREFIX>_{PROMPT,MODEL,
// EFFORT} keys, falling back to the provisional model/effort defaults and the
// per-site config-default prompt (the placeholder until the owning phase lands).
func loadSite(getenv func(string) string, name, prefix, defModel, defEffort, defPrompt string) CallSite {
	return CallSite{
		Name:   name,
		Prompt: envStr(getenv, prefix+"_PROMPT", defPrompt),
		Model:  envStr(getenv, prefix+"_MODEL", defModel),
		Effort: envStr(getenv, prefix+"_EFFORT", defEffort),
	}
}

func envStr(getenv func(string) string, key, def string) string {
	if v := strings.TrimSpace(getenv(key)); v != "" {
		return v
	}
	return def
}

func envInt(getenv func(string) string, key string, def int) int {
	v := strings.TrimSpace(getenv(key))
	if v == "" {
		return def
	}
	n, err := strconv.Atoi(v)
	if err != nil {
		return def
	}
	return n
}
