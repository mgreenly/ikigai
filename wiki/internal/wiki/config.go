package wiki

import (
	"fmt"
	"strconv"
	"strings"

	agentkit "github.com/ikigenba/agentkit"
	"github.com/ikigenba/agentkit/anthropic"

	"wiki/internal/llm"
)

const defaultMaxTokens = 16384

// CallSites carries wiki's per-stage generation settings.
type CallSites struct {
	Extract      llm.CallSite
	Compile      llm.CallSite
	AskSubject   llm.CallSite
	AskSynthesis llm.CallSite
}

// Config is wiki's service-side runtime configuration.
type Config struct {
	CallSites         CallSites
	WorkerConcurrency int
	SearchDefault     int
	SearchCap         int
	Provider          agentkit.Provider
	LLM               *llm.Client
}

// NewConfig reads wiki's secret and constructs the shared LLM provider/client.
func NewConfig(getenv func(string) string) (Config, error) {
	apiKey := strings.TrimSpace(getenv("ANTHROPIC_API_KEY"))
	if apiKey == "" {
		return Config{}, fmt.Errorf("ANTHROPIC_API_KEY is required")
	}

	callSites, err := resolveCallSites(getenv)
	if err != nil {
		return Config{}, err
	}
	provider := anthropic.New(apiKey)
	return Config{
		CallSites:         callSites,
		WorkerConcurrency: WorkerConcurrency,
		SearchDefault:     SearchDefault,
		SearchCap:         SearchCap,
		Provider:          provider,
		LLM:               llm.NewClient(provider, ModelID),
	}, nil
}

func resolveCallSites(getenv func(string) string) (CallSites, error) {
	extract, err := resolveCallSite(getenv, "EXTRACT", defaultExtractCallSite())
	if err != nil {
		return CallSites{}, err
	}
	compile, err := resolveCallSite(getenv, "COMPILE", defaultCompileCallSite())
	if err != nil {
		return CallSites{}, err
	}
	askSubject, err := resolveCallSite(getenv, "ASK_SUBJECT", defaultAskSubjectCallSite())
	if err != nil {
		return CallSites{}, err
	}
	askSynthesis, err := resolveCallSite(getenv, "ASK_SYNTHESIS", defaultAskSynthesisCallSite())
	if err != nil {
		return CallSites{}, err
	}
	return CallSites{
		Extract:      extract,
		Compile:      compile,
		AskSubject:   askSubject,
		AskSynthesis: askSynthesis,
	}, nil
}

// resolveCallSite layers <PREFIX>_MODEL / _REASONING / _TEMPERATURE / _MAX_TOKENS onto base.
func resolveCallSite(getenv func(string) string, prefix string, base llm.CallSite) (llm.CallSite, error) {
	site := base
	if model := strings.TrimSpace(getenv(prefix + "_MODEL")); model != "" {
		site.Model = model
	}
	if raw := strings.TrimSpace(getenv(prefix + "_REASONING")); raw != "" {
		reasoning, err := parseReasoning(raw)
		if err != nil {
			return llm.CallSite{}, fmt.Errorf("%s_REASONING: %w", prefix, err)
		}
		site.Reasoning = reasoning
	}
	if raw := strings.TrimSpace(getenv(prefix + "_TEMPERATURE")); raw != "" {
		temp, err := strconv.ParseFloat(raw, 64)
		if err != nil {
			return llm.CallSite{}, fmt.Errorf("%s_TEMPERATURE: %w", prefix, err)
		}
		site.Temperature = &temp
	}
	if raw := strings.TrimSpace(getenv(prefix + "_MAX_TOKENS")); raw != "" {
		maxTokens, err := strconv.Atoi(raw)
		if err != nil {
			return llm.CallSite{}, fmt.Errorf("%s_MAX_TOKENS: %w", prefix, err)
		}
		if maxTokens <= 0 {
			return llm.CallSite{}, fmt.Errorf("%s_MAX_TOKENS: must be greater than zero", prefix)
		}
		site.MaxTokens = maxTokens
	}
	return site, nil
}

func parseReasoning(raw string) (any, error) {
	switch strings.ToLower(strings.TrimSpace(raw)) {
	case "disabled", "off":
		return llm.DisableReasoning(), nil
	case "minimal", "low", "medium", "high", "xhigh", "max", "none":
		return agentkit.Level(strings.ToLower(strings.TrimSpace(raw))), nil
	default:
		return nil, fmt.Errorf("must be disabled, off, or a native reasoning level")
	}
}

func defaultExtractCallSite() llm.CallSite {
	temp := 0.0
	return llm.CallSite{
		Stage:           "extract",
		Model:           ModelID,
		Temperature:     &temp,
		Reasoning:       llm.DisableReasoning(),
		MaxTokens:       defaultMaxTokens,
		MaxParseRetries: 2,
	}
}

func defaultCompileCallSite() llm.CallSite {
	temp := 0.0
	return llm.CallSite{
		Stage:           "compile",
		Model:           ModelID,
		Temperature:     &temp,
		Reasoning:       llm.DisableReasoning(),
		MaxTokens:       defaultMaxTokens,
		MaxParseRetries: 2,
	}
}

func defaultAskSubjectCallSite() llm.CallSite {
	return llm.CallSite{
		Stage:     "ask-subject",
		Model:     ModelID,
		Reasoning: agentkit.Level("low"),
		MaxTokens: defaultMaxTokens,
	}
}

func defaultAskSynthesisCallSite() llm.CallSite {
	return llm.CallSite{
		Stage:     "ask-synthesis",
		Model:     ModelID,
		Reasoning: agentkit.Level("low"),
		MaxTokens: defaultMaxTokens,
	}
}
