package runner

import (
	"testing"

	"prompts/internal/prompt"
)

func TestGenSettings_MaxTokensOmittedByDefault(t *testing.T) {
	cfg := prompt.Config{Provider: "anthropic", Model: "claude-haiku-4-5"}
	gen := genSettings(cfg)

	if gen.MaxTokens != 0 {
		t.Errorf("MaxTokens = %d, want zero-value provider default", gen.MaxTokens)
	}
}

func TestGenSettings_MaxTokensHonorsConfig(t *testing.T) {
	cfg := prompt.Config{Provider: "anthropic", Model: "claude-haiku-4-5", MaxTokens: 12345}
	gen := genSettings(cfg)

	if gen.MaxTokens != 12345 {
		t.Errorf("MaxTokens = %d, want explicit 12345", gen.MaxTokens)
	}
}
