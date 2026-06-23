package eval

import (
	"testing"

	agentkit "github.com/ikigenba/agentkit"
	"github.com/ikigenba/agentkit/anthropic"
)

func TestDefaultJudgeCallSiteUsesPinnedOpusHighReasoningWithoutTemperature(t *testing.T) {
	// R-DWI0-C7E2
	site := DefaultJudgeCallSite()
	if site.Stage != "judge" {
		t.Fatalf("stage = %q, want judge", site.Stage)
	}
	if site.Model != anthropic.ModelOpus48 {
		t.Fatalf("model = %q, want %q", site.Model, anthropic.ModelOpus48)
	}
	if site.Temperature != nil {
		t.Fatalf("temperature = %v, want unset for extended thinking", *site.Temperature)
	}
	if site.MaxTokens < 16384 {
		t.Fatalf("max tokens = %d, want at least 16384", site.MaxTokens)
	}
	if site.MaxParseRetries != 2 {
		t.Fatalf("max parse retries = %d, want 2", site.MaxParseRetries)
	}
	if site.Reasoning != agentkit.Level("high") {
		t.Fatalf("reasoning = %#v, want high level", site.Reasoning)
	}
}
