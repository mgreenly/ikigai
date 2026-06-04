package anthropic

import (
	"testing"

	"ralph/internal/engine/provider"
)

// TestBuildPayloadUsesRequestMaxTokens verifies the wire payload carries the
// caller-resolved max_tokens instead of the former hardcoded 4096.
func TestBuildPayloadUsesRequestMaxTokens(t *testing.T) {
	payload := buildPayload("claude-haiku-4-5", provider.Request{MaxTokens: 32000})
	if got := payload["max_tokens"]; got != 32000 {
		t.Errorf("max_tokens = %v, want 32000", got)
	}
}

// TestBuildPayloadMaxTokensFallback verifies that an unset (zero) MaxTokens
// falls back to the conservative default so the required field is always
// present on the wire.
func TestBuildPayloadMaxTokensFallback(t *testing.T) {
	payload := buildPayload("claude-haiku-4-5", provider.Request{})
	if got := payload["max_tokens"]; got != defaultMaxTokens {
		t.Errorf("max_tokens = %v, want fallback %d", got, defaultMaxTokens)
	}
}
