package agent

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"testing"

	"agentkit/provider"
	"agentkit/schema"
	"agentkit/wire"
)

// fakeClient is a provider.Client whose Stream replays a fixed
// sequence of events. The channel is closed after the last event so
// drainTurn's range terminates.
type fakeClient struct {
	events []provider.Event
	err    error
}

func (f *fakeClient) Stream(ctx context.Context, req provider.Request) (<-chan provider.Event, error) {
	if f.err != nil {
		return nil, f.err
	}
	ch := make(chan provider.Event, len(f.events))
	for _, ev := range f.events {
		ch <- ev
	}
	close(ch)
	return ch, nil
}

// R-VJBZ-S578: a single iteration terminates with exactly one result
// event whose structured_output validates against the supplied
// --json-schema. The simplest slice: end_turn with assistant text that
// is the literal ralph-loops control object.
func TestR_VJBZ_S578_IterationEmitsOneResultMatchingSchema(t *testing.T) {
	const ralphSchema = `{
		"type": "object",
		"properties": {
			"status": {"type": "string", "enum": ["DONE", "CONTINUE"]}
		},
		"required": ["status"],
		"additionalProperties": false
	}`
	sch, err := schema.Parse([]byte(ralphSchema))
	if err != nil {
		t.Fatalf("schema.Parse: %v", err)
	}

	client := &fakeClient{events: []provider.Event{
		provider.EventTextDelta{Text: `{"status":"DONE"}`},
		provider.EventDone{StopReason: "end_turn"},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)

	if err := Run(context.Background(), client, sess, provider.Request{}, Options{Schema: sch}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	lines := splitLines(out.String())
	if len(lines) != 2 {
		t.Fatalf("expected exactly 2 events (assistant + result), got %d: %q", len(lines), out.String())
	}

	var assistant struct {
		Type    string `json:"type"`
		Message struct {
			Role    string           `json:"role"`
			Content []map[string]any `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[0]), &assistant); err != nil {
		t.Fatalf("unmarshal assistant: %v", err)
	}
	if assistant.Type != "assistant" {
		t.Fatalf("first event type = %q, want assistant", assistant.Type)
	}
	if len(assistant.Message.Content) != 1 || assistant.Message.Content[0]["type"] != "text" {
		t.Fatalf("assistant content = %+v, want one text block", assistant.Message.Content)
	}

	var result struct {
		Type             string          `json:"type"`
		StructuredOutput json.RawMessage `json:"structured_output"`
		IsError          bool            `json:"is_error"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &result); err != nil {
		t.Fatalf("unmarshal result: %v", err)
	}
	if result.Type != "result" {
		t.Fatalf("second event type = %q, want result", result.Type)
	}
	if result.IsError {
		t.Fatalf("is_error = true, want false; structured_output=%s", result.StructuredOutput)
	}

	var got any
	if err := json.Unmarshal(result.StructuredOutput, &got); err != nil {
		t.Fatalf("unmarshal structured_output: %v", err)
	}
	if err := sch.Validate(got); err != nil {
		t.Fatalf("emitted structured_output failed schema: %v", err)
	}
	gotMap, ok := got.(map[string]any)
	if !ok || gotMap["status"] != "DONE" {
		t.Fatalf("structured_output = %v, want {status:DONE}", got)
	}

	if !sess.Finished() {
		t.Fatalf("session not finished after Run")
	}
}

// sequenceClient is a provider.Client whose Stream replays a different
// fixed sequence on each call, in order. It is used to simulate a
// model that returns invalid structured_output on early attempts and a
// schema-conforming object on a later one.
type sequenceClient struct {
	sequences [][]provider.Event
	calls     int
}

func (s *sequenceClient) Stream(ctx context.Context, req provider.Request) (<-chan provider.Event, error) {
	if s.calls >= len(s.sequences) {
		return nil, &provider.Error{Kind: provider.ErrUnknown, Msg: "sequenceClient exhausted"}
	}
	evs := s.sequences[s.calls]
	s.calls++
	ch := make(chan provider.Event, len(evs))
	for _, ev := range evs {
		ch <- ev
	}
	close(ch)
	return ch, nil
}

// R-WFWM-BKWX: when the model produces output that fails to validate
// against the supplied --json-schema, the agent retries the model up
// to a bounded number of times before surfacing an iteration error.
// On a successful retry the iteration emits exactly one result event;
// every attempt's turn is recorded as its own assistant event.
func TestR_WFWM_BKWX_AgentRetriesOnValidationFailure(t *testing.T) {
	const ralphSchema = `{
		"type": "object",
		"properties": {
			"status": {"type": "string", "enum": ["DONE", "CONTINUE"]}
		},
		"required": ["status"],
		"additionalProperties": false
	}`
	sch, err := schema.Parse([]byte(ralphSchema))
	if err != nil {
		t.Fatalf("schema.Parse: %v", err)
	}

	// Two failing attempts (malformed JSON, then schema-violating
	// JSON) followed by a passing one. Verifies bounded retry.
	t.Run("eventual_success", func(t *testing.T) {
		client := &sequenceClient{sequences: [][]provider.Event{
			{
				provider.EventTextDelta{Text: `not json at all`},
				provider.EventDone{StopReason: "end_turn"},
			},
			{
				provider.EventTextDelta{Text: `{"status":"NOPE"}`},
				provider.EventDone{StopReason: "end_turn"},
			},
			{
				provider.EventTextDelta{Text: `{"status":"DONE"}`},
				provider.EventDone{StopReason: "end_turn"},
			},
		}}

		var out bytes.Buffer
		sess := wire.NewSession(&out)

		if err := Run(context.Background(), client, sess, provider.Request{}, Options{Schema: sch}); err != nil {
			t.Fatalf("Run: %v", err)
		}
		if client.calls != 3 {
			t.Fatalf("client.calls = %d, want 3", client.calls)
		}

		lines := splitLines(out.String())
		if len(lines) != 4 {
			t.Fatalf("expected 4 events (3 assistants + result), got %d: %q", len(lines), out.String())
		}
		for i := 0; i < 3; i++ {
			var ev struct {
				Type string `json:"type"`
			}
			if err := json.Unmarshal([]byte(lines[i]), &ev); err != nil {
				t.Fatalf("unmarshal line %d: %v", i, err)
			}
			if ev.Type != "assistant" {
				t.Fatalf("line %d type = %q, want assistant", i, ev.Type)
			}
		}
		var result struct {
			Type             string          `json:"type"`
			StructuredOutput json.RawMessage `json:"structured_output"`
			IsError          bool            `json:"is_error"`
		}
		if err := json.Unmarshal([]byte(lines[3]), &result); err != nil {
			t.Fatalf("unmarshal result: %v", err)
		}
		if result.Type != "result" {
			t.Fatalf("last event type = %q, want result", result.Type)
		}
		if result.IsError {
			t.Fatalf("is_error = true, want false; structured_output=%s", result.StructuredOutput)
		}
		var got map[string]any
		if err := json.Unmarshal(result.StructuredOutput, &got); err != nil {
			t.Fatalf("unmarshal structured_output: %v", err)
		}
		if got["status"] != "DONE" {
			t.Fatalf("structured_output = %v, want {status:DONE}", got)
		}
	})

	// All maxStructuredAttempts attempts fail validation. The agent
	// emits one assistant per attempt and a final is_error result.
	t.Run("exhausted_retries", func(t *testing.T) {
		bad := []provider.Event{
			provider.EventTextDelta{Text: `{"status":"NOPE"}`},
			provider.EventDone{StopReason: "end_turn"},
		}
		client := &sequenceClient{sequences: [][]provider.Event{bad, bad, bad}}

		var out bytes.Buffer
		sess := wire.NewSession(&out)

		if err := Run(context.Background(), client, sess, provider.Request{}, Options{Schema: sch}); err != nil {
			t.Fatalf("Run: %v", err)
		}
		if client.calls != maxStructuredAttempts {
			t.Fatalf("client.calls = %d, want %d", client.calls, maxStructuredAttempts)
		}

		lines := splitLines(out.String())
		if len(lines) != maxStructuredAttempts+1 {
			t.Fatalf("expected %d events, got %d: %q", maxStructuredAttempts+1, len(lines), out.String())
		}
		var result struct {
			Type             string          `json:"type"`
			StructuredOutput json.RawMessage `json:"structured_output"`
			IsError          bool            `json:"is_error"`
		}
		if err := json.Unmarshal([]byte(lines[len(lines)-1]), &result); err != nil {
			t.Fatalf("unmarshal result: %v", err)
		}
		if result.Type != "result" || !result.IsError {
			t.Fatalf("last event = %+v, want result with is_error=true", result)
		}
		var payload map[string]string
		if err := json.Unmarshal(result.StructuredOutput, &payload); err != nil {
			t.Fatalf("unmarshal structured_output: %v", err)
		}
		if !strings.Contains(payload["error"], "after 3 attempts") {
			t.Fatalf("error message = %q, want it to mention attempts count", payload["error"])
		}
	})
}

// R-8PF6-I8FP: ikigai-cli sends a non-empty system prompt on every
// provider request so that the model operates as an agent rather than
// a plain chatbot.
func TestR_8PF6_I8FP_FramingPromptIsNonEmpty(t *testing.T) {
	if FramingPrompt == "" {
		t.Fatal("FramingPrompt must be non-empty (R-8PF6-I8FP): the system prompt is required to orient the model as an agent")
	}
}

// R-GA6J-9O0I (agent): the framing system prompt orients the model as
// an autonomous agent confined to a single persistent folder whose
// deliverables persist as files, with NO network access, and whose final
// assistant message is recorded verbatim as the run result (free text —
// no required JSON format). This replaces the original bare-JSON framing,
// which no longer applies now that agent runs in freeform terminal mode.
func TestR_GA6J_9O0I_FramingPromptOrientsAgent(t *testing.T) {
	p := strings.ToLower(FramingPrompt)
	if !strings.Contains(p, "folder") {
		t.Error("FramingPrompt must describe the persistent folder (R-GA6J-9O0I)")
	}
	if !strings.Contains(p, "file") {
		t.Error("FramingPrompt must instruct the model to leave deliverables as files (R-GA6J-9O0I)")
	}
	if !strings.Contains(p, "network") {
		t.Error("FramingPrompt must state there is no network access (R-GA6J-9O0I)")
	}
	if !strings.Contains(p, "free text") {
		t.Error("FramingPrompt must state the final message is recorded as free text (R-GA6J-9O0I)")
	}
}

// R-8293-8LCI: when an assistant turn ends with tool_use, the agent
// dispatches every tool_use block, emits a user event with tool_results,
// appends both turns to history, and re-invokes the provider. The
// iteration ends when the model returns a non-tool stop reason.
func TestR_8293_8LCI_ToolRoundTripDispatchesToolsAndContinues(t *testing.T) {
	// Write a known file the Read tool can consume.
	tmp := t.TempDir()
	filePath := tmp + "/hello.txt"
	if err := os.WriteFile(filePath, []byte("hello world\n"), 0o644); err != nil {
		t.Fatalf("write temp file: %v", err)
	}

	toolInput, err := json.Marshal(map[string]string{"file_path": filePath})
	if err != nil {
		t.Fatalf("marshal tool input: %v", err)
	}

	// First provider call: the model asks to read the temp file.
	// Second provider call: the model returns the final structured output.
	client := &sequenceClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_01", Name: "Read", Input: toolInput},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: `{"status":"DONE"}`},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	const ralphSchema = `{
		"type": "object",
		"properties": {
			"status": {"type": "string", "enum": ["DONE", "CONTINUE"]}
		},
		"required": ["status"],
		"additionalProperties": false
	}`
	sch, err := schema.Parse([]byte(ralphSchema))
	if err != nil {
		t.Fatalf("schema.Parse: %v", err)
	}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{Schema: sch}); err != nil {
		t.Fatalf("Run: %v", err)
	}
	if client.calls != 2 {
		t.Fatalf("client.calls = %d, want 2 (one tool_use + one end_turn)", client.calls)
	}

	lines := splitLines(out.String())
	// Expected: assistant(tool_use), user(tool_result), assistant(text), result
	if len(lines) != 4 {
		t.Fatalf("expected 4 events, got %d: %q", len(lines), out.String())
	}

	// Line 0: assistant with tool_use block.
	var assistantEv struct {
		Type    string `json:"type"`
		Message struct {
			Content []struct {
				Type string `json:"type"`
				ID   string `json:"id"`
			} `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[0]), &assistantEv); err != nil {
		t.Fatalf("unmarshal line 0: %v", err)
	}
	if assistantEv.Type != "assistant" {
		t.Fatalf("line 0 type = %q, want assistant", assistantEv.Type)
	}
	if len(assistantEv.Message.Content) != 1 || assistantEv.Message.Content[0].Type != "tool_use" {
		t.Fatalf("line 0 content = %+v, want one tool_use block", assistantEv.Message.Content)
	}
	toolUseID := assistantEv.Message.Content[0].ID
	if toolUseID != "toolu_01" {
		t.Fatalf("tool_use id = %q, want toolu_01", toolUseID)
	}

	// Line 1: user with tool_result block correlated by id.
	var userEv struct {
		Type    string `json:"type"`
		Message struct {
			Content []struct {
				Type      string `json:"type"`
				ToolUseID string `json:"tool_use_id"`
				IsError   bool   `json:"is_error"`
			} `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &userEv); err != nil {
		t.Fatalf("unmarshal line 1: %v", err)
	}
	if userEv.Type != "user" {
		t.Fatalf("line 1 type = %q, want user", userEv.Type)
	}
	if len(userEv.Message.Content) != 1 || userEv.Message.Content[0].Type != "tool_result" {
		t.Fatalf("line 1 content = %+v, want one tool_result block", userEv.Message.Content)
	}
	if userEv.Message.Content[0].ToolUseID != toolUseID {
		t.Fatalf("tool_result tool_use_id = %q, want %q", userEv.Message.Content[0].ToolUseID, toolUseID)
	}
	if userEv.Message.Content[0].IsError {
		t.Fatalf("tool_result is_error = true; Read should have succeeded")
	}

	// Line 2: second assistant with text.
	var assistantEv2 struct {
		Type string `json:"type"`
	}
	if err := json.Unmarshal([]byte(lines[2]), &assistantEv2); err != nil {
		t.Fatalf("unmarshal line 2: %v", err)
	}
	if assistantEv2.Type != "assistant" {
		t.Fatalf("line 2 type = %q, want assistant", assistantEv2.Type)
	}

	// Line 3: result with schema-conforming structured_output.
	var resultEv struct {
		Type             string          `json:"type"`
		StructuredOutput json.RawMessage `json:"structured_output"`
		IsError          bool            `json:"is_error"`
	}
	if err := json.Unmarshal([]byte(lines[3]), &resultEv); err != nil {
		t.Fatalf("unmarshal line 3: %v", err)
	}
	if resultEv.Type != "result" {
		t.Fatalf("line 3 type = %q, want result", resultEv.Type)
	}
	if resultEv.IsError {
		t.Fatalf("result is_error = true; structured_output = %s", resultEv.StructuredOutput)
	}
	var payload map[string]any
	if err := json.Unmarshal(resultEv.StructuredOutput, &payload); err != nil {
		t.Fatalf("unmarshal structured_output: %v", err)
	}
	if payload["status"] != "DONE" {
		t.Fatalf("structured_output = %v, want {status:DONE}", payload)
	}

	if !sess.Finished() {
		t.Fatalf("session not finished after Run")
	}
}

// R-DPI6-73NQ: Bash tool_result user events carry a tool_use_result sidecar
// with stdout, stderr, and interrupted fields. Verified by running a Bash
// tool_use through the agent loop and asserting the user event on stdout
// carries the correct top-level sidecar object.
func TestR_DPI6_73NQ_BashSidecarInUserEvent(t *testing.T) {
	toolInput, err := json.Marshal(map[string]string{
		"command": "echo hello-stdout; echo hello-stderr 1>&2",
	})
	if err != nil {
		t.Fatalf("marshal tool input: %v", err)
	}

	client := &sequenceClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_bash_01", Name: "Bash", Input: toolInput},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: `{"status":"DONE"}`},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	lines := splitLines(out.String())
	// Expected: assistant(tool_use), user(tool_result+sidecar), assistant(text), result
	if len(lines) != 4 {
		t.Fatalf("expected 4 events, got %d: %q", len(lines), out.String())
	}

	// Line 1: user event must carry the Bash sidecar at top level.
	var userEv struct {
		Type    string `json:"type"`
		Message struct {
			Content []struct {
				Type      string `json:"type"`
				ToolUseID string `json:"tool_use_id"`
			} `json:"content"`
		} `json:"message"`
		ToolUseResult *struct {
			Stdout      string `json:"stdout"`
			Stderr      string `json:"stderr"`
			Interrupted bool   `json:"interrupted"`
		} `json:"tool_use_result"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &userEv); err != nil {
		t.Fatalf("unmarshal user event: %v", err)
	}
	if userEv.Type != "user" {
		t.Fatalf("line 1 type = %q, want user", userEv.Type)
	}
	if len(userEv.Message.Content) != 1 || userEv.Message.Content[0].Type != "tool_result" {
		t.Fatalf("line 1 content = %+v, want one tool_result block", userEv.Message.Content)
	}
	if userEv.Message.Content[0].ToolUseID != "toolu_bash_01" {
		t.Fatalf("tool_use_id = %q, want toolu_bash_01", userEv.Message.Content[0].ToolUseID)
	}
	if userEv.ToolUseResult == nil {
		t.Fatal("tool_use_result sidecar must be present for Bash tool_result (R-DPI6-73NQ)")
	}
	if !strings.Contains(userEv.ToolUseResult.Stdout, "hello-stdout") {
		t.Errorf("sidecar stdout = %q, want to contain hello-stdout", userEv.ToolUseResult.Stdout)
	}
	if !strings.Contains(userEv.ToolUseResult.Stderr, "hello-stderr") {
		t.Errorf("sidecar stderr = %q, want to contain hello-stderr", userEv.ToolUseResult.Stderr)
	}
	if userEv.ToolUseResult.Interrupted {
		t.Errorf("sidecar interrupted = true, want false for non-timed-out command")
	}
}

// R-YSX3-4AE9: each backend populates the result event's usage,
// total_cost_usd, num_turns, duration_ms, and modelUsage fields.
// This test verifies the full data path: a provider emits EventUsage,
// the agent loop accumulates it, and the result event carries correct values.
func TestR_YSX3_4AE9_BackendUsagePopulatesResultEvent(t *testing.T) {
	client := &fakeClient{events: []provider.Event{
		provider.EventTextDelta{Text: `{"status":"CONTINUE"}`},
		// R-YSX3-4AE9: the backend emits EventUsage before EventDone.
		provider.EventUsage{InputTokens: 100, OutputTokens: 50},
		provider.EventDone{StopReason: "end_turn"},
	}}

	const ralphSchema = `{
		"type": "object",
		"properties": {
			"status": {"type": "string", "enum": ["DONE", "CONTINUE"]}
		},
		"required": ["status"],
		"additionalProperties": false
	}`
	sch, err := schema.Parse([]byte(ralphSchema))
	if err != nil {
		t.Fatalf("schema.Parse: %v", err)
	}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	req := provider.Request{Model: "claude-haiku-4-5"}
	if err := Run(context.Background(), client, sess, req, Options{Schema: sch}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	lines := splitLines(out.String())
	if len(lines) < 2 {
		t.Fatalf("expected at least 2 events (assistant + result), got %d", len(lines))
	}
	resultLine := lines[len(lines)-1]

	var result struct {
		Type         string `json:"type"`
		IsError      bool   `json:"is_error"`
		NumTurns     int    `json:"num_turns"`
		DurationMs   int64  `json:"duration_ms"`
		TotalCostUSD float64 `json:"total_cost_usd"`
		Usage        *struct {
			InputTokens  int `json:"input_tokens"`
			OutputTokens int `json:"output_tokens"`
		} `json:"usage"`
		ModelUsage map[string]struct {
			InputTokens  int     `json:"inputTokens"`
			OutputTokens int     `json:"outputTokens"`
			CostUSD      float64 `json:"costUSD"`
		} `json:"modelUsage"`
	}
	if err := json.Unmarshal([]byte(resultLine), &result); err != nil {
		t.Fatalf("unmarshal result: %v", err)
	}
	if result.Type != "result" {
		t.Fatalf("type = %q, want result", result.Type)
	}
	if result.IsError {
		t.Fatalf("is_error = true, unexpected")
	}
	if result.NumTurns != 1 {
		t.Errorf("num_turns = %d, want 1", result.NumTurns)
	}
	if result.DurationMs < 0 {
		t.Errorf("duration_ms = %d, want >= 0", result.DurationMs)
	}
	if result.Usage == nil {
		t.Fatal("usage must be present when tokens were consumed")
	}
	if result.Usage.InputTokens != 100 {
		t.Errorf("usage.input_tokens = %d, want 100", result.Usage.InputTokens)
	}
	if result.Usage.OutputTokens != 50 {
		t.Errorf("usage.output_tokens = %d, want 50", result.Usage.OutputTokens)
	}
	if result.TotalCostUSD <= 0 {
		t.Errorf("total_cost_usd = %v, want > 0 (computed from pricing registry)", result.TotalCostUSD)
	}
	mu, ok := result.ModelUsage["claude-haiku-4-5"]
	if !ok {
		t.Fatal("modelUsage[claude-haiku-4-5] must be present")
	}
	if mu.InputTokens != 100 {
		t.Errorf("modelUsage inputTokens = %d, want 100", mu.InputTokens)
	}
	if mu.OutputTokens != 50 {
		t.Errorf("modelUsage outputTokens = %d, want 50", mu.OutputTokens)
	}
	if mu.CostUSD <= 0 {
		t.Errorf("modelUsage costUSD = %v, want > 0", mu.CostUSD)
	}
}

// R-FPG8-RKEP: thinking blocks with empty text must be filtered from stdout
// but still preserved in provider history for round-trip. Non-empty thinking
// blocks pass through normally.
func TestR_FPG8_RKEP_EmptyThinkingBlocksFilteredFromStdout(t *testing.T) {
	t.Run("empty_thinking_filtered", func(t *testing.T) {
		client := &fakeClient{events: []provider.Event{
			provider.EventThinking{Text: "", Signature: "sig-opaque"},
			provider.EventTextDelta{Text: `{"status":"DONE"}`},
			provider.EventDone{StopReason: "end_turn"},
		}}

		var out bytes.Buffer
		sess := wire.NewSession(&out)
		if err := Run(context.Background(), client, sess, provider.Request{}, Options{}); err != nil {
			t.Fatalf("Run: %v", err)
		}

		lines := splitLines(out.String())
		if len(lines) != 2 {
			t.Fatalf("expected 2 events (assistant + result), got %d: %q", len(lines), out.String())
		}
		var assistant struct {
			Type    string `json:"type"`
			Message struct {
				Content []map[string]any `json:"content"`
			} `json:"message"`
		}
		if err := json.Unmarshal([]byte(lines[0]), &assistant); err != nil {
			t.Fatalf("unmarshal assistant: %v", err)
		}
		for _, block := range assistant.Message.Content {
			if block["type"] == "thinking" {
				t.Errorf("empty thinking block must not appear on stdout (R-FPG8-RKEP); got block: %v", block)
			}
		}
	})

	t.Run("nonempty_thinking_passes", func(t *testing.T) {
		client := &fakeClient{events: []provider.Event{
			provider.EventThinking{Text: "reasoning...", Signature: "sig-opaque"},
			provider.EventTextDelta{Text: `{"status":"DONE"}`},
			provider.EventDone{StopReason: "end_turn"},
		}}

		var out bytes.Buffer
		sess := wire.NewSession(&out)
		if err := Run(context.Background(), client, sess, provider.Request{}, Options{}); err != nil {
			t.Fatalf("Run: %v", err)
		}

		lines := splitLines(out.String())
		if len(lines) != 2 {
			t.Fatalf("expected 2 events, got %d", len(lines))
		}
		var assistant struct {
			Type    string `json:"type"`
			Message struct {
				Content []map[string]any `json:"content"`
			} `json:"message"`
		}
		if err := json.Unmarshal([]byte(lines[0]), &assistant); err != nil {
			t.Fatalf("unmarshal assistant: %v", err)
		}
		var found bool
		for _, block := range assistant.Message.Content {
			if block["type"] == "thinking" {
				found = true
				if block["thinking"] == "" {
					t.Errorf("thinking block must carry non-empty text when forwarded (R-FPG8-RKEP)")
				}
			}
		}
		if !found {
			t.Error("non-empty thinking block must appear in assistant content (R-FPG8-RKEP)")
		}
	})
}

// R-EW6N-L2M1: when an assistant turn contains N tool_use blocks, ikigai-cli
// emits exactly N user events on stdout, each carrying a single tool_result
// block. Bash results carry a non-nil sidecar; Read results do not.
func TestR_EW6N_L2M1_OneUserEventPerToolResult(t *testing.T) {
	tmp := t.TempDir()
	filePath := tmp + "/data.txt"
	if err := os.WriteFile(filePath, []byte("file-content\n"), 0o644); err != nil {
		t.Fatalf("write temp file: %v", err)
	}

	bashInput, err := json.Marshal(map[string]string{
		"command": "printf sidecar-stdout",
	})
	if err != nil {
		t.Fatalf("marshal bash input: %v", err)
	}
	readInput, err := json.Marshal(map[string]string{"file_path": filePath})
	if err != nil {
		t.Fatalf("marshal read input: %v", err)
	}

	// First provider call: assistant issues two tool_use blocks in one turn.
	// Second provider call: model ends with structured output.
	client := &sequenceClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_bash_01", Name: "Bash", Input: bashInput},
			provider.EventToolUse{ID: "toolu_read_01", Name: "Read", Input: readInput},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: `{"status":"DONE"}`},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{}); err != nil {
		t.Fatalf("Run: %v", err)
	}
	if client.calls != 2 {
		t.Fatalf("client.calls = %d, want 2", client.calls)
	}

	lines := splitLines(out.String())
	// Expected: assistant(2 tool_uses), user(bash result), user(read result),
	// assistant(text), result → 5 events.
	if len(lines) != 5 {
		t.Fatalf("expected 5 events, got %d:\n%s", len(lines), out.String())
	}

	// Line 0: assistant must carry exactly 2 tool_use blocks.
	var assistantEv struct {
		Type    string `json:"type"`
		Message struct {
			Content []struct {
				Type string `json:"type"`
				ID   string `json:"id"`
			} `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[0]), &assistantEv); err != nil {
		t.Fatalf("unmarshal line 0: %v", err)
	}
	if assistantEv.Type != "assistant" {
		t.Fatalf("line 0 type = %q, want assistant", assistantEv.Type)
	}
	if len(assistantEv.Message.Content) != 2 {
		t.Fatalf("line 0 content length = %d, want 2 tool_use blocks", len(assistantEv.Message.Content))
	}
	for i, blk := range assistantEv.Message.Content {
		if blk.Type != "tool_use" {
			t.Errorf("line 0 content[%d].type = %q, want tool_use", i, blk.Type)
		}
	}

	type userEvent struct {
		Type    string `json:"type"`
		Message struct {
			Content []struct {
				Type      string `json:"type"`
				ToolUseID string `json:"tool_use_id"`
			} `json:"content"`
		} `json:"message"`
		ToolUseResult *struct {
			Stdout string `json:"stdout"`
		} `json:"tool_use_result"`
	}

	// Lines 1 and 2: each must be a user event with exactly one tool_result.
	wantIDs := []string{"toolu_bash_01", "toolu_read_01"}
	wantSidecar := []bool{true, false}
	for i, lineIdx := range []int{1, 2} {
		var uev userEvent
		if err := json.Unmarshal([]byte(lines[lineIdx]), &uev); err != nil {
			t.Fatalf("unmarshal line %d: %v", lineIdx, err)
		}
		if uev.Type != "user" {
			t.Fatalf("line %d type = %q, want user", lineIdx, uev.Type)
		}
		if len(uev.Message.Content) != 1 {
			t.Fatalf("line %d content length = %d, want exactly 1 tool_result", lineIdx, len(uev.Message.Content))
		}
		blk := uev.Message.Content[0]
		if blk.Type != "tool_result" {
			t.Errorf("line %d content[0].type = %q, want tool_result", lineIdx, blk.Type)
		}
		if blk.ToolUseID != wantIDs[i] {
			t.Errorf("line %d tool_use_id = %q, want %q", lineIdx, blk.ToolUseID, wantIDs[i])
		}
		hasSidecar := uev.ToolUseResult != nil
		if hasSidecar != wantSidecar[i] {
			t.Errorf("line %d sidecar present = %v, want %v", lineIdx, hasSidecar, wantSidecar[i])
		}
	}

	// Lines 3 and 4: second assistant + result.
	var assistantEv2 struct{ Type string `json:"type"` }
	if err := json.Unmarshal([]byte(lines[3]), &assistantEv2); err != nil {
		t.Fatalf("unmarshal line 3: %v", err)
	}
	if assistantEv2.Type != "assistant" {
		t.Fatalf("line 3 type = %q, want assistant", assistantEv2.Type)
	}
	var resultEv struct {
		Type    string `json:"type"`
		IsError bool   `json:"is_error"`
	}
	if err := json.Unmarshal([]byte(lines[4]), &resultEv); err != nil {
		t.Fatalf("unmarshal line 4: %v", err)
	}
	if resultEv.Type != "result" {
		t.Fatalf("line 4 type = %q, want result", resultEv.Type)
	}
	if resultEv.IsError {
		t.Fatal("result is_error = true, want false")
	}
}

// capturingClient records every provider.Request passed to Stream.
type capturingClient struct {
	sequences        [][]provider.Event
	calls            int
	capturedRequests []provider.Request
}

func (c *capturingClient) Stream(ctx context.Context, req provider.Request) (<-chan provider.Event, error) {
	c.capturedRequests = append(c.capturedRequests, req)
	if c.calls >= len(c.sequences) {
		return nil, &provider.Error{Kind: provider.ErrUnknown, Msg: "capturingClient exhausted"}
	}
	evs := c.sequences[c.calls]
	c.calls++
	ch := make(chan provider.Event, len(evs))
	for _, ev := range evs {
		ch <- ev
	}
	close(ch)
	return ch, nil
}

// R-XQHM-7TKL: when Run is called with req.ResponseSchema set, that schema
// must be present in every client.Stream call — including tool-result
// follow-ups — so provider-native structured-output mode stays engaged for
// the entire iteration.
func TestR_XQHM_7TKL_ResponseSchemaForwardedOnEveryStreamCall(t *testing.T) {
	rawSchema := json.RawMessage(`{"type":"object","properties":{"status":{"type":"string"}},"required":["status"]}`)

	bashInput, err := json.Marshal(map[string]string{"command": "echo hi"})
	if err != nil {
		t.Fatalf("marshal tool input: %v", err)
	}

	// Two-call sequence: first ends with tool_use, second ends with end_turn.
	// Both calls must carry ResponseSchema unchanged.
	client := &capturingClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_01", Name: "Bash", Input: bashInput},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: `{"status":"done"}`},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	req := provider.Request{ResponseSchema: rawSchema}

	if err := Run(context.Background(), client, sess, req, Options{}); err != nil {
		t.Fatalf("Run: %v", err)
	}
	if client.calls != 2 {
		t.Fatalf("client.calls = %d, want 2", client.calls)
	}
	if len(client.capturedRequests) != 2 {
		t.Fatalf("captured %d requests, want 2", len(client.capturedRequests))
	}
	for i, captured := range client.capturedRequests {
		if string(captured.ResponseSchema) != string(rawSchema) {
			t.Errorf("Stream call %d: ResponseSchema = %q, want %q", i, captured.ResponseSchema, rawSchema)
		}
	}
}

// Freeform mode (sch == nil): a non-tool stop must emit a single result
// event whose structured_output is the raw assistant text, with no JSON
// parsing and no error, even when the text is not valid JSON.
func TestFreeformStopEmitsRawText(t *testing.T) {
	const finalText = "All done. I wrote results.txt with the summary."
	client := &fakeClient{events: []provider.Event{
		provider.EventTextDelta{Text: finalText},
		provider.EventDone{StopReason: "end_turn"},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	lines := splitLines(out.String())
	if len(lines) != 2 {
		t.Fatalf("expected 2 events (assistant + result), got %d: %q", len(lines), out.String())
	}

	var result struct {
		Type             string          `json:"type"`
		StructuredOutput json.RawMessage `json:"structured_output"`
		IsError          bool            `json:"is_error"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &result); err != nil {
		t.Fatalf("unmarshal result: %v", err)
	}
	if result.Type != "result" {
		t.Fatalf("type = %q, want result", result.Type)
	}
	if result.IsError {
		t.Fatalf("is_error = true, want false; structured_output=%s", result.StructuredOutput)
	}
	var got string
	if err := json.Unmarshal(result.StructuredOutput, &got); err != nil {
		t.Fatalf("structured_output is not a JSON string (freeform should emit raw text): %v; raw=%s", err, result.StructuredOutput)
	}
	if got != finalText {
		t.Fatalf("result text = %q, want %q", got, finalText)
	}
	if !sess.Finished() {
		t.Fatalf("session not finished after Run")
	}
}

// Freeform mode tolerates empty final text: emit an empty-string result,
// not an error.
func TestFreeformStopAllowsEmptyText(t *testing.T) {
	client := &fakeClient{events: []provider.Event{
		provider.EventDone{StopReason: "end_turn"},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	lines := splitLines(out.String())
	resultLine := lines[len(lines)-1]
	var result struct {
		Type             string          `json:"type"`
		StructuredOutput json.RawMessage `json:"structured_output"`
		IsError          bool            `json:"is_error"`
	}
	if err := json.Unmarshal([]byte(resultLine), &result); err != nil {
		t.Fatalf("unmarshal result: %v", err)
	}
	if result.Type != "result" || result.IsError {
		t.Fatalf("result = %+v, want non-error result", result)
	}
	var got string
	if err := json.Unmarshal(result.StructuredOutput, &got); err != nil {
		t.Fatalf("structured_output not a JSON string: %v; raw=%s", err, result.StructuredOutput)
	}
	if got != "" {
		t.Fatalf("result text = %q, want empty string", got)
	}
}

// fakeToolSource is a test ToolSource. It owns a fixed set of
// service-prefixed tool names, advertises one descriptor per owned name,
// and returns a canned text result (or a Go error, when errOnDispatch is
// set) from Dispatch. It records the (name, input) pairs it was asked to
// dispatch so tests can assert routing.
type fakeToolSource struct {
	owned         map[string]bool
	resultText    string
	errOnDispatch error
	dispatched    []struct {
		name  string
		input json.RawMessage
	}
}

func (f *fakeToolSource) Descriptors() []provider.Tool {
	var ds []provider.Tool
	for name := range f.owned {
		ds = append(ds, provider.Tool{
			Name:        name,
			InputSchema: json.RawMessage(`{"type":"object"}`),
		})
	}
	return ds
}

func (f *fakeToolSource) Owns(name string) bool { return f.owned[name] }

func (f *fakeToolSource) Dispatch(ctx context.Context, name string, input json.RawMessage) (wire.ToolResultBlock, error) {
	f.dispatched = append(f.dispatched, struct {
		name  string
		input json.RawMessage
	}{name, input})
	if f.errOnDispatch != nil {
		return wire.ToolResultBlock{}, f.errOnDispatch
	}
	// Deliberately leave ToolUseID empty: the loop is responsible for
	// attaching the tool_use id to source-dispatched results.
	b, err := wire.NewToolResultBlock("", false, f.resultText)
	return b, err
}

// Phase 4: when opts.Tools is non-nil, Run advertises the source's
// descriptors on the very first provider request, alongside any caller-
// supplied built-ins already present in req.Tools.
func TestToolSource_DescriptorsAdvertisedOnRequest(t *testing.T) {
	src := &fakeToolSource{
		owned:      map[string]bool{"ikigenba_crm_search": true},
		resultText: "ok",
	}

	client := &capturingClient{sequences: [][]provider.Event{
		{
			provider.EventTextDelta{Text: "done"},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	// A caller-supplied built-in descriptor must survive alongside the source's.
	req := provider.Request{Tools: []provider.Tool{{Name: "Read", InputSchema: json.RawMessage(`{}`)}}}

	if err := Run(context.Background(), client, sess, req, Options{Tools: src}); err != nil {
		t.Fatalf("Run: %v", err)
	}
	if len(client.capturedRequests) != 1 {
		t.Fatalf("captured %d requests, want 1", len(client.capturedRequests))
	}
	names := map[string]bool{}
	for _, tool := range client.capturedRequests[0].Tools {
		names[tool.Name] = true
	}
	if !names["Read"] {
		t.Errorf("caller-supplied built-in Read missing from req.Tools; got %v", names)
	}
	if !names["ikigenba_crm_search"] {
		t.Errorf("source descriptor ikigenba_crm_search missing from req.Tools; got %v", names)
	}
}

// Phase 4: a tool_use whose name the source Owns is dispatched through the
// source's Dispatch, and the resulting tool_result carries the correct
// tool_use id (attached by the loop) and the source's text.
func TestToolSource_OwnedToolRoutedThroughSource(t *testing.T) {
	src := &fakeToolSource{
		owned:      map[string]bool{"ikigenba_crm_search": true},
		resultText: "crm-says-hello",
	}

	toolInput := json.RawMessage(`{"q":"acme"}`)
	client := &sequenceClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_crm_01", Name: "ikigenba_crm_search", Input: toolInput},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: "done"},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{Tools: src}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	// The source must have been asked to dispatch the owned tool by name+input.
	if len(src.dispatched) != 1 {
		t.Fatalf("source dispatched %d tools, want 1", len(src.dispatched))
	}
	if src.dispatched[0].name != "ikigenba_crm_search" {
		t.Errorf("dispatched name = %q, want ikigenba_crm_search", src.dispatched[0].name)
	}
	if string(src.dispatched[0].input) != string(toolInput) {
		t.Errorf("dispatched input = %s, want %s", src.dispatched[0].input, toolInput)
	}

	lines := splitLines(out.String())
	// assistant(tool_use), user(tool_result), assistant(text), result
	if len(lines) != 4 {
		t.Fatalf("expected 4 events, got %d: %q", len(lines), out.String())
	}
	var userEv struct {
		Type    string `json:"type"`
		Message struct {
			Content []struct {
				Type      string `json:"type"`
				ToolUseID string `json:"tool_use_id"`
				IsError   bool   `json:"is_error"`
				Content   string `json:"content"`
			} `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &userEv); err != nil {
		t.Fatalf("unmarshal user event: %v", err)
	}
	if len(userEv.Message.Content) != 1 || userEv.Message.Content[0].Type != "tool_result" {
		t.Fatalf("user content = %+v, want one tool_result", userEv.Message.Content)
	}
	blk := userEv.Message.Content[0]
	if blk.ToolUseID != "toolu_crm_01" {
		t.Errorf("tool_result tool_use_id = %q, want toolu_crm_01 (loop must attach id)", blk.ToolUseID)
	}
	if blk.IsError {
		t.Errorf("tool_result is_error = true, want false")
	}
	if blk.Content != "crm-says-hello" {
		t.Errorf("tool_result content = %q, want crm-says-hello", blk.Content)
	}
}

// Phase 4: a tool_use with a built-in name the source does NOT own still
// goes through the built-in tools.Dispatch path, unaffected by the
// presence of opts.Tools. The source's Dispatch is never called.
func TestToolSource_UnownedToolFallsThroughToBuiltins(t *testing.T) {
	tmp := t.TempDir()
	filePath := tmp + "/hello.txt"
	if err := os.WriteFile(filePath, []byte("hello world\n"), 0o644); err != nil {
		t.Fatalf("write temp file: %v", err)
	}
	readInput, err := json.Marshal(map[string]string{"file_path": filePath})
	if err != nil {
		t.Fatalf("marshal read input: %v", err)
	}

	src := &fakeToolSource{
		owned:      map[string]bool{"ikigenba_crm_search": true},
		resultText: "should-not-appear",
	}

	client := &sequenceClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_read_01", Name: "Read", Input: readInput},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: "done"},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{Tools: src}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	if len(src.dispatched) != 0 {
		t.Fatalf("source Dispatch was called %d times for an unowned tool, want 0", len(src.dispatched))
	}

	lines := splitLines(out.String())
	if len(lines) != 4 {
		t.Fatalf("expected 4 events, got %d: %q", len(lines), out.String())
	}
	var userEv struct {
		Message struct {
			Content []struct {
				ToolUseID string `json:"tool_use_id"`
				IsError   bool   `json:"is_error"`
				Content   string `json:"content"`
			} `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &userEv); err != nil {
		t.Fatalf("unmarshal user event: %v", err)
	}
	if len(userEv.Message.Content) != 1 {
		t.Fatalf("user content len = %d, want 1", len(userEv.Message.Content))
	}
	if userEv.Message.Content[0].ToolUseID != "toolu_read_01" {
		t.Errorf("tool_use_id = %q, want toolu_read_01", userEv.Message.Content[0].ToolUseID)
	}
	if userEv.Message.Content[0].IsError {
		t.Errorf("built-in Read tool_result is_error = true, want false")
	}
	if !strings.Contains(userEv.Message.Content[0].Content, "hello world") {
		t.Errorf("built-in Read content = %q, want it to contain the file body", userEv.Message.Content[0].Content)
	}
}

// Phase 4 / decision #9: when the source's Dispatch returns a Go error,
// the loop converts it into an is_error tool_result for that tool and the
// run CONTINUES (Run does not return the error).
func TestToolSource_DispatchErrorIsNonFatal(t *testing.T) {
	src := &fakeToolSource{
		owned:         map[string]bool{"ikigenba_crm_search": true},
		errOnDispatch: fmt.Errorf("boom from source"),
	}

	client := &sequenceClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_crm_01", Name: "ikigenba_crm_search", Input: json.RawMessage(`{}`)},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: "recovered"},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{Tools: src}); err != nil {
		t.Fatalf("Run returned error %v; a source Dispatch error must be non-fatal", err)
	}
	if client.calls != 2 {
		t.Fatalf("client.calls = %d, want 2 (run must continue past the failed tool)", client.calls)
	}

	lines := splitLines(out.String())
	if len(lines) != 4 {
		t.Fatalf("expected 4 events, got %d: %q", len(lines), out.String())
	}
	var userEv struct {
		Message struct {
			Content []struct {
				Type      string `json:"type"`
				ToolUseID string `json:"tool_use_id"`
				IsError   bool   `json:"is_error"`
				Content   string `json:"content"`
			} `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &userEv); err != nil {
		t.Fatalf("unmarshal user event: %v", err)
	}
	if len(userEv.Message.Content) != 1 || userEv.Message.Content[0].Type != "tool_result" {
		t.Fatalf("user content = %+v, want one tool_result", userEv.Message.Content)
	}
	blk := userEv.Message.Content[0]
	if !blk.IsError {
		t.Errorf("tool_result is_error = false, want true for a failed source dispatch")
	}
	if blk.ToolUseID != "toolu_crm_01" {
		t.Errorf("tool_result tool_use_id = %q, want toolu_crm_01", blk.ToolUseID)
	}
	if !strings.Contains(blk.Content, "boom from source") {
		t.Errorf("tool_result content = %q, want it to mention the source error", blk.Content)
	}
	// Final result event must be a clean (non-error) end_turn.
	var resultEv struct {
		Type    string `json:"type"`
		IsError bool   `json:"is_error"`
	}
	if err := json.Unmarshal([]byte(lines[3]), &resultEv); err != nil {
		t.Fatalf("unmarshal result: %v", err)
	}
	if resultEv.Type != "result" || resultEv.IsError {
		t.Fatalf("final event = %+v, want non-error result", resultEv)
	}
}

// Phase 4: with opts.Tools == nil, no descriptors are added to req.Tools
// and tool dispatch is byte-identical to the pre-Phase-4 built-in path.
func TestToolSource_NilLeavesRequestAndDispatchUnchanged(t *testing.T) {
	tmp := t.TempDir()
	filePath := tmp + "/hello.txt"
	if err := os.WriteFile(filePath, []byte("nil-path\n"), 0o644); err != nil {
		t.Fatalf("write temp file: %v", err)
	}
	readInput, err := json.Marshal(map[string]string{"file_path": filePath})
	if err != nil {
		t.Fatalf("marshal read input: %v", err)
	}

	client := &capturingClient{sequences: [][]provider.Event{
		{
			provider.EventToolUse{ID: "toolu_read_01", Name: "Read", Input: readInput},
			provider.EventDone{StopReason: "tool_use"},
		},
		{
			provider.EventTextDelta{Text: "done"},
			provider.EventDone{StopReason: "end_turn"},
		},
	}}

	var out bytes.Buffer
	sess := wire.NewSession(&out)
	if err := Run(context.Background(), client, sess, provider.Request{}, Options{Tools: nil}); err != nil {
		t.Fatalf("Run: %v", err)
	}

	// No descriptors added to any request.
	for i, captured := range client.capturedRequests {
		if len(captured.Tools) != 0 {
			t.Errorf("request %d has %d tools, want 0 (nil Tools must not advertise)", i, len(captured.Tools))
		}
	}

	lines := splitLines(out.String())
	if len(lines) != 4 {
		t.Fatalf("expected 4 events, got %d: %q", len(lines), out.String())
	}
	var userEv struct {
		Message struct {
			Content []struct {
				ToolUseID string `json:"tool_use_id"`
				IsError   bool   `json:"is_error"`
				Content   string `json:"content"`
			} `json:"content"`
		} `json:"message"`
	}
	if err := json.Unmarshal([]byte(lines[1]), &userEv); err != nil {
		t.Fatalf("unmarshal user event: %v", err)
	}
	if len(userEv.Message.Content) != 1 {
		t.Fatalf("user content len = %d, want 1", len(userEv.Message.Content))
	}
	if userEv.Message.Content[0].ToolUseID != "toolu_read_01" || userEv.Message.Content[0].IsError {
		t.Errorf("built-in dispatch changed under nil Tools: %+v", userEv.Message.Content[0])
	}
	if !strings.Contains(userEv.Message.Content[0].Content, "nil-path") {
		t.Errorf("built-in Read content = %q, want it to contain the file body", userEv.Message.Content[0].Content)
	}
}

func splitLines(s string) []string {
	s = strings.TrimRight(s, "\n")
	if s == "" {
		return nil
	}
	return strings.Split(s, "\n")
}
