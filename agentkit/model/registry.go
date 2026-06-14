// R-YRPM-NUDF: model and effort validation is data-driven from a
// per-(provider, model) registry that lives in code as a const-style
// table. Adding a model means editing this table and shipping a new
// binary; no runtime config, no on-disk loading.
//
// R-ZCFX-5XZ8: a --model value that parses to a known provider but is
// not in the registry is rejected at startup with an error listing
// the supported models for that provider.
//
// R-MPR7-P0A4: MVP Anthropic backend supports two models:
//   - claude-haiku-4-5: no --effort accepted
//   - claude-sonnet-4-6: accepts low|medium|high|xhigh|max
//
// R-1GZL-PHUB: MVP OpenAI backend supports one model:
//   - gpt-5.5: accepts none|low|medium|high|xhigh; default "medium" (R-22XS-LD6T)
//
// R-ZZLK-I9CK: the registry also carries per-model pricing data used to
// compute total_cost_usd and modelUsage[<model>].costUSD on result events.
// Every entry must declare every rate it bills on; a model with unknown
// pricing cannot ship (silently-wrong totals).
package model

import (
	"fmt"
	"sort"
	"strings"
)

// PricingSpec carries USD per-million token rates for a model.
// R-ZZLK-I9CK: every registry entry must declare all rates it bills on.
// Zero means the provider does not charge for that tier (e.g. OpenAI
// has no cache-creation charge, so CacheCreationPerM is 0 for gpt-5.5).
//
// R-V2X8-QZDK: models with tiered pricing set InputTokenThreshold > 0
// and populate the AboveThreshold* fields. When the request's input tokens
// exceed the threshold, the entire request bills at the above-threshold
// rates; otherwise the base rates apply.
type PricingSpec struct {
	InputPerM         float64 // USD per million input tokens
	OutputPerM        float64 // USD per million output tokens
	CacheReadPerM     float64 // USD per million cache-read input tokens
	CacheCreationPerM float64 // USD per million cache-creation input tokens

	// R-V2X8-QZDK: when InputTokenThreshold > 0, requests whose input_tokens
	// exceed this value bill at the AboveThreshold* rates for the entire request.
	InputTokenThreshold             int
	AboveThresholdInputPerM         float64
	AboveThresholdOutputPerM        float64
	AboveThresholdCacheReadPerM     float64
	AboveThresholdCacheCreationPerM float64
}

// ComputeCost returns the total USD cost for the given token counts.
// R-V2X8-QZDK: when InputTokenThreshold > 0 and inputTokens exceeds it,
// the above-threshold rates apply to the entire request.
func (p PricingSpec) ComputeCost(inputTokens, outputTokens, cacheReadTokens, cacheCreationTokens int) float64 {
	in, out, cr, cc := p.InputPerM, p.OutputPerM, p.CacheReadPerM, p.CacheCreationPerM
	if p.InputTokenThreshold > 0 && inputTokens > p.InputTokenThreshold {
		in, out, cr, cc = p.AboveThresholdInputPerM, p.AboveThresholdOutputPerM, p.AboveThresholdCacheReadPerM, p.AboveThresholdCacheCreationPerM
	}
	return float64(inputTokens)/1e6*in +
		float64(outputTokens)/1e6*out +
		float64(cacheReadTokens)/1e6*cr +
		float64(cacheCreationTokens)/1e6*cc
}

// modelSpec holds the legal --effort vocabulary, the pinned default
// effort, pricing data, and static capability data for a given model.
//
// efforts: nil means the model takes no --effort argument at all (per
// R-MPR7-P0A4 for Haiku 4.5). A non-nil slice lists the accepted
// values; the per-flag effort-validation pass (R-ZX67-O1L1) reads it.
//
// defaultEffort: when --effort is omitted and efforts is non-nil, the
// backend uses this value in the request. R-22XS-LD6T pins "medium" for
// gpt-5.5. An empty string means no explicit default (the Anthropic
// backend handles effort internally for adaptive-thinking models).
type modelSpec struct {
	efforts         []string
	defaultEffort   string
	pricing         PricingSpec
	contextWindow   int // maximum context length in tokens
	maxOutputTokens int // maximum output tokens per request
}

// ContextSpec carries the static capacity limits for a model.
// R-Y5QZ-UNB2: used to populate contextWindow and maxOutputTokens in
// the result event's modelUsage entries.
type ContextSpec struct {
	ContextWindow   int
	MaxOutputTokens int
}

// ModelContext returns the ContextSpec for the resolved model.
func ModelContext(r Resolved) ContextSpec {
	if models, ok := registry[r.Provider]; ok {
		if spec, ok := models[r.BareID]; ok {
			return ContextSpec{
				ContextWindow:   spec.contextWindow,
				MaxOutputTokens: spec.maxOutputTokens,
			}
		}
	}
	return ContextSpec{}
}

// DefaultEffort returns the registry-pinned default effort string for r.
// Returns "" if the model has no registered default (Anthropic models
// handle effort in the backend; nil-effort models have no default).
func DefaultEffort(r Resolved) string {
	if models, ok := registry[r.Provider]; ok {
		if spec, ok := models[r.BareID]; ok {
			return spec.defaultEffort
		}
	}
	return ""
}

// ModelPricing returns the PricingSpec for the resolved model.
// R-ZZLK-I9CK: every registry entry declares all rates it bills on;
// this function is the sole source for computing total_cost_usd and
// modelUsage[<model>].costUSD on result events.
func ModelPricing(r Resolved) PricingSpec {
	if models, ok := registry[r.Provider]; ok {
		if spec, ok := models[r.BareID]; ok {
			return spec.pricing
		}
	}
	return PricingSpec{}
}

// EmbeddingPricingSpec carries the per-million input-token rate for an
// embeddings model. R-ZZLK-I9CK: every billed model declares its rate so
// the per-call cost_usd (P0c) is real, not zero. Embeddings bill only on
// input tokens; there is no output or cache tier (the response is the
// vector, not generated tokens).
type EmbeddingPricingSpec struct {
	InputPerM float64 // USD per million input tokens
}

// ComputeCost returns the USD cost of embedding inputTokens tokens.
func (p EmbeddingPricingSpec) ComputeCost(inputTokens int) float64 {
	return float64(inputTokens) / 1e6 * p.InputPerM
}

// embeddingRegistry: bare embeddings-model ID -> pricing. The chat
// modelSpec is chat-shaped (efforts, maxOutputTokens), so embeddings get a
// dedicated pricing table rather than a chat-registry row. R-ZZLK-I9CK.
//
// text-embedding-3-large: https://platform.openai.com/pricing (as of 2026-05),
// $0.13 per million input tokens; output/cache = 0 (no such tier).
var embeddingRegistry = map[string]EmbeddingPricingSpec{
	"text-embedding-3-large": {InputPerM: 0.13},
}

// EmbeddingPricing returns the EmbeddingPricingSpec for the named
// embeddings model. The bool is false when the model is unregistered, so a
// caller can refuse to ship a model with unknown pricing (R-ZZLK-I9CK)
// rather than silently bill zero.
func EmbeddingPricing(model string) (EmbeddingPricingSpec, bool) {
	spec, ok := embeddingRegistry[model]
	return spec, ok
}

// registry: provider -> bare model ID -> spec. The keys carry any
// suffix that distinguishes context variants (e.g. "[1m]"); the
// resolved bare ID is matched verbatim.
// R-MPR7-P0A4: Anthropic MVP models.
// R-1GZL-PHUB: OpenAI MVP model.
// R-L4ES-AFDE: Google MVP model.
// R-ZZLK-I9CK: every entry carries pricing data (USD per million tokens).
var registry = map[Provider]map[string]modelSpec{
	ProviderAnthropic: {
		// R-MPR7-P0A4: claude-haiku-4-5 — no --effort accepted.
		// Pricing: https://www.anthropic.com/pricing (as of 2026-05)
		"claude-haiku-4-5": {
			efforts: nil,
			pricing: PricingSpec{
				InputPerM:         0.80,
				OutputPerM:        4.00,
				CacheReadPerM:     0.08,
				CacheCreationPerM: 1.00,
			},
			contextWindow:   200000,
			maxOutputTokens: 8192,
		},
		// R-MPR7-P0A4: claude-sonnet-4-6 — accepts low|medium|high|xhigh|max.
		// Pricing: https://www.anthropic.com/pricing (as of 2026-05)
		"claude-sonnet-4-6": {
			efforts: []string{"low", "medium", "high", "xhigh", "max"},
			pricing: PricingSpec{
				InputPerM:         3.00,
				OutputPerM:        15.00,
				CacheReadPerM:     0.30,
				CacheCreationPerM: 3.75,
			},
			contextWindow:   200000,
			maxOutputTokens: 64000,
		},
	},
	ProviderOpenAI: {
		// R-1GZL-PHUB: gpt-5.5 accepts none|low|medium|high|xhigh.
		// R-22XS-LD6T: when --effort is omitted, send "medium".
		// R-ZEVA-05QR: OpenAI has no cache-creation charge (CacheCreationPerM=0).
		// Pricing: https://platform.openai.com/pricing (as of 2026-05)
		"gpt-5.5": {
			efforts:       []string{"none", "low", "medium", "high", "xhigh"},
			defaultEffort: "medium",
			pricing: PricingSpec{
				InputPerM:         2.00,
				OutputPerM:        8.00,
				CacheReadPerM:     0.50,
				CacheCreationPerM: 0.00,
			},
			contextWindow:   131072,
			maxOutputTokens: 32768,
		},
	},
	ProviderGoogle: {
		// R-L4ES-AFDE: gemini-3.1-pro-preview accepts low|medium|high (no disable).
		// R-M1C2-M8E5: when --effort is omitted, send "medium" (registry-pinned default).
		// R-V2X8-QZDK: tiered pricing at 200K input-token threshold.
		// Pricing: https://ai.google.dev/gemini-api/docs/pricing (as of 2026-05)
		"gemini-3.1-pro-preview": {
			efforts:       []string{"low", "medium", "high"},
			defaultEffort: "medium",
			pricing: PricingSpec{
				InputPerM:         2.00,
				OutputPerM:        12.00,
				CacheReadPerM:     0.20,
				CacheCreationPerM: 0.00,
				// R-V2X8-QZDK: >200K input tokens bills the entire request at premium rates.
				InputTokenThreshold:             200_000,
				AboveThresholdInputPerM:         4.00,
				AboveThresholdOutputPerM:        18.00,
				AboveThresholdCacheReadPerM:     0.40,
				AboveThresholdCacheCreationPerM: 0.00,
			},
			contextWindow:   2_000_000,
			maxOutputTokens: 8192,
		},
	},
}

// Validate checks that the resolved (provider, bare ID) is present in
// the registry. On miss it returns an error listing the supported
// models for that provider, sorted for stable output.
func Validate(r Resolved) error {
	models, ok := registry[r.Provider]
	if !ok || len(models) == 0 {
		return fmt.Errorf("--model %q: provider %q has no supported models in this build", r.BareID, r.Provider)
	}
	if _, ok := models[r.BareID]; ok {
		return nil
	}
	return fmt.Errorf("--model %q is not supported (provider %s); supported models: %s",
		r.BareID, r.Provider, strings.Join(supportedModels(models), ", "))
}

// ValidateEffort checks that the supplied --effort value is legal for
// the resolved model per the registry. R-31CY-UXSX: when the model's
// efforts slice is nil, the model takes no --effort at all and any
// non-empty value is rejected with an error naming the supported value
// as "(none)". When the slice is non-nil, an empty effort is accepted
// (caller is expected to fall back to a model-specific default) and a
// non-empty effort must be a member of the slice.
func ValidateEffort(r Resolved, effort string) error {
	models, ok := registry[r.Provider]
	if !ok {
		return fmt.Errorf("--effort %q: provider %q has no supported models in this build", effort, r.Provider)
	}
	spec, ok := models[r.BareID]
	if !ok {
		return fmt.Errorf("--effort %q: model %q is not in the registry", effort, r.BareID)
	}
	if spec.efforts == nil {
		if effort == "" {
			return nil
		}
		return fmt.Errorf("--effort %q is not supported for model %q; supported values: (none)", effort, r.BareID)
	}
	if effort == "" {
		return nil
	}
	for _, e := range spec.efforts {
		if e == effort {
			return nil
		}
	}
	return fmt.Errorf("--effort %q is not supported for model %q; supported values: %s",
		effort, r.BareID, strings.Join(spec.efforts, ", "))
}

func supportedModels(m map[string]modelSpec) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	sort.Strings(out)
	return out
}
