//go:build integration

// The standing integration tier's read-side slice (P10) — see "Integration
// testing" in docs/wiki-redesign-plan.md. It runs the REAL pinned ask (prompt,
// model, effort) triple over a fixture wiki and asserts the output is
// STRUCTURALLY valid — never whether it is good (quality is Part II's graded
// sweep). Build-tag gated (`-tags=integration`) so it is always in the tree but
// never in the unit gate, which stays deterministic, free, and offline.
//
// With no key/network it emits the visible `INTEGRATION CHECKPOINT SKIPPED — no
// keys` line and skips — never passing as if it ran.
package read

import (
	"context"
	"os"
	"testing"
	"time"

	"agentkit/model"
	"agentkit/provider/anthropic"
	"agentkit/provider/openai"

	"wiki/internal/config"
	"wiki/internal/llm"
)

func liveFactory() llm.ClientFactory {
	return func(r model.Resolved) (llm.Client, error) {
		switch r.Provider {
		case model.ProviderOpenAI:
			return openai.New(os.Getenv("OPENAI_API_KEY"), r.BareID)
		default:
			return anthropic.New(os.Getenv("ANTHROPIC_API_KEY"), r.BareID)
		}
	}
}

func TestAskIntegration(t *testing.T) {
	if os.Getenv("ANTHROPIC_API_KEY") == "" && os.Getenv("OPENAI_API_KEY") == "" {
		t.Log("INTEGRATION CHECKPOINT SKIPPED — no keys")
		t.Skip("no provider keys present")
	}

	conn := newTestDB(t)
	seedAcme(t, conn)
	svc := newReadService(conn)
	asks := NewAskStore(conn)

	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	site := cfg.LLM.Ask
	if site.Prompt == "" {
		site.Prompt = config.DefaultAskPrompt
	}

	w := llm.New(liveFactory(), nil)
	asker := NewAsker(svc, w, newInboxStore(t, conn), asks, site, llm.AgentBudget{
		MaxTurns:  cfg.AskMaxTurns,
		MaxTokens: cfg.AskMaxTokens,
		MaxWall:   time.Duration(cfg.AskMaxWallSeconds) * time.Second,
	})

	ctx, cancel := context.WithTimeout(context.Background(), 120*time.Second)
	defer cancel()

	// 1) An answerable question: answer-xor-abstention, every cited subject resolves.
	ans, err := asker.Ask(ctx, "owner@x.com", "Who is the CEO of Acme Corp?")
	if err != nil {
		t.Fatalf("live ask failed (checkpoint RED — investigate): %v", err)
	}
	if !ans.Found {
		t.Error("answerable question abstained (checkpoint RED) — Acme Corp is in the fixture")
	}
	for _, c := range ans.Citations {
		wp, ok, err := svc.readPage(ctx, c.Subject)
		if err != nil {
			t.Fatalf("citation resolve: %v", err)
		}
		if !ok {
			t.Errorf("cited subject %q does not resolve to a page", c.Subject)
		}
		_ = wp
	}

	// 2) A known-gap question: the wiki has nothing — it must abstain, not fabricate.
	gap, err := asker.Ask(ctx, "owner@x.com", "What is the boiling point of dilithium on Qo'noS?")
	if err != nil {
		t.Fatalf("live gap ask failed (checkpoint RED): %v", err)
	}
	if gap.Found || len(gap.Citations) != 0 {
		t.Errorf("known-gap question did not abstain (checkpoint RED): %+v", gap)
	}

	// 3) Real search returns whole-page hits, registry-first.
	hits, err := svc.Search(ctx, "Acme Corp", 5)
	if err != nil {
		t.Fatalf("live search: %v", err)
	}
	if len(hits) == 0 || hits[0].Subject != "01HACMECORPSUBJECTID0001" || hits[0].Body == "" {
		t.Errorf("search not registry-first whole-page: %v", hits)
	}
}
