package prompt

import (
	"encoding/json"
	"testing"
)

func TestConfigJSONCarriesProviderModelAndGenerationControls(t *testing.T) {
	// R-JTBA-4RDB
	temp := 0.7
	topP := 0.9
	budget := 4096
	thinking := false
	cfg := Config{
		Provider:       "anthropic",
		Model:          "claude-sonnet-4-6",
		Temperature:    &temp,
		TopP:           &topP,
		MaxTokens:      8192,
		Effort:         "high",
		ThinkingBudget: &budget,
		ThinkingLevel:  "medium",
		Thinking:       &thinking,
	}

	var got map[string]any
	b, err := json.Marshal(cfg)
	if err != nil {
		t.Fatalf("Marshal: %v", err)
	}
	if err := json.Unmarshal(b, &got); err != nil {
		t.Fatalf("Unmarshal: %v", err)
	}

	assertJSONField(t, got, "provider", "anthropic")
	assertJSONField(t, got, "model", "claude-sonnet-4-6")
	assertJSONField(t, got, "temperature", 0.7)
	assertJSONField(t, got, "top_p", 0.9)
	assertJSONField(t, got, "max_tokens", float64(8192))
	assertJSONField(t, got, "effort", "high")
	assertJSONField(t, got, "thinking_budget", float64(4096))
	assertJSONField(t, got, "thinking_level", "medium")
	assertJSONField(t, got, "thinking", false)
}

func TestConfigJSONCarriesRetryLoopAndProviderTuning(t *testing.T) {
	// R-JUJ6-IJ40
	cfg := Config{
		Provider:         "zai",
		Model:            "glm-5.2",
		MaxAttempts:      5,
		BaseDelay:        "500ms",
		MaxDelay:         "10s",
		MaxElapsed:       "1m",
		IgnoreRetryAfter: true,
		ToolLoopLimit:    42,
		BaseURL:          "https://example.test/zai",
	}

	var got map[string]any
	b, err := json.Marshal(cfg)
	if err != nil {
		t.Fatalf("Marshal: %v", err)
	}
	if err := json.Unmarshal(b, &got); err != nil {
		t.Fatalf("Unmarshal: %v", err)
	}

	assertJSONField(t, got, "max_attempts", float64(5))
	assertJSONField(t, got, "base_delay", "500ms")
	assertJSONField(t, got, "max_delay", "10s")
	assertJSONField(t, got, "max_elapsed", "1m")
	assertJSONField(t, got, "ignore_retry_after", true)
	assertJSONField(t, got, "tool_loop_limit", float64(42))
	assertJSONField(t, got, "base_url", "https://example.test/zai")
}

func assertJSONField(t *testing.T, got map[string]any, key string, want any) {
	t.Helper()
	if got[key] != want {
		t.Fatalf("%s = %#v, want %#v (json: %#v)", key, got[key], want, got)
	}
}
