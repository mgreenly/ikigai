package mcp

import (
	"context"
	"encoding/json"
	"testing"

	"wiki/internal/events"
	"wiki/internal/inbox"
)

// fakeLintRunner records the LintRun call for the lint_run dispatch test.
type fakeLintRunner struct {
	lastOwner, lastJob string
	rec                inbox.Receipt
	err                error
}

func (f *fakeLintRunner) LintRun(_ context.Context, owner, job string) (inbox.Receipt, error) {
	f.lastOwner, f.lastJob = owner, job
	return f.rec, f.err
}

func newHandlerWithLint(l LintRunner) *Handler {
	return NewHandler("1", "wiki",
		func(ctx context.Context) (map[string]any, error) { return map[string]any{"ok": true}, nil },
		events.Registry, nil, nil, l, nil)
}

// TestLintRunAcceptsTrigger: lint_run forwards the job + authenticated owner to the
// LintRunner and returns the trigger row's inbox id (a receipt, not a result).
func TestLintRunAcceptsTrigger(t *testing.T) {
	fake := &fakeLintRunner{rec: inbox.Receipt{ID: "01LINT"}}
	h := newHandlerWithLint(fake)
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"lint_run","arguments":{"job":"lint-dups"}}}`)
	var r map[string]any
	if err := json.Unmarshal([]byte(toolText(t, out)), &r); err != nil {
		t.Fatalf("receipt: %v", err)
	}
	if r["id"] != "01LINT" || r["job"] != "lint-dups" {
		t.Errorf("lint_run receipt = %v", r)
	}
	if fake.lastJob != "lint-dups" || fake.lastOwner != "owner@example.com" {
		t.Errorf("LintRun got owner=%q job=%q", fake.lastOwner, fake.lastJob)
	}
}

// TestLintRunRequiresJob: a missing 'job' is a tool error, not a panic.
func TestLintRunRequiresJob(t *testing.T) {
	h := newHandlerWithLint(&fakeLintRunner{})
	out := rpc(t, h, `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"lint_run","arguments":{}}}`)
	if out["result"].(map[string]any)["isError"] != true {
		t.Errorf("expected isError for missing job")
	}
}
