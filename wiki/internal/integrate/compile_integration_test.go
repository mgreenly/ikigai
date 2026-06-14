//go:build integration

// The standing integration tier's compile slice (P8) — see "Integration testing"
// in docs/wiki-redesign-plan.md. It runs the REAL pinned (prompt, model, effort)
// triple for compile against a blunt event pile and asserts the output is
// STRUCTURALLY valid — never whether it is good (quality is Part II's graded
// sweep). It is build-tag gated (`-tags=integration`) so it is always in the tree
// but never in the unit gate, which must stay deterministic, free, and offline.
//
// With no key/network it emits the visible `INTEGRATION CHECKPOINT SKIPPED — no
// keys` line and skips — never passing as if it ran.
package integrate

import (
	"context"
	"os"
	"strings"
	"testing"
	"time"

	"wiki/internal/config"
	"wiki/internal/llm"
)

func TestCompileIntegration(t *testing.T) {
	if os.Getenv("ANTHROPIC_API_KEY") == "" && os.Getenv("OPENAI_API_KEY") == "" {
		t.Log("INTEGRATION CHECKPOINT SKIPPED — no keys")
		t.Skip("no provider keys present")
	}

	cfg, err := config.Load(os.Getenv)
	if err != nil {
		t.Fatalf("config.Load: %v", err)
	}
	site := cfg.LLM.Compile
	if site.Prompt == "" {
		site.Prompt = config.DefaultCompilePrompt
	}

	w := llm.New(liveFactory(), nil)
	c := NewCompiler(NewWrapperCaller(w), site)

	// A blunt event pile: two obvious deal-stage events for one deal. Even a weak
	// model should compile a closed-deal subject citing the events; the assertion
	// is structural, not quality.
	events := []EventRow{
		{ID: "01EVTBLUNTA", Source: "crm:deal.stage_changed", Payload: []byte(`{"deal":"Acme renewal","from":"prospect","to":"negotiation","at":"2024-05-02"}`)},
		{ID: "01EVTBLUNTB", Source: "crm:deal.stage_changed", Payload: []byte(`{"deal":"Acme renewal","from":"negotiation","to":"closed_won","at":"2024-05-23"}`)},
	}

	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()

	subs, err := c.Compile(ctx, events)
	if err != nil {
		t.Fatalf("live compile failed (checkpoint RED — investigate): %v", err)
	}
	if len(subs) == 0 {
		t.Fatal("live compile returned no subjects for a blunt event pile (checkpoint RED)")
	}

	valid := map[string]struct{}{"01EVTBLUNTA": {}, "01EVTBLUNTB": {}}
	sawCite := false
	for _, s := range subs {
		switch s.Type {
		case TypeEntity, TypeEvent, TypeConcept:
		default:
			t.Errorf("subject %q has invalid type %q", s.Name, s.Type)
		}
		if strings.TrimSpace(s.Name) == "" {
			t.Error("subject with empty name")
		}
		if len(s.Claims) == 0 {
			t.Errorf("subject %q has no claims", s.Name)
		}
		for _, cl := range s.Claims {
			if len(cl.Cites) == 0 {
				t.Errorf("digest claim carries no cite (§5): %q", cl.Text)
			}
			for _, id := range cl.Cites {
				if _, ok := valid[id]; !ok {
					t.Errorf("claim cites %q which is not a presented event id (fabricated): %q", id, cl.Text)
				} else {
					sawCite = true
				}
			}
		}
	}
	if !sawCite {
		t.Error("no claim cited a real presented event id (checkpoint RED)")
	}
}
