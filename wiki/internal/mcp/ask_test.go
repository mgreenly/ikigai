package mcp

import (
	"context"
	"testing"

	"wiki/internal/ask"
)

// stubAsker is a test double for the Asker seam: it records the last call and
// returns a canned result, proving the verb → asker wiring (arg parsing, owner
// threading, result shaping) without spawning a real agent job.
type stubAsker struct {
	gotOwner    string
	gotQuestion string
	result      ask.Result
}

func (s *stubAsker) Ask(_ context.Context, owner, _ string, question string) (ask.Result, error) {
	s.gotOwner = owner
	s.gotQuestion = question
	return s.result, nil
}

func TestAsk_Verb(t *testing.T) {
	stub := &stubAsker{result: ask.Result{JobID: "ask-job-1"}}
	h := NewHandler(nil, nil, stub)

	p, isErr := callTool(t, h, "wiki_ask", `{"question":"What are otters?"}`)
	if isErr {
		t.Fatalf("ask returned isError: %v", p)
	}
	if p["job_id"] != "ask-job-1" {
		t.Fatalf("ask result = %v", p)
	}
	if stub.gotOwner != "me@example.com" {
		t.Fatalf("owner threaded = %q, want me@example.com", stub.gotOwner)
	}
	if stub.gotQuestion != "What are otters?" {
		t.Fatalf("question threaded = %q", stub.gotQuestion)
	}
}

func TestAsk_RequiresQuestion(t *testing.T) {
	h := NewHandler(nil, nil, &stubAsker{})
	_, isErr := callTool(t, h, "wiki_ask", `{}`)
	if !isErr {
		t.Fatal("ask with empty question should be a tool-error")
	}
}

func TestAsk_NilAskerUnavailable(t *testing.T) {
	h := NewHandler(nil, nil, nil)
	_, isErr := callTool(t, h, "wiki_ask", `{"question":"x"}`)
	if !isErr {
		t.Fatal("ask with nil asker should be a tool-error")
	}
}
