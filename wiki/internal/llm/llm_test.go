package llm

import (
	"context"
	"errors"
	"testing"

	"agentkit/model"
	"agentkit/provider"

	"wiki/internal/config"
)

// fakeClient is a no-op streaming client; the seam test only needs the factory
// to resolve a client, not to stream.
type fakeClient struct{}

func (fakeClient) Stream(ctx context.Context, req provider.Request) (<-chan provider.Event, error) {
	ch := make(chan provider.Event)
	close(ch)
	return ch, nil
}

// TestRequestUsesInjectedTriple is the enablement-seam test (obligation 1): the
// wrapper builds the provider.Request purely from the injected CallSite triple —
// swap the triple and the request follows. No constant, no env at the site.
func TestRequestUsesInjectedTriple(t *testing.T) {
	w := New(nil, nil)

	siteA := config.CallSite{Name: "extract", Prompt: "PROMPT A", Model: "claude-sonnet-4-6", Effort: "medium"}
	reqA := w.Request(siteA, nil, nil, nil)
	if reqA.Model != "claude-sonnet-4-6" || reqA.Effort != "medium" || reqA.SystemPrompt != "PROMPT A" {
		t.Fatalf("request did not follow triple A: %+v", reqA)
	}

	// Swap the triple — the harness's exact move.
	siteB := config.CallSite{Name: "extract", Prompt: "PROMPT B", Model: "gpt-5.5", Effort: "high"}
	reqB := w.Request(siteB, nil, nil, nil)
	if reqB.Model != "gpt-5.5" || reqB.Effort != "high" || reqB.SystemPrompt != "PROMPT B" {
		t.Fatalf("request did not follow swapped triple B: %+v", reqB)
	}
}

// TestNotWiredWithoutFactory: a call shape returns ErrNotWired when no provider
// factory is configured — the scaffold seam is present and typed, never a silent
// no-op that would hide a missing wiring.
func TestNotWiredWithoutFactory(t *testing.T) {
	w := New(nil, nil)
	site := config.CallSite{Name: "extract", Model: "claude-sonnet-4-6", Effort: "medium"}

	if _, err := w.Structured(context.Background(), site, nil, nil); !errors.Is(err, ErrNotWired) {
		t.Errorf("Structured without factory: want ErrNotWired, got %v", err)
	}
	if _, err := w.Agent(context.Background(), site, nil, nil, AgentBudget{}, nil); !errors.Is(err, ErrNotWired) {
		t.Errorf("Agent without factory: want ErrNotWired, got %v", err)
	}
}

// TestFactoryResolvesByProvider: the wrapper resolves a site's model id to a
// provider through agentkit/model — claude-* and gpt-* both reach the factory,
// chosen purely by config.
func TestFactoryResolvesByProvider(t *testing.T) {
	var got []model.Provider
	factory := func(r model.Resolved) (Client, error) {
		got = append(got, r.Provider)
		return fakeClient{}, nil
	}
	w := New(factory, nil)

	// Anthropic site.
	if _, err := w.Structured(context.Background(), config.CallSite{Name: "x", Model: "claude-sonnet-4-6", Effort: "medium"}, nil, nil); !errors.Is(err, ErrNotWired) {
		// body is a stub (returns ErrNotWired) but the client must have resolved first.
		if err != nil && !errors.Is(err, ErrNotWired) {
			t.Fatalf("unexpected error: %v", err)
		}
	}
	// OpenAI site.
	if _, err := w.Structured(context.Background(), config.CallSite{Name: "y", Model: "gpt-5.5", Effort: "high"}, nil, nil); err != nil && !errors.Is(err, ErrNotWired) {
		t.Fatalf("unexpected error: %v", err)
	}

	if len(got) != 2 || got[0] != model.ProviderAnthropic || got[1] != model.ProviderOpenAI {
		t.Errorf("factory provider resolution = %v, want [anthropic openai]", got)
	}
}
