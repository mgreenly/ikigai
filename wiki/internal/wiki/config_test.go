package wiki

import (
	"reflect"
	"strings"
	"testing"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/llm"
)

func TestNewConfigBuildsDefaultPerCallSiteModels(t *testing.T) {
	// R-GIY9-26PA
	cfg, err := NewConfig(fakeGetenv(map[string]string{
		"ANTHROPIC_API_KEY": "test-key",
	}))
	if err != nil {
		t.Fatalf("NewConfig: %v", err)
	}
	if cfg.Provider == nil {
		t.Fatal("Provider is nil")
	}
	if cfg.LLM == nil {
		t.Fatal("LLM is nil")
	}
	if cfg.LLM.Provider() != cfg.Provider {
		t.Fatal("LLM provider is not the shared provider")
	}
	if cfg.LLM.Model() != ModelID {
		t.Fatalf("LLM model = %q, want %q", cfg.LLM.Model(), ModelID)
	}
	assertResolvedSite(t, cfg.CallSites.Extract, "extract", ModelID, 0, llm.DisableReasoning(), 16384, 2)
	assertResolvedSite(t, cfg.CallSites.Compile, "compile", ModelID, 0, llm.DisableReasoning(), 16384, 2)
	assertResolvedSite(t, cfg.CallSites.AskSubject, "ask-subject", ModelID, nil, agentkit.Level("low"), 16384, 0)
	assertResolvedSite(t, cfg.CallSites.AskSynthesis, "ask-synthesis", ModelID, nil, agentkit.Level("low"), 16384, 0)
}

func TestNewConfigLayersPerCallSiteEnvironmentOverrides(t *testing.T) {
	// R-GK65-FYFZ
	cfg, err := NewConfig(fakeGetenv(map[string]string{
		"ANTHROPIC_API_KEY":        "test-key",
		"EXTRACT_MODEL":            "extract-model",
		"EXTRACT_TEMPERATURE":      "0.25",
		"COMPILE_MODEL":            "compile-model",
		"COMPILE_MAX_TOKENS":       "4096",
		"ASK_SUBJECT_MODEL":        "subject-model",
		"ASK_SUBJECT_REASONING":    "high",
		"ASK_SYNTHESIS_MODEL":      "synthesis-model",
		"ASK_SYNTHESIS_REASONING":  "disabled",
		"ASK_SYNTHESIS_MAX_TOKENS": "8192",
	}))
	if err != nil {
		t.Fatalf("NewConfig: %v", err)
	}

	assertResolvedSite(t, cfg.CallSites.Extract, "extract", "extract-model", 0.25, llm.DisableReasoning(), 16384, 2)
	assertResolvedSite(t, cfg.CallSites.Compile, "compile", "compile-model", 0, llm.DisableReasoning(), 4096, 2)
	assertResolvedSite(t, cfg.CallSites.AskSubject, "ask-subject", "subject-model", nil, agentkit.Level("high"), 16384, 0)
	assertResolvedSite(t, cfg.CallSites.AskSynthesis, "ask-synthesis", "synthesis-model", nil, llm.DisableReasoning(), 8192, 0)
}

func TestNewConfigRejectsMalformedCallSiteEnvironment(t *testing.T) {
	tests := []struct {
		name    string
		env     map[string]string
		wantErr string
	}{
		{
			name: "temperature",
			env: map[string]string{
				"ANTHROPIC_API_KEY":   "test-key",
				"EXTRACT_TEMPERATURE": "warm",
			},
			wantErr: "EXTRACT_TEMPERATURE",
		},
		{
			name: "max tokens",
			env: map[string]string{
				"ANTHROPIC_API_KEY":  "test-key",
				"COMPILE_MAX_TOKENS": "0",
			},
			wantErr: "COMPILE_MAX_TOKENS",
		},
		{
			name: "reasoning",
			env: map[string]string{
				"ANTHROPIC_API_KEY":     "test-key",
				"ASK_SUBJECT_REASONING": "turbo",
			},
			wantErr: "ASK_SUBJECT_REASONING",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// R-GLE1-TQ6O
			_, err := NewConfig(fakeGetenv(tt.env))
			if err == nil {
				t.Fatal("NewConfig returned nil error")
			}
			if !strings.Contains(err.Error(), tt.wantErr) {
				t.Fatalf("error = %v, want %q", err, tt.wantErr)
			}
		})
	}
}

func assertResolvedSite(t *testing.T, got llm.CallSite, stage, model string, temp any, reasoning any, maxTokens, maxParseRetries int) {
	t.Helper()
	if got.Stage != stage {
		t.Fatalf("stage = %q, want %q", got.Stage, stage)
	}
	if got.Model != model {
		t.Fatalf("%s model = %q, want %q", stage, got.Model, model)
	}
	if temp == nil {
		if got.Temperature != nil {
			t.Fatalf("%s temperature = %#v, want nil", stage, got.Temperature)
		}
	} else {
		var wantTemp float64
		switch v := temp.(type) {
		case float64:
			wantTemp = v
		case int:
			wantTemp = float64(v)
		default:
			t.Fatalf("unsupported temperature expectation type %T", temp)
		}
		if got.Temperature == nil || *got.Temperature != wantTemp {
			t.Fatalf("%s temperature = %#v, want %v", stage, got.Temperature, wantTemp)
		}
	}
	if !reflect.DeepEqual(got.Reasoning, reasoning) {
		t.Fatalf("%s reasoning = %#v, want %#v", stage, got.Reasoning, reasoning)
	}
	if got.MaxTokens != maxTokens {
		t.Fatalf("%s MaxTokens = %d, want %d", stage, got.MaxTokens, maxTokens)
	}
	if got.MaxParseRetries != maxParseRetries {
		t.Fatalf("%s MaxParseRetries = %d, want %d", stage, got.MaxParseRetries, maxParseRetries)
	}
}

func fakeGetenv(values map[string]string) func(string) string {
	return func(key string) string {
		return values[key]
	}
}
