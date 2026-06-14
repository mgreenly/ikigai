//go:build integration

// P11k fixture capture for the ask call-site prompt gate. See
// docs/wiki-redesign-plan.md, "P11k — Keyed Part-I validation gate": capture one
// REAL-model ask answer from the live pinned triple over the fixture wiki and
// commit it as testdata/ask_response.json's recorded sibling, so the offline
// (a-ii) gate runs against real output. Runs only with WIKI_CAPTURE_FIXTURES=1
// and keys present; drives the production Asker and tees the agent's final raw
// answer JSON.
package read

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
	"time"

	"agentkit/provider"

	"wiki/internal/config"
	"wiki/internal/llm"
)

// teeAskCaller wraps the ask agent caller and writes the final raw answer JSON.
type teeAskCaller struct {
	inner caller
	path  string
	t     *testing.T
}

func (c *teeAskCaller) Agent(ctx context.Context, site config.CallSite, msgs []provider.Message, tools []provider.Tool, budget llm.AgentBudget, dispatch llm.ToolDispatch) (*llm.StructuredResult, error) {
	res, err := c.inner.Agent(ctx, site, msgs, tools, budget, dispatch)
	if err != nil {
		return res, err
	}
	if res != nil {
		writeRecorded(c.t, c.path, res.Raw)
	}
	return res, nil
}

func writeRecorded(t *testing.T, name, raw string) {
	t.Helper()
	// The agent often wraps its JSON answer in prose; the structured-output
	// contract is the object ParseAnswer extracts, so record exactly that.
	obj := extractJSONObject(stripCodeFence(raw))
	var v any
	if err := json.Unmarshal([]byte(obj), &v); err != nil {
		t.Fatalf("recorded answer is not valid JSON: %v\n%s", err, raw)
	}
	b, err := json.MarshalIndent(v, "", "  ")
	if err != nil {
		t.Fatalf("marshal recorded: %v", err)
	}
	b = append(b, '\n')
	if err := os.WriteFile(filepath.Join("testdata", name), b, 0o644); err != nil {
		t.Fatalf("write recorded fixture: %v", err)
	}
	t.Logf("recorded real-model fixture testdata/%s (%d bytes)", name, len(b))
}

func TestCaptureAskFixture(t *testing.T) {
	if os.Getenv("WIKI_CAPTURE_FIXTURES") != "1" {
		t.Skip("set WIKI_CAPTURE_FIXTURES=1 to (re)record real-model fixtures")
	}
	if os.Getenv("ANTHROPIC_API_KEY") == "" && os.Getenv("OPENAI_API_KEY") == "" {
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
	tee := &teeAskCaller{inner: w, path: "ask_response.json", t: t}
	asker := NewAsker(svc, tee, newInboxStore(t, conn), asks, site, llm.AgentBudget{
		MaxTurns:  cfg.AskMaxTurns,
		MaxTokens: cfg.AskMaxTokens,
		MaxWall:   time.Duration(cfg.AskMaxWallSeconds) * time.Second,
	})

	ctx, cancel := context.WithTimeout(context.Background(), 120*time.Second)
	defer cancel()
	ans, err := asker.Ask(ctx, "owner@x.com", "Who is the CEO of Acme Corp?")
	if err != nil {
		t.Fatalf("capture ask: %v", err)
	}
	if !ans.Found {
		t.Fatal("answerable question abstained — the (a-ii) gate needs a found fixture; re-run")
	}
}
