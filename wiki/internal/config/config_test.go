package config

import (
	"reflect"
	"sort"
	"testing"
)

// getenvFrom builds a getenv closure over a map.
func getenvFrom(m map[string]string) func(string) string {
	return func(k string) string { return m[k] }
}

// TestLoadDefaults: an empty env yields the provisional defaults and a valid,
// startup-passing config (every call-site triple resolves to a known model that
// accepts its effort).
func TestLoadDefaults(t *testing.T) {
	cfg, err := Load(getenvFrom(nil))
	if err != nil {
		t.Fatalf("Load with defaults must validate: %v", err)
	}
	if cfg.InboxInlineMax != 4096 {
		t.Errorf("InboxInlineMax = %d, want 4096", cfg.InboxInlineMax)
	}
	if cfg.IngestMaxBytes != 131072 {
		t.Errorf("IngestMaxBytes = %d, want 131072", cfg.IngestMaxBytes)
	}
	if cfg.IntegrationWorkers != 4 {
		t.Errorf("IntegrationWorkers = %d, want 4", cfg.IntegrationWorkers)
	}
	if cfg.Embed.Model != "text-embedding-3-large" {
		t.Errorf("Embed.Model = %q", cfg.Embed.Model)
	}
	if cfg.MatchExcerptChars != 600 {
		t.Errorf("MatchExcerptChars = %d, want 600", cfg.MatchExcerptChars)
	}
	// Sites whose owning phase has not yet landed carry a placeholder prompt; a
	// landed site carries its real config-default prompt (extract — P6a; match — P6b2;
	// merge — P7a2; compile — P8).
	realDefaults := map[string]string{
		"extract": DefaultExtractPrompt,
		"match":   DefaultMatchPrompt,
		"merge":   DefaultMergePrompt,
		"compile": DefaultCompilePrompt,
	}
	for _, s := range cfg.LLM.sites() {
		if want, ok := realDefaults[s.Name]; ok {
			if s.Prompt != want {
				t.Errorf("site %q: prompt should default to its real config-default, got %q", s.Name, s.Prompt)
			}
		} else if s.Prompt != placeholderPrompt {
			t.Errorf("site %q: prompt should default to placeholder, got %q", s.Name, s.Prompt)
		}
		if s.Model == "" {
			t.Errorf("site %q: model must not be empty", s.Name)
		}
	}
}

// TestLoadEnvOverrides: a site's WIKI_<SITE>_{PROMPT,MODEL,EFFORT} keys override
// the defaults — the config-injection seam is env-driven, never a constant.
func TestLoadEnvOverrides(t *testing.T) {
	cfg, err := Load(getenvFrom(map[string]string{
		"WIKI_EXTRACT_PROMPT":      "custom extract prompt",
		"WIKI_EXTRACT_MODEL":       "gpt-5.5",
		"WIKI_EXTRACT_EFFORT":      "high",
		"WIKI_INTEGRATION_WORKERS": "8",
		"WIKI_MATCH_EXCERPT_CHARS": "1200",
	}))
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	if cfg.LLM.Extract.Prompt != "custom extract prompt" {
		t.Errorf("extract prompt not overridden: %q", cfg.LLM.Extract.Prompt)
	}
	if cfg.LLM.Extract.Model != "gpt-5.5" {
		t.Errorf("extract model not overridden: %q", cfg.LLM.Extract.Model)
	}
	if cfg.LLM.Extract.Effort != "high" {
		t.Errorf("extract effort not overridden: %q", cfg.LLM.Extract.Effort)
	}
	if cfg.IntegrationWorkers != 8 {
		t.Errorf("workers not overridden: %d", cfg.IntegrationWorkers)
	}
	if cfg.MatchExcerptChars != 1200 {
		t.Errorf("MatchExcerptChars not overridden: %d", cfg.MatchExcerptChars)
	}
}

// TestValidateRejectsUnknownModel: a wrong/renamed model fails validation at the
// serve boundary, not at first call.
func TestValidateRejectsUnknownModel(t *testing.T) {
	_, err := Load(getenvFrom(map[string]string{
		"WIKI_MERGE_MODEL": "claude-does-not-exist-9",
	}))
	if err == nil {
		t.Fatal("an unknown model must fail Load validation")
	}
}

// TestValidateRejectsBadEffort: an effort the model does not accept fails.
func TestValidateRejectsBadEffort(t *testing.T) {
	_, err := Load(getenvFrom(map[string]string{
		// haiku accepts no effort vocabulary.
		"WIKI_MATCH_MODEL":  "claude-haiku-4-5",
		"WIKI_MATCH_EFFORT": "high",
	}))
	if err == nil {
		t.Fatal("a rejected effort must fail Load validation")
	}
}

// TestRegistryMatchesLLMFields: the call-site Registry's injected sites must be
// exactly the eight CallSite names the LLM struct carries — the two sources of
// truth cannot drift.
func TestRegistryMatchesLLMFields(t *testing.T) {
	cfg, err := Load(getenvFrom(nil))
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	var llmNames []string
	for _, s := range cfg.LLM.sites() {
		llmNames = append(llmNames, s.Name)
	}
	registryInjected := InjectedSites()

	sort.Strings(llmNames)
	sort.Strings(registryInjected)
	if !reflect.DeepEqual(llmNames, registryInjected) {
		t.Errorf("LLM fields %v != registry injected sites %v", llmNames, registryInjected)
	}
}

// TestRegistryHasTenSites: the harness-callable registry is the ten-site
// superset (research §"inference inventory"), with each site naming its phase.
func TestRegistryHasTenSites(t *testing.T) {
	wantInjected := 8 // the eight config-injected LLM sites
	gotInjected := 0
	seen := map[string]bool{}
	for _, s := range Registry {
		if seen[s.Name] {
			t.Errorf("duplicate registry site %q", s.Name)
		}
		seen[s.Name] = true
		if s.Phase == "" {
			t.Errorf("site %q has no landing phase", s.Name)
		}
		if s.Injected {
			gotInjected++
		}
	}
	if gotInjected != wantInjected {
		t.Errorf("injected sites = %d, want %d", gotInjected, wantInjected)
	}
	// canonical_name_pick must ride the dup judge (design §6).
	for _, s := range Registry {
		if s.Name == "canonical_name_pick" {
			if s.Injected {
				t.Error("canonical_name_pick must not be a config-injected site")
			}
			if s.RidesOn != "lint_dup_judge" {
				t.Errorf("canonical_name_pick rides %q, want lint_dup_judge", s.RidesOn)
			}
		}
	}
}
