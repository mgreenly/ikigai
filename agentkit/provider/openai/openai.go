// Package openai implements [provider.Client] against the first-party
// OpenAI Responses API.
//
// R-WWTI-LSSO: this backend posts to https://api.openai.com/v1/responses
// using Server-Sent Events for streaming. The Chat Completions endpoint
// is not used.
//
// R-0W9B-7E8I: authentication uses OPENAI_API_KEY as a bearer credential
// via "Authorization: Bearer <key>". No org/project headers, no Azure.
package openai

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"strings"

	"agentkit/provider"
	"agentkit/trace"
)

const (
	defaultBaseURL = "https://api.openai.com"
	responsesPath  = "/v1/responses"
)

// Client is the OpenAI Responses API backend.
type Client struct {
	apiKey  string
	model   string
	baseURL string
	http    *http.Client
	// R-92NN-7DNI: optional trace writer; nil means no tracing.
	tracer *trace.Tracer
}

// SetTracer attaches t to the client. Subsequent Stream calls emit
// HTTP_REQ, HTTP_RESP, and SSE trace lines to t.
func (c *Client) SetTracer(t *trace.Tracer) {
	c.tracer = t
}

// SetBaseURL overrides the default API base URL. Used in tests to redirect
// to a local httptest server.
func (c *Client) SetBaseURL(u string) {
	c.baseURL = u
}

// New constructs a [Client]. apiKey must be the value of the
// OPENAI_API_KEY env var; an empty key is rejected so that a
// missing credential surfaces here rather than as a 401 from the API.
func New(apiKey, model string) (*Client, error) {
	if apiKey == "" {
		return nil, fmt.Errorf("openai: OPENAI_API_KEY is required")
	}
	if model == "" {
		return nil, fmt.Errorf("openai: model is required")
	}
	return &Client{
		apiKey:  apiKey,
		model:   model,
		baseURL: defaultBaseURL,
		http:    &http.Client{},
	}, nil
}

// Stream issues a streaming Responses API call and returns a channel
// of normalized [provider.Event] values. The channel closes when the
// stream ends (cleanly or otherwise); on a non-2xx HTTP response
// Stream returns a typed *[provider.Error] before any goroutine starts.
//
// R-WWTI-LSSO: POST /v1/responses with stream:true.
func (c *Client) Stream(ctx context.Context, req provider.Request) (<-chan provider.Event, error) {
	effort := req.Effort
	// Safety-net: if effort is still empty for gpt-5.5, default to "medium"
	// per R-22XS-LD6T (the driver should apply DefaultEffort first).
	if effort == "" && c.model == "gpt-5.5" {
		effort = "medium"
	}

	body, err := json.Marshal(buildPayload(c.model, effort, req))
	if err != nil {
		return nil, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "marshal request"}
	}

	endpoint, err := url.JoinPath(c.baseURL, responsesPath)
	if err != nil {
		return nil, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "build endpoint"}
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, endpoint, bytes.NewReader(body))
	if err != nil {
		return nil, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "build request"}
	}
	// R-0W9B-7E8I: bearer auth, no x-api-key, no org/project headers.
	httpReq.Header.Set("Authorization", "Bearer "+c.apiKey)
	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Accept", "text/event-stream")

	c.tracer.LogRequest(httpReq.Method, httpReq.URL.String(), httpReq.Header, body)

	resp, err := c.http.Do(httpReq)
	if err != nil {
		return nil, mapTransportError(err)
	}

	if resp.StatusCode != http.StatusOK {
		errBody, _ := io.ReadAll(resp.Body)
		resp.Body.Close()
		c.tracer.LogResponse(resp.StatusCode, resp.Header, errBody)
		return nil, mapErrorBody(resp.StatusCode, errBody)
	}

	c.tracer.LogResponse(resp.StatusCode, resp.Header, nil)

	out := make(chan provider.Event)
	go func() {
		defer close(out)
		defer resp.Body.Close()
		p := &sseParser{ctx: ctx, out: out, items: map[int]*itemState{}, tracer: c.tracer}
		p.run(resp.Body)
	}()
	return out, nil
}

// buildPayload translates a [provider.Request] into the JSON shape the
// Responses API expects.
//
// R-5RUU-AD0I: system prompt goes in the top-level instructions field.
// R-3D9Z-4ND7: store:false and include:["reasoning.encrypted_content"].
// R-4JYG-IMBI: reasoning.summary is not requested.
// R-3Z86-0IPP: structured output via text.format.json_schema with strict:true.
// R-2RBS-8S0P: tool definitions use type:"function" with strict:true.
func buildPayload(model, effort string, req provider.Request) map[string]any {
	payload := map[string]any{
		"model":   model,
		"input":   translateMessages(req.Messages),
		"stream":  true,
		"store":   false,                                    // R-3D9Z-4ND7
		"include": []string{"reasoning.encrypted_content"}, // R-3D9Z-4ND7
	}
	// Port delta: honor this repo's Request.MaxTokens as max_output_tokens.
	// A zero value lets OpenAI apply its own default rather than pinning a
	// low cap; the driver normally resolves a concrete ceiling upstream.
	if req.MaxTokens > 0 {
		payload["max_output_tokens"] = req.MaxTokens
	}
	if req.SystemPrompt != "" {
		payload["instructions"] = req.SystemPrompt // R-5RUU-AD0I
	}
	if effort != "" {
		// R-4JYG-IMBI: only effort is set; summary is intentionally omitted.
		payload["reasoning"] = map[string]any{"effort": effort}
	}
	if len(req.Tools) > 0 {
		payload["tools"] = translateTools(req.Tools)
		payload["tool_choice"] = "auto"
	}
	if len(req.ResponseSchema) > 0 {
		// R-3Z86-0IPP: forward schema verbatim into text.format with strict:true.
		payload["text"] = map[string]any{
			"format": map[string]any{
				"type":   "json_schema",
				"name":   "response",
				"strict": true,
				"schema": req.ResponseSchema,
			},
		}
	}
	return payload
}

// translateMessages converts provider-neutral messages into the Responses
// API input array. Each block becomes one or more typed input items.
func translateMessages(msgs []provider.Message) []any {
	var out []any
	for _, m := range msgs {
		switch m.Role {
		case provider.RoleUser:
			out = append(out, translateUserBlocks(m.Blocks)...)
		case provider.RoleAssistant:
			out = append(out, translateAssistantBlocks(m.Blocks)...)
		}
	}
	return out
}

// translateUserBlocks maps user-turn blocks into Responses API input items.
// TextBlocks become message/user items; ToolResultBlocks become
// function_call_output items. R-2RBS-8S0P.
func translateUserBlocks(blocks []provider.Block) []any {
	var out []any
	var content []any

	flushText := func() {
		if len(content) > 0 {
			out = append(out, map[string]any{
				"type":    "message",
				"role":    "user",
				"content": content,
			})
			content = nil
		}
	}

	for _, b := range blocks {
		switch v := b.(type) {
		case provider.TextBlock:
			content = append(content, map[string]any{
				"type": "input_text",
				"text": v.Text,
			})
		case provider.ToolResultBlock:
			flushText()
			// R-2RBS-8S0P: tool result → function_call_output using call_id.
			out = append(out, map[string]any{
				"type":    "function_call_output",
				"call_id": v.ToolUseID,
				"output":  v.Content,
			})
		}
	}
	flushText()
	return out
}

// translateAssistantBlocks maps assistant-turn blocks back into Responses
// API input items for multi-turn conversation history.
//
// R-2RBS-8S0P: ToolUseBlock → function_call item (call_id = ToolUseBlock.ID).
// R-3D9Z-4ND7: ThinkingBlock → reasoning item with encrypted_content.
//
//	ThinkingBlock.Text carries the reasoning item's id field;
//	ThinkingBlock.Signature carries encrypted_content.
func translateAssistantBlocks(blocks []provider.Block) []any {
	var out []any
	var content []any

	flushText := func() {
		if len(content) > 0 {
			out = append(out, map[string]any{
				"type":    "message",
				"role":    "assistant",
				"content": content,
			})
			content = nil
		}
	}

	for _, b := range blocks {
		switch v := b.(type) {
		case provider.TextBlock:
			content = append(content, map[string]any{
				"type": "output_text",
				"text": v.Text,
			})
		case provider.ThinkingBlock:
			flushText()
			// R-3D9Z-4ND7: round-trip reasoning item unchanged.
			// Text = reasoning id (e.g. "rs_..."), Signature = encrypted_content.
			out = append(out, map[string]any{
				"type":              "reasoning",
				"id":                v.Text,
				"summary":           []any{},
				"encrypted_content": v.Signature,
			})
		case provider.ToolUseBlock:
			flushText()
			// R-2RBS-8S0P: ToolUseBlock.ID is the call_id. The function_call
			// item requires a separate id; we derive it as "fc_"+call_id since
			// we don't store the original fc_* id (it is only needed here).
			input := string(v.Input)
			if input == "" {
				input = "{}"
			}
			out = append(out, map[string]any{
				"type":      "function_call",
				"id":        "fc_" + v.ID,
				"call_id":   v.ID,
				"name":      v.Name,
				"arguments": input,
			})
		}
	}
	flushText()
	return out
}

// translateTools maps provider-neutral tool descriptors to OpenAI's
// function-tool shape with strict:true. R-2RBS-8S0P.
func translateTools(tools []provider.Tool) []any {
	out := make([]any, 0, len(tools))
	for _, t := range tools {
		schema := json.RawMessage(t.InputSchema)
		if len(schema) == 0 {
			schema = json.RawMessage(`{"type":"object","properties":{},"required":[],"additionalProperties":false}`)
		} else {
			// R-3V3G-PYML (instance of R-3959-U3A3): adapt neutral schema to
			// OpenAI strict-mode shape before transmission.
			schema = adaptSchemaForStrictMode(schema)
		}
		out = append(out, map[string]any{
			"type":       "function",
			"name":       t.Name,
			"parameters": schema,
			"strict":     true,
		})
	}
	return out
}

// adaptSchemaForStrictMode rewrites a JSON Schema to comply with
// OpenAI Responses API strict-mode requirements: every object level
// must declare additionalProperties:false, every property must appear
// in required, and properties absent from the neutral required list are
// expressed as nullable union types so callers may pass null to omit
// them. Nested object schemas (in properties values or items) are
// adapted recursively. R-3V3G-PYML.
func adaptSchemaForStrictMode(raw json.RawMessage) json.RawMessage {
	if len(raw) == 0 {
		return raw
	}
	var m map[string]any
	if err := json.Unmarshal(raw, &m); err != nil {
		return raw
	}
	out, err := json.Marshal(adaptSchemaNode(m))
	if err != nil {
		return raw
	}
	return json.RawMessage(out)
}

// adaptSchemaNode recursively applies strict-mode rules to a single
// decoded schema node. Returns a new map; the input is not modified.
func adaptSchemaNode(node map[string]any) map[string]any {
	result := make(map[string]any, len(node))
	for k, v := range node {
		result[k] = v
	}

	typ, _ := node["type"].(string)
	if typ == "object" {
		props, hasProps := node["properties"].(map[string]any)

		// Build the set of properties already declared required.
		requiredSet := map[string]bool{}
		if reqList, ok := node["required"].([]any); ok {
			for _, r := range reqList {
				if s, ok := r.(string); ok {
					requiredSet[s] = true
				}
			}
		}

		if hasProps {
			newProps := make(map[string]any, len(props))
			allNames := make([]any, 0, len(props))
			for name, propSchema := range props {
				allNames = append(allNames, name)
				if propMap, ok := propSchema.(map[string]any); ok {
					adapted := adaptSchemaNode(propMap)
					if !requiredSet[name] {
						adapted = makeNullable(adapted)
					}
					newProps[name] = adapted
				} else {
					newProps[name] = propSchema
				}
			}
			result["properties"] = newProps
			// Every property must be in required.
			result["required"] = allNames
		}
		// Every object level must forbid additional properties.
		result["additionalProperties"] = false
	}

	// Recursively adapt array item schemas.
	if items, ok := node["items"].(map[string]any); ok {
		result["items"] = adaptSchemaNode(items)
	}

	return result
}

// makeNullable extends a property schema so its type includes null,
// allowing the caller to omit the property by passing null explicitly.
func makeNullable(prop map[string]any) map[string]any {
	result := make(map[string]any, len(prop))
	for k, v := range prop {
		result[k] = v
	}
	switch t := prop["type"].(type) {
	case string:
		if t != "null" {
			result["type"] = []any{t, "null"}
		}
	case []any:
		for _, item := range t {
			if item == "null" {
				return result
			}
		}
		result["type"] = append(append([]any{}, t...), "null")
	}
	return result
}

// itemState accumulates per-output-item SSE delta state.
type itemState struct {
	typ    string
	id     string
	callID string
	name   string
	// argsJSON accumulates function_call_arguments.delta fragments.
	argsJSON strings.Builder
}

// sseParser turns the Responses API SSE event stream into normalized
// [provider.Event] values. R-WWTI-LSSO.
type sseParser struct {
	ctx          context.Context
	out          chan<- provider.Event
	items        map[int]*itemState
	hadToolUse   bool
	inputTokens  int
	outputTokens int
	tracer       *trace.Tracer
}

func (p *sseParser) emit(e provider.Event) bool {
	select {
	case p.out <- e:
		return true
	case <-p.ctx.Done():
		return false
	}
}

func (p *sseParser) run(r io.Reader) {
	scanner := bufio.NewScanner(r)
	scanner.Buffer(make([]byte, 64*1024), 1024*1024)

	var event string
	var data strings.Builder

	flush := func() {
		if data.Len() == 0 {
			event = ""
			return
		}
		p.tracer.LogSSEPair(event, data.String())
		p.handle(event, data.String())
		event = ""
		data.Reset()
	}

	for scanner.Scan() {
		line := scanner.Text()
		if line == "" {
			flush()
			continue
		}
		if strings.HasPrefix(line, ":") {
			continue
		}
		if rest, ok := strings.CutPrefix(line, "event:"); ok {
			event = strings.TrimSpace(rest)
			continue
		}
		if rest, ok := strings.CutPrefix(line, "data:"); ok {
			rest = strings.TrimPrefix(rest, " ")
			if data.Len() > 0 {
				data.WriteByte('\n')
			}
			data.WriteString(rest)
		}
	}
	flush()
}

func (p *sseParser) handle(event, data string) {
	switch event {
	case "response.output_item.added":
		var d struct {
			OutputIndex int `json:"output_index"`
			Item        struct {
				Type   string `json:"type"`
				ID     string `json:"id"`
				CallID string `json:"call_id"`
				Name   string `json:"name"`
			} `json:"item"`
		}
		if err := json.Unmarshal([]byte(data), &d); err != nil {
			return
		}
		p.items[d.OutputIndex] = &itemState{
			typ:    d.Item.Type,
			id:     d.Item.ID,
			callID: d.Item.CallID,
			name:   d.Item.Name,
		}

	case "response.output_text.delta":
		var d struct {
			Delta string `json:"delta"`
		}
		if err := json.Unmarshal([]byte(data), &d); err != nil {
			return
		}
		p.emit(provider.EventTextDelta{Text: d.Delta})

	case "response.function_call_arguments.delta":
		var d struct {
			OutputIndex int    `json:"output_index"`
			Delta       string `json:"delta"`
		}
		if err := json.Unmarshal([]byte(data), &d); err != nil {
			return
		}
		if item := p.items[d.OutputIndex]; item != nil {
			item.argsJSON.WriteString(d.Delta)
		}

	case "response.output_item.done":
		var d struct {
			OutputIndex int `json:"output_index"`
			Item        struct {
				Type             string `json:"type"`
				ID               string `json:"id"`
				CallID           string `json:"call_id"`
				Name             string `json:"name"`
				Arguments        string `json:"arguments"`
				EncryptedContent string `json:"encrypted_content"`
			} `json:"item"`
		}
		if err := json.Unmarshal([]byte(data), &d); err != nil {
			return
		}
		item := p.items[d.OutputIndex]
		if item == nil {
			return
		}
		delete(p.items, d.OutputIndex)

		switch d.Item.Type {
		case "function_call":
			// R-2RBS-8S0P: function_call item → EventToolUse.
			// Use call_id as EventToolUse.ID so ToolResultBlock.ToolUseID matches.
			argsStr := item.argsJSON.String()
			if argsStr == "" {
				argsStr = d.Item.Arguments
			}
			if argsStr == "" {
				argsStr = "{}"
			}
			var argsJSON json.RawMessage
			if err := json.Unmarshal([]byte(argsStr), &argsJSON); err != nil {
				argsJSON = json.RawMessage(argsStr)
			}
			p.hadToolUse = true
			p.emit(provider.EventToolUse{
				ID:    d.Item.CallID,
				Name:  d.Item.Name,
				Input: argsJSON,
			})

		case "reasoning":
			// R-3D9Z-4ND7: reasoning item → EventThinking.
			// R-4JYG-IMBI: no summary; only encrypted_content is preserved.
			// Text = item id (for round-tripping); Signature = encrypted_content.
			if d.Item.EncryptedContent == "" {
				return
			}
			p.emit(provider.EventThinking{
				Text:      d.Item.ID,
				Signature: d.Item.EncryptedContent,
			})
		}

	case "response.completed":
		// R-ZEVA-05QR: parse input_tokens_details.cached_tokens for CacheReadInputTokens.
		var d struct {
			Response struct {
				Usage struct {
					InputTokens        int `json:"input_tokens"`
					OutputTokens       int `json:"output_tokens"`
					InputTokensDetails struct {
						CachedTokens int `json:"cached_tokens"`
					} `json:"input_tokens_details"`
				} `json:"usage"`
			} `json:"response"`
		}
		if err := json.Unmarshal([]byte(data), &d); err != nil {
			return
		}
		// R-ZEVA-05QR: CacheCreationInputTokens is always 0 — OpenAI has no cache-creation concept.
		p.emit(provider.EventUsage{
			InputTokens:          d.Response.Usage.InputTokens,
			OutputTokens:         d.Response.Usage.OutputTokens,
			CacheReadInputTokens: d.Response.Usage.InputTokensDetails.CachedTokens,
		})
		stopReason := "end_turn"
		if p.hadToolUse {
			stopReason = "tool_use"
		}
		p.emit(provider.EventDone{StopReason: stopReason})

	case "response.failed":
		// In-stream failure: close the channel without EventDone. The agent
		// loop will surface an iteration error after exhausting retries.
		// We don't re-open the error path here because Stream already returned
		// a nil error and the channel is the only signaling surface available.
	}
}

// mapErrorBody reads an HTTP error response body (JSON) and returns a
// typed [provider.Error]. R-574J-S9EP.
func mapErrorBody(statusCode int, body []byte) *provider.Error {
	var resp struct {
		Error struct {
			Type string `json:"type"`
		} `json:"error"`
	}
	if len(body) > 0 {
		_ = json.Unmarshal(body, &resp)
	}
	return mapErrorType(resp.Error.Type, statusCode)
}

// mapErrorType translates an OpenAI error.type string and HTTP status
// into a typed [provider.Error]. R-574J-S9EP.
func mapErrorType(errType string, statusCode int) *provider.Error {
	switch errType {
	case "authentication_error":
		return &provider.Error{Kind: provider.ErrAuth, Msg: "openai rejected credentials"}
	case "invalid_request_error":
		return &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "openai rejected the request"}
	case "rate_limit_error":
		return &provider.Error{Kind: provider.ErrRateLimit, Msg: "openai rate-limited the request"}
	case "server_error", "overloaded_error":
		return &provider.Error{Kind: provider.ErrServer, Msg: "openai server error"}
	}
	// Fall back to HTTP status code. R-574J-S9EP.
	switch {
	case statusCode == http.StatusUnauthorized || statusCode == http.StatusForbidden:
		return &provider.Error{Kind: provider.ErrAuth, Msg: "openai rejected credentials"}
	case statusCode == http.StatusTooManyRequests:
		return &provider.Error{Kind: provider.ErrRateLimit, Msg: "openai rate-limited the request"}
	case statusCode >= 500:
		return &provider.Error{Kind: provider.ErrServer, Msg: "openai server error"}
	case statusCode >= 400:
		return &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "openai rejected the request"}
	default:
		return &provider.Error{Kind: provider.ErrUnknown, Msg: "openai error"}
	}
}

// mapTransportError converts a Go transport error into a typed
// [provider.Error]. R-574J-S9EP.
func mapTransportError(err error) *provider.Error {
	if errors.Is(err, context.DeadlineExceeded) {
		return &provider.Error{Kind: provider.ErrTimeout, Msg: "openai request deadline exceeded"}
	}
	if errors.Is(err, context.Canceled) {
		return &provider.Error{Kind: provider.ErrUnknown, Msg: "openai request canceled"}
	}
	var netErr net.Error
	if errors.As(err, &netErr) && netErr.Timeout() {
		return &provider.Error{Kind: provider.ErrTimeout, Msg: "openai request timed out"}
	}
	return &provider.Error{Kind: provider.ErrUnknown, Msg: "openai transport failure"}
}
