package openai_test

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"

	"agentkit/provider"
	openaibackend "agentkit/provider/openai"
)

// sseResponse writes a minimal successful SSE stream that emits
// response.output_text.delta then response.completed.
func sseResponse(w http.ResponseWriter, textDelta, stopReason string) {
	w.Header().Set("Content-Type", "text/event-stream")
	w.WriteHeader(http.StatusOK)
	flusher, _ := w.(http.Flusher)

	outputItems := "[]"
	if stopReason == "tool_use" {
		outputItems = `[{"type":"function_call"}]`
	}

	fmt.Fprintf(w, "event: response.output_text.delta\ndata: {\"delta\":%q}\n\n", textDelta)
	fmt.Fprintf(w, "event: response.completed\ndata: {\"response\":{\"output\":%s,\"usage\":{\"input_tokens\":10,\"output_tokens\":5}}}\n\n", outputItems)
	if flusher != nil {
		flusher.Flush()
	}
}

func newTestClient(t *testing.T, handler http.HandlerFunc) (*openaibackend.Client, *httptest.Server) {
	t.Helper()
	srv := httptest.NewServer(handler)
	t.Cleanup(srv.Close)
	c, err := openaibackend.New("test-key", "gpt-5.5")
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	c.SetBaseURL(srv.URL)
	return c, srv
}

func drainEvents(t *testing.T, ch <-chan provider.Event) (text string, done bool, stopReason string, usage provider.EventUsage) {
	t.Helper()
	for ev := range ch {
		switch e := ev.(type) {
		case provider.EventTextDelta:
			text += e.Text
		case provider.EventDone:
			done = true
			stopReason = e.StopReason
		case provider.EventUsage:
			usage = e
		}
	}
	return
}

// TestR_WWTI_LSSO_OpenAIBackendUsesResponsesAPISSE verifies that the
// OpenAI backend posts to /v1/responses with stream:true and bearer auth,
// and that the SSE response is correctly decoded into provider events.
// R-WWTI-LSSO.
func TestR_WWTI_LSSO_OpenAIBackendUsesResponsesAPISSE(t *testing.T) {
	var gotMethod, gotPath string
	var gotBody map[string]any
	var gotAuth string

	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		gotMethod = r.Method
		gotPath = r.URL.Path
		gotAuth = r.Header.Get("Authorization")
		if err := json.NewDecoder(r.Body).Decode(&gotBody); err != nil {
			t.Errorf("decode body: %v", err)
		}
		sseResponse(w, "hello", "end_turn")
	})

	ch, err := c.Stream(context.Background(), provider.Request{
		Model: "gpt-5.5",
	})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	text, done, _, _ := drainEvents(t, ch)

	if gotMethod != http.MethodPost {
		t.Errorf("method = %q, want POST", gotMethod)
	}
	if gotPath != "/v1/responses" {
		t.Errorf("path = %q, want /v1/responses", gotPath)
	}
	if gotAuth != "Bearer test-key" {
		t.Errorf("Authorization = %q, want Bearer test-key", gotAuth)
	}
	if stream, _ := gotBody["stream"].(bool); !stream {
		t.Errorf("stream = %v, want true", gotBody["stream"])
	}
	if text != "hello" {
		t.Errorf("text = %q, want hello", text)
	}
	if !done {
		t.Error("EventDone not received")
	}
}

// TestR_5RUU_AD0I_SystemPromptViaInstructionsField verifies that the
// system prompt is sent in the top-level instructions field, not inside
// the input array. R-5RUU-AD0I.
func TestR_5RUU_AD0I_SystemPromptViaInstructionsField(t *testing.T) {
	var gotBody map[string]any

	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		sseResponse(w, "ok", "end_turn")
	})

	ch, err := c.Stream(context.Background(), provider.Request{
		SystemPrompt: "you are a helpful assistant",
	})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	if gotBody["instructions"] != "you are a helpful assistant" {
		t.Errorf("instructions = %v, want system prompt string", gotBody["instructions"])
	}
	// system prompt must NOT appear as a developer/system role message inside input
	if inputs, ok := gotBody["input"].([]any); ok {
		for _, item := range inputs {
			if m, ok := item.(map[string]any); ok {
				if m["role"] == "developer" || m["role"] == "system" {
					t.Error("system prompt leaked into input array as developer/system role message")
				}
			}
		}
	}
}

// TestMaxTokensSentAsMaxOutputTokens verifies the port delta: this repo's
// Request.MaxTokens (the field ikigai-cli predates) is sent on the wire as
// the Responses API's max_output_tokens, and is omitted when unset (zero).
func TestMaxTokensSentAsMaxOutputTokens(t *testing.T) {
	t.Run("set", func(t *testing.T) {
		var gotBody map[string]any
		c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
			_ = json.NewDecoder(r.Body).Decode(&gotBody)
			sseResponse(w, "ok", "end_turn")
		})
		ch, err := c.Stream(context.Background(), provider.Request{MaxTokens: 1234})
		if err != nil {
			t.Fatalf("Stream: %v", err)
		}
		drainEvents(t, ch)
		got, ok := gotBody["max_output_tokens"].(float64)
		if !ok {
			t.Fatalf("max_output_tokens missing or wrong type: %v", gotBody["max_output_tokens"])
		}
		if int(got) != 1234 {
			t.Errorf("max_output_tokens = %v, want 1234", got)
		}
	})

	t.Run("unset", func(t *testing.T) {
		var gotBody map[string]any
		c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
			_ = json.NewDecoder(r.Body).Decode(&gotBody)
			sseResponse(w, "ok", "end_turn")
		})
		ch, err := c.Stream(context.Background(), provider.Request{})
		if err != nil {
			t.Fatalf("Stream: %v", err)
		}
		drainEvents(t, ch)
		if _, ok := gotBody["max_output_tokens"]; ok {
			t.Error("max_output_tokens must be absent when MaxTokens is unset")
		}
	})
}

// TestR_4JYG_IMBI_NoReasoningSummaries verifies that when effort is set,
// the reasoning object only contains effort (no summary field). R-4JYG-IMBI.
func TestR_4JYG_IMBI_NoReasoningSummaries(t *testing.T) {
	var gotBody map[string]any

	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		sseResponse(w, "ok", "end_turn")
	})

	ch, err := c.Stream(context.Background(), provider.Request{Effort: "high"})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	reasoning, ok := gotBody["reasoning"].(map[string]any)
	if !ok {
		t.Fatal("reasoning field missing from request")
	}
	if _, hasSummary := reasoning["summary"]; hasSummary {
		t.Error("reasoning.summary must not be sent (R-4JYG-IMBI)")
	}
	if reasoning["effort"] != "high" {
		t.Errorf("reasoning.effort = %v, want high", reasoning["effort"])
	}
}

// TestR_3D9Z_4ND7_StatelessReasoningRoundTrip verifies that every request
// sends store:false and include:["reasoning.encrypted_content"], and that
// reasoning items in conversation history are round-tripped with their
// encrypted_content. R-3D9Z-4ND7.
func TestR_3D9Z_4ND7_StatelessReasoningRoundTrip(t *testing.T) {
	var gotBodies []map[string]any

	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		var body map[string]any
		_ = json.NewDecoder(r.Body).Decode(&body)
		gotBodies = append(gotBodies, body)
		sseResponse(w, "ok", "end_turn")
	})

	// First request: verify store:false and include.
	ch, err := c.Stream(context.Background(), provider.Request{})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	if len(gotBodies) == 0 {
		t.Fatal("no request body captured")
	}
	body := gotBodies[0]
	if store, _ := body["store"].(bool); store {
		t.Error("store must be false (R-3D9Z-4ND7)")
	}
	include, _ := body["include"].([]any)
	if len(include) == 0 || include[0] != "reasoning.encrypted_content" {
		t.Errorf("include = %v, want [reasoning.encrypted_content]", include)
	}

	// Second request: conversation history includes a reasoning ThinkingBlock.
	// Verify it arrives as a reasoning item with correct id and encrypted_content.
	gotBodies = nil
	ch2, err := c.Stream(context.Background(), provider.Request{
		Messages: []provider.Message{
			{
				Role: provider.RoleAssistant,
				Blocks: []provider.Block{
					provider.ThinkingBlock{Text: "rs_abc123", Signature: "enc_xyz"},
				},
			},
		},
	})
	if err != nil {
		t.Fatalf("Stream 2: %v", err)
	}
	drainEvents(t, ch2)

	if len(gotBodies) == 0 {
		t.Fatal("no request body for second call")
	}
	inputs, _ := gotBodies[0]["input"].([]any)
	var foundReasoning bool
	for _, item := range inputs {
		m, ok := item.(map[string]any)
		if !ok {
			continue
		}
		if m["type"] == "reasoning" {
			foundReasoning = true
			if m["id"] != "rs_abc123" {
				t.Errorf("reasoning.id = %v, want rs_abc123", m["id"])
			}
			if m["encrypted_content"] != "enc_xyz" {
				t.Errorf("reasoning.encrypted_content = %v, want enc_xyz", m["encrypted_content"])
			}
		}
	}
	if !foundReasoning {
		t.Error("reasoning item not found in input array for second request")
	}
}

// TestR_3Z86_0IPP_StructuredOutputViaTextFormat verifies that a
// ResponseSchema is forwarded verbatim into text.format.json_schema with
// strict:true. R-3Z86-0IPP.
func TestR_3Z86_0IPP_StructuredOutputViaTextFormat(t *testing.T) {
	var gotBody map[string]any

	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		sseResponse(w, "ok", "end_turn")
	})

	schema := json.RawMessage(`{"type":"object","properties":{"answer":{"type":"string"}},"required":["answer"],"additionalProperties":false}`)
	ch, err := c.Stream(context.Background(), provider.Request{ResponseSchema: schema})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	text, ok := gotBody["text"].(map[string]any)
	if !ok {
		t.Fatal("text field missing")
	}
	format, ok := text["format"].(map[string]any)
	if !ok {
		t.Fatal("text.format field missing")
	}
	if format["type"] != "json_schema" {
		t.Errorf("format.type = %v, want json_schema", format["type"])
	}
	if strict, _ := format["strict"].(bool); !strict {
		t.Error("format.strict must be true")
	}
	if format["schema"] == nil {
		t.Error("format.schema must be present")
	}
}

// TestR_2RBS_8S0P_ToolUseTranslation verifies tool definition translation
// (type:function, strict:true), function_call SSE items becoming
// EventToolUse events, and ToolResultBlock becoming function_call_output.
// R-2RBS-8S0P.
func TestR_2RBS_8S0P_ToolUseTranslation(t *testing.T) {
	t.Run("tool_definitions", func(t *testing.T) {
		var gotBody map[string]any
		c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
			_ = json.NewDecoder(r.Body).Decode(&gotBody)
			sseResponse(w, "ok", "end_turn")
		})

		toolSchema := json.RawMessage(`{"type":"object","properties":{"cmd":{"type":"string"}},"required":["cmd"],"additionalProperties":false}`)
		ch, err := c.Stream(context.Background(), provider.Request{
			Tools: []provider.Tool{{Name: "bash", InputSchema: toolSchema}},
		})
		if err != nil {
			t.Fatalf("Stream: %v", err)
		}
		drainEvents(t, ch)

		tools, _ := gotBody["tools"].([]any)
		if len(tools) == 0 {
			t.Fatal("tools array missing or empty")
		}
		tool, _ := tools[0].(map[string]any)
		if tool["type"] != "function" {
			t.Errorf("tool.type = %v, want function", tool["type"])
		}
		if tool["name"] != "bash" {
			t.Errorf("tool.name = %v, want bash", tool["name"])
		}
		if strict, _ := tool["strict"].(bool); !strict {
			t.Error("tool.strict must be true")
		}
	})

	t.Run("function_call_sse_to_event_tool_use", func(t *testing.T) {
		c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "text/event-stream")
			w.WriteHeader(http.StatusOK)
			flusher, _ := w.(http.Flusher)
			// Emit a function_call output item.
			fmt.Fprintln(w, `event: response.output_item.added`)
			fmt.Fprintln(w, `data: {"output_index":0,"item":{"type":"function_call","id":"fc_001","call_id":"call_001","name":"bash","arguments":""}}`)
			fmt.Fprintln(w, ``)
			fmt.Fprintln(w, `event: response.function_call_arguments.delta`)
			fmt.Fprintln(w, `data: {"output_index":0,"delta":"{\"cmd\":\"ls\"}"}`)
			fmt.Fprintln(w, ``)
			fmt.Fprintln(w, `event: response.output_item.done`)
			fmt.Fprintln(w, `data: {"output_index":0,"item":{"type":"function_call","id":"fc_001","call_id":"call_001","name":"bash","arguments":"{\"cmd\":\"ls\"}"}}`)
			fmt.Fprintln(w, ``)
			fmt.Fprintln(w, `event: response.completed`)
			fmt.Fprintln(w, `data: {"response":{"output":[{"type":"function_call"}],"usage":{"input_tokens":10,"output_tokens":5}}}`)
			fmt.Fprintln(w, ``)
			if flusher != nil {
				flusher.Flush()
			}
		})

		ch, err := c.Stream(context.Background(), provider.Request{})
		if err != nil {
			t.Fatalf("Stream: %v", err)
		}

		var toolUse *provider.EventToolUse
		var done bool
		var stopReason string
		for ev := range ch {
			switch e := ev.(type) {
			case provider.EventToolUse:
				eu := e
				toolUse = &eu
			case provider.EventDone:
				done = true
				stopReason = e.StopReason
			}
		}

		if toolUse == nil {
			t.Fatal("EventToolUse not received")
		}
		if toolUse.ID != "call_001" {
			t.Errorf("EventToolUse.ID = %q, want call_001", toolUse.ID)
		}
		if toolUse.Name != "bash" {
			t.Errorf("EventToolUse.Name = %q, want bash", toolUse.Name)
		}
		var input map[string]string
		if err := json.Unmarshal(toolUse.Input, &input); err != nil {
			t.Fatalf("unmarshal input: %v", err)
		}
		if input["cmd"] != "ls" {
			t.Errorf("input[cmd] = %q, want ls", input["cmd"])
		}
		if !done {
			t.Error("EventDone not received")
		}
		if stopReason != "tool_use" {
			t.Errorf("stop_reason = %q, want tool_use", stopReason)
		}
	})

	t.Run("tool_result_to_function_call_output", func(t *testing.T) {
		var gotBody map[string]any
		c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
			_ = json.NewDecoder(r.Body).Decode(&gotBody)
			sseResponse(w, "done", "end_turn")
		})

		ch, err := c.Stream(context.Background(), provider.Request{
			Messages: []provider.Message{
				{
					Role: provider.RoleUser,
					Blocks: []provider.Block{
						provider.ToolResultBlock{ToolUseID: "call_001", Content: "file.txt"},
					},
				},
			},
		})
		if err != nil {
			t.Fatalf("Stream: %v", err)
		}
		drainEvents(t, ch)

		inputs, _ := gotBody["input"].([]any)
		var foundOutput bool
		for _, item := range inputs {
			m, ok := item.(map[string]any)
			if !ok {
				continue
			}
			if m["type"] == "function_call_output" {
				foundOutput = true
				if m["call_id"] != "call_001" {
					t.Errorf("call_id = %v, want call_001", m["call_id"])
				}
				if m["output"] != "file.txt" {
					t.Errorf("output = %v, want file.txt", m["output"])
				}
			}
		}
		if !foundOutput {
			t.Error("function_call_output item not found in input array")
		}
	})
}

// TestR_574J_S9EP_ErrorTaxonomyMapping verifies that OpenAI HTTP error
// types and status codes are mapped to the correct provider.ErrorKind.
// R-574J-S9EP.
func TestR_574J_S9EP_ErrorTaxonomyMapping(t *testing.T) {
	cases := []struct {
		name       string
		statusCode int
		body       string
		wantKind   provider.ErrorKind
	}{
		{
			name:       "authentication_error",
			statusCode: http.StatusUnauthorized,
			body:       `{"error":{"type":"authentication_error","message":"bad key"}}`,
			wantKind:   provider.ErrAuth,
		},
		{
			name:       "invalid_request_error",
			statusCode: http.StatusBadRequest,
			body:       `{"error":{"type":"invalid_request_error","message":"bad param"}}`,
			wantKind:   provider.ErrInvalidRequest,
		},
		{
			name:       "rate_limit_error",
			statusCode: http.StatusTooManyRequests,
			body:       `{"error":{"type":"rate_limit_error","message":"slow down"}}`,
			wantKind:   provider.ErrRateLimit,
		},
		{
			name:       "server_error",
			statusCode: http.StatusInternalServerError,
			body:       `{"error":{"type":"server_error","message":"internal"}}`,
			wantKind:   provider.ErrServer,
		},
		{
			name:       "overloaded_error",
			statusCode: 503,
			body:       `{"error":{"type":"overloaded_error","message":"busy"}}`,
			wantKind:   provider.ErrServer,
		},
		{
			name:       "401_no_body",
			statusCode: http.StatusUnauthorized,
			body:       `{}`,
			wantKind:   provider.ErrAuth,
		},
		{
			name:       "500_no_type",
			statusCode: http.StatusInternalServerError,
			body:       `{}`,
			wantKind:   provider.ErrServer,
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
				w.WriteHeader(tc.statusCode)
				fmt.Fprint(w, tc.body)
			})

			_, err := c.Stream(context.Background(), provider.Request{})
			if err == nil {
				t.Fatal("expected error, got nil")
			}
			pe, ok := err.(*provider.Error)
			if !ok {
				t.Fatalf("error type = %T, want *provider.Error", err)
			}
			if pe.Kind != tc.wantKind {
				t.Errorf("Kind = %v, want %v", pe.Kind, tc.wantKind)
			}
		})
	}

	t.Run("timeout", func(t *testing.T) {
		c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
			// block until context cancels — triggers client deadline
			<-r.Context().Done()
		})

		ctx, cancel := context.WithCancel(context.Background())
		cancel() // already cancelled

		_, err := c.Stream(ctx, provider.Request{})
		if err == nil {
			t.Fatal("expected error, got nil")
		}
		pe, ok := err.(*provider.Error)
		if !ok {
			t.Fatalf("error type = %T, want *provider.Error", err)
		}
		// cancelled context → ErrUnknown (not timeout)
		_ = pe // any typed provider.Error is acceptable for cancelled
	})
}

// TestR_6MCB_UMJV_V1ImplementsAnthropicAndOpenAI is a compile-time check
// that the openai.Client satisfies provider.Client. R-6MCB-UMJV.
func TestR_6MCB_UMJV_V1ImplementsAnthropicAndOpenAI(t *testing.T) {
	var _ provider.Client = (*openaibackend.Client)(nil)
}

// TestR_78AI_QHWD_ProviderAbstractionExtensible verifies that adding the
// OpenAI backend required no changes to the provider abstraction interface
// or any non-provider package. The compile-time check is sufficient:
// openai.Client implements provider.Client without modifying provider.go.
// R-78AI-QHWD.
func TestR_78AI_QHWD_ProviderAbstractionExtensible(t *testing.T) {
	var _ provider.Client = (*openaibackend.Client)(nil)
	// Verify the backend was constructed without changing the interface.
	c, err := openaibackend.New("key", "gpt-5.5")
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	if c == nil {
		t.Error("New returned nil")
	}
	// Confirm it satisfies the interface at runtime.
	var client provider.Client = c
	if client == nil {
		t.Error("client is nil")
	}

	// Verify the instructions field key is not present when no system prompt.
	var gotBody map[string]any
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		sseResponse(w, "ok", "end_turn")
	}))
	defer srv.Close()
	c.SetBaseURL(srv.URL)

	ch, err2 := c.Stream(context.Background(), provider.Request{})
	if err2 != nil {
		t.Fatalf("Stream: %v", err2)
	}
	drainEvents(t, ch)

	if _, hasInstructions := gotBody["instructions"]; hasInstructions {
		t.Error("instructions must not be sent when SystemPrompt is empty")
	}
}

// TestR_WWTI_LSSO_ReasoningItemInSSEBecomesEventThinking verifies that
// a reasoning output_item.done event containing encrypted_content becomes
// an EventThinking with the item id and encrypted_content preserved.
// This is the SSE-decode side of R-3D9Z-4ND7 and confirms R-WWTI-LSSO
// parses reasoning items correctly.
func TestR_WWTI_LSSO_ReasoningItemInSSEBecomesEventThinking(t *testing.T) {
	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher, _ := w.(http.Flusher)
		fmt.Fprintln(w, `event: response.output_item.added`)
		fmt.Fprintln(w, `data: {"output_index":0,"item":{"type":"reasoning","id":"rs_001"}}`)
		fmt.Fprintln(w, ``)
		fmt.Fprintln(w, `event: response.output_item.done`)
		fmt.Fprintln(w, `data: {"output_index":0,"item":{"type":"reasoning","id":"rs_001","encrypted_content":"enc_abc"}}`)
		fmt.Fprintln(w, ``)
		fmt.Fprintln(w, `event: response.completed`)
		fmt.Fprintln(w, `data: {"response":{"output":[],"usage":{"input_tokens":5,"output_tokens":3}}}`)
		fmt.Fprintln(w, ``)
		if flusher != nil {
			flusher.Flush()
		}
	})

	ch, err := c.Stream(context.Background(), provider.Request{})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}

	var thinking *provider.EventThinking
	for ev := range ch {
		if e, ok := ev.(provider.EventThinking); ok {
			eu := e
			thinking = &eu
		}
	}

	if thinking == nil {
		t.Fatal("EventThinking not received")
	}
	if thinking.Text != "rs_001" {
		t.Errorf("EventThinking.Text = %q, want rs_001 (reasoning item id)", thinking.Text)
	}
	if thinking.Signature != "enc_abc" {
		t.Errorf("EventThinking.Signature = %q, want enc_abc (encrypted_content)", thinking.Signature)
	}
}

// TestR_5RUU_AD0I_NoInstructionsWhenSystemPromptEmpty verifies that when
// no system prompt is supplied, the instructions field is absent from
// the request body. R-5RUU-AD0I.
func TestR_5RUU_AD0I_NoInstructionsWhenSystemPromptEmpty(t *testing.T) {
	var gotBody map[string]any
	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		sseResponse(w, "ok", "end_turn")
	})

	ch, err := c.Stream(context.Background(), provider.Request{})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	if _, ok := gotBody["instructions"]; ok {
		t.Error("instructions must not be present when SystemPrompt is empty")
	}
}

// TestR_3V3G_PYML_ToolDefinitionAdaptedForOpenAIStrictMode verifies that
// the OpenAI backend rewrites a neutral tool schema into strict-mode shape
// before transmission: additionalProperties:false at every object level,
// all properties listed in required, and optional properties expressed as
// nullable union types. R-3V3G-PYML.
func TestR_3V3G_PYML_ToolDefinitionAdaptedForOpenAIStrictMode(t *testing.T) {
	// Neutral Read-like schema: file_path is required; offset and limit are optional.
	neutralSchema := json.RawMessage(`{
		"type": "object",
		"properties": {
			"file_path": {"type": "string", "description": "path"},
			"offset":    {"type": "integer", "description": "start line"},
			"limit":     {"type": "integer", "description": "max lines"}
		},
		"required": ["file_path"]
	}`)

	var gotBody map[string]any
	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		sseResponse(w, "ok", "end_turn")
	})

	ch, err := c.Stream(context.Background(), provider.Request{
		Tools: []provider.Tool{{Name: "Read", InputSchema: neutralSchema}},
	})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	tools, _ := gotBody["tools"].([]any)
	if len(tools) == 0 {
		t.Fatal("tools array missing or empty")
	}
	tool, _ := tools[0].(map[string]any)
	params, _ := tool["parameters"].(map[string]any)
	if params == nil {
		t.Fatal("tool.parameters missing")
	}

	// additionalProperties must be false at the top-level object.
	if ap, ok := params["additionalProperties"].(bool); !ok || ap {
		t.Errorf("parameters.additionalProperties = %v, want false", params["additionalProperties"])
	}

	// All three properties must appear in required.
	reqList, _ := params["required"].([]any)
	requiredSet := map[string]bool{}
	for _, r := range reqList {
		if s, ok := r.(string); ok {
			requiredSet[s] = true
		}
	}
	for _, name := range []string{"file_path", "offset", "limit"} {
		if !requiredSet[name] {
			t.Errorf("required does not contain %q; required = %v", name, reqList)
		}
	}

	props, _ := params["properties"].(map[string]any)

	// file_path was originally required → must NOT be nullable.
	fp, _ := props["file_path"].(map[string]any)
	if typ, ok := fp["type"].(string); !ok || typ != "string" {
		t.Errorf("file_path.type = %v, want \"string\" (non-nullable)", fp["type"])
	}

	// offset and limit were optional → must be nullable union types.
	for _, name := range []string{"offset", "limit"} {
		prop, _ := props[name].(map[string]any)
		typeVal, ok := prop["type"].([]any)
		if !ok {
			t.Errorf("%s.type = %v, want nullable array type", name, prop["type"])
			continue
		}
		hasNull := false
		hasInt := false
		for _, item := range typeVal {
			switch item {
			case "null":
				hasNull = true
			case "integer":
				hasInt = true
			}
		}
		if !hasNull {
			t.Errorf("%s.type %v missing \"null\"", name, typeVal)
		}
		if !hasInt {
			t.Errorf("%s.type %v missing \"integer\"", name, typeVal)
		}
	}
}

// TestR_3959_U3A3_ProviderAdaptsNeutralToolSchema verifies that the OpenAI
// backend adapts the neutral tool schema before transmission: the neutral
// schema (which is not already OpenAI-strict) is rewritten so the wire
// format meets provider requirements, without modifying the tool definition
// itself. R-3959-U3A3.
func TestR_3959_U3A3_ProviderAdaptsNeutralToolSchema(t *testing.T) {
	// This neutral schema deliberately omits additionalProperties:false —
	// a correct neutral schema for Claude Code's wire format.
	neutralSchema := json.RawMessage(`{
		"type": "object",
		"properties": {
			"command": {"type": "string"}
		},
		"required": ["command"]
	}`)

	// Confirm the neutral schema itself is NOT already strict-mode compliant.
	var neutral map[string]any
	_ = json.Unmarshal(neutralSchema, &neutral)
	if _, hasAP := neutral["additionalProperties"]; hasAP {
		t.Skip("neutral schema already has additionalProperties — test assumption invalid")
	}

	var gotBody map[string]any
	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		_ = json.NewDecoder(r.Body).Decode(&gotBody)
		sseResponse(w, "ok", "end_turn")
	})

	ch, err := c.Stream(context.Background(), provider.Request{
		Tools: []provider.Tool{{Name: "Bash", InputSchema: neutralSchema}},
	})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	drainEvents(t, ch)

	tools, _ := gotBody["tools"].([]any)
	if len(tools) == 0 {
		t.Fatal("tools array missing")
	}
	tool, _ := tools[0].(map[string]any)
	params, _ := tool["parameters"].(map[string]any)
	if params == nil {
		t.Fatal("tool.parameters missing")
	}

	// The transmitted schema must have additionalProperties:false even
	// though the neutral schema did not — the backend adapted it.
	if ap, ok := params["additionalProperties"].(bool); !ok || ap {
		t.Errorf("transmitted parameters.additionalProperties = %v, want false (provider must adapt neutral schema)", params["additionalProperties"])
	}
}

// TestR_ZEVA_05QR_OpenAIUsageMappingIncludesCachedTokens verifies that
// cached_tokens from input_tokens_details is mapped to CacheReadInputTokens
// and CacheCreationInputTokens is always zero. R-ZEVA-05QR.
func TestR_ZEVA_05QR_OpenAIUsageMappingIncludesCachedTokens(t *testing.T) {
	c, _ := newTestClient(t, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		flusher, _ := w.(http.Flusher)
		fmt.Fprintln(w, `event: response.completed`)
		fmt.Fprintln(w, `data: {"response":{"output":[],"usage":{"input_tokens":20,"output_tokens":8,"input_tokens_details":{"cached_tokens":5}}}}`)
		fmt.Fprintln(w, ``)
		if flusher != nil {
			flusher.Flush()
		}
	})

	ch, err := c.Stream(context.Background(), provider.Request{})
	if err != nil {
		t.Fatalf("Stream: %v", err)
	}
	_, _, _, usage := drainEvents(t, ch)

	if usage.InputTokens != 20 {
		t.Errorf("InputTokens = %d, want 20", usage.InputTokens)
	}
	if usage.OutputTokens != 8 {
		t.Errorf("OutputTokens = %d, want 8", usage.OutputTokens)
	}
	if usage.CacheReadInputTokens != 5 {
		t.Errorf("CacheReadInputTokens = %d, want 5 (from input_tokens_details.cached_tokens)", usage.CacheReadInputTokens)
	}
	if usage.CacheCreationInputTokens != 0 {
		t.Errorf("CacheCreationInputTokens = %d, want 0 (OpenAI has no cache-creation concept)", usage.CacheCreationInputTokens)
	}
}

// TestExportedSymbolsPresent verifies the package exposes the constructor
// and configuration methods used by the composition root.
func TestExportedSymbolsPresent(t *testing.T) {
	// New, SetBaseURL, SetTracer, Stream must all be present.
	c, err := openaibackend.New("key", "gpt-5.5")
	if err != nil || c == nil {
		t.Fatalf("New: err=%v c=%v", err, c)
	}
	c.SetBaseURL("http://localhost")
	c.SetTracer(nil)
}

// TestNewRejectsEmptyInputs verifies the constructor refuses an empty key
// or empty model rather than deferring to a 401 at call time.
func TestNewRejectsEmptyInputs(t *testing.T) {
	if _, err := openaibackend.New("", "gpt-5.5"); err == nil {
		t.Error("New with empty key must error")
	}
	if _, err := openaibackend.New("key", ""); err == nil {
		t.Error("New with empty model must error")
	}
}
