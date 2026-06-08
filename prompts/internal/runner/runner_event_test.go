package runner

import (
	"strings"
	"testing"

	"agentkit/model"
	"agentkit/provider"
	"prompts/internal/prompt"
)

// userBlocks returns the blocks of the request's single user message.
func userBlocks(t *testing.T, req provider.Request) []provider.Block {
	t.Helper()
	if len(req.Messages) != 1 {
		t.Fatalf("Messages len = %d, want 1", len(req.Messages))
	}
	if req.Messages[0].Role != provider.RoleUser {
		t.Fatalf("Messages[0].Role = %v, want user", req.Messages[0].Role)
	}
	return req.Messages[0].Blocks
}

// TestBuildRequest_NoEventSingleBlock verifies a manual run (nil eventJSON)
// produces a user message with exactly one block equal to the verbatim user
// prompt.
func TestBuildRequest_NoEventSingleBlock(t *testing.T) {
	resolved, err := model.Resolve("haiku")
	if err != nil {
		t.Fatalf("model.Resolve: %v", err)
	}

	cfg := prompt.Config{Provider: "anthropic", Model: "haiku"}
	req := buildRequest(cfg, "do the thing", "", nil, resolved)

	blocks := userBlocks(t, req)
	if len(blocks) != 1 {
		t.Fatalf("user blocks len = %d, want 1", len(blocks))
	}
	tb, ok := blocks[0].(provider.TextBlock)
	if !ok {
		t.Fatalf("blocks[0] type = %T, want provider.TextBlock", blocks[0])
	}
	if tb.Text != "do the thing" {
		t.Errorf("blocks[0].Text = %q, want verbatim user prompt", tb.Text)
	}
}

// TestBuildRequest_EventSecondBlock verifies an event-triggered run appends a
// second user block carrying the preamble and the event JSON.
func TestBuildRequest_EventSecondBlock(t *testing.T) {
	resolved, err := model.Resolve("haiku")
	if err != nil {
		t.Fatalf("model.Resolve: %v", err)
	}

	event := []byte(`{"source":"crm","type":"contact.created","event_id":"01J","payload":{"id":"c1"}}`)
	cfg := prompt.Config{Provider: "anthropic", Model: "haiku"}
	req := buildRequest(cfg, "do the thing", "", event, resolved)

	blocks := userBlocks(t, req)
	if len(blocks) != 2 {
		t.Fatalf("user blocks len = %d, want 2", len(blocks))
	}

	first, ok := blocks[0].(provider.TextBlock)
	if !ok {
		t.Fatalf("blocks[0] type = %T, want provider.TextBlock", blocks[0])
	}
	if first.Text != "do the thing" {
		t.Errorf("blocks[0].Text = %q, want verbatim user prompt", first.Text)
	}

	second, ok := blocks[1].(provider.TextBlock)
	if !ok {
		t.Fatalf("blocks[1] type = %T, want provider.TextBlock", blocks[1])
	}
	if !strings.Contains(second.Text, eventPreamble) {
		t.Errorf("blocks[1].Text missing event preamble; got %q", second.Text)
	}
	if !strings.Contains(second.Text, "contact.created") {
		t.Errorf("blocks[1].Text missing event content; got %q", second.Text)
	}
}
