package runner

import (
	"testing"

	"ralph/internal/engine/model"
	"ralph/internal/session"
)

// TestBuildRequest_MaxTokensDefaultsToModelMax verifies that when a session
// config sets no explicit max_tokens, buildRequest resolves the ceiling to the
// model's registry-pinned maximum output tokens rather than leaving it unset /
// falling back to a fixed low cap. This is the regression guard for the bug
// where every run was hard-capped at 4096 output tokens.
func TestBuildRequest_MaxTokensDefaultsToModelMax(t *testing.T) {
	resolved, err := model.Resolve("haiku")
	if err != nil {
		t.Fatalf("model.Resolve: %v", err)
	}
	want := model.ModelContext(resolved).MaxOutputTokens
	if want <= 0 {
		t.Fatalf("test precondition: model %q has no registered MaxOutputTokens", resolved.BareID)
	}

	sess := session.Session{
		Prompt: "do the thing",
		Config: session.Config{Provider: "anthropic", Model: "haiku"},
	}
	req := (&Runner{}).buildRequest(sess, resolved)

	if req.MaxTokens != want {
		t.Errorf("MaxTokens = %d, want model max %d", req.MaxTokens, want)
	}
}

// TestBuildRequest_MaxTokensHonorsConfig verifies an explicit config value wins
// over the model default.
func TestBuildRequest_MaxTokensHonorsConfig(t *testing.T) {
	resolved, err := model.Resolve("haiku")
	if err != nil {
		t.Fatalf("model.Resolve: %v", err)
	}

	sess := session.Session{
		Prompt: "do the thing",
		Config: session.Config{Provider: "anthropic", Model: "haiku", MaxTokens: 12345},
	}
	req := (&Runner{}).buildRequest(sess, resolved)

	if req.MaxTokens != 12345 {
		t.Errorf("MaxTokens = %d, want explicit 12345", req.MaxTokens)
	}
}
