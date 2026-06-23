package eval

import (
	agentkit "github.com/ikigenba/agentkit"
	"github.com/ikigenba/agentkit/anthropic"

	"wiki/internal/llm"
)

// Judge runs eval-only claim scoring through a pinned LLM call site.
type Judge struct {
	c    *llm.Client
	site llm.CallSite
}

// NewJudge builds a claim judge from an injected LLM client and call site.
func NewJudge(c *llm.Client, site llm.CallSite) *Judge {
	return &Judge{c: c, site: site}
}

// DefaultJudgeCallSite returns the pinned reference yardstick for eval scoring.
func DefaultJudgeCallSite() llm.CallSite {
	return llm.CallSite{
		Stage:           "judge",
		Model:           anthropic.ModelOpus48,
		Reasoning:       agentkit.Level("high"),
		MaxTokens:       16384,
		MaxParseRetries: 2,
	}
}
