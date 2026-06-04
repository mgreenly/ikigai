// Package anthropic implements [provider.Client] against the
// first-party Anthropic Messages API.
//
// R-0LK7-BGEX: this backend talks to Anthropic over HTTPS using the
// Messages API and SSE streaming. It does not delegate to the real
// `claude` binary.
//
// R-18QA-L3I4: authentication uses the ANTHROPIC_API_KEY env var as
// a bearer credential per Anthropic's documented header format
// (`x-api-key` plus `anthropic-version`). No OAuth / Bedrock /
// Vertex routing.
package anthropic

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

	"ralph/internal/engine/provider"
	"ralph/internal/engine/trace"
)

const (
	defaultBaseURL   = "https://api.anthropic.com"
	anthropicVersion = "2023-06-01"
	messagesPath     = "/v1/messages"

	// R-2E6V-LAPQ: 1M-context variants are gated by Anthropic's
	// documented anthropic-beta header. The [1m] suffix on a
	// model ID selects this beta; the suffix is stripped before
	// the model name is sent on the wire.
	contextOneMillionSuffix = "[1m]"
	contextOneMillionBeta   = "context-1m-2025-08-07"

	// defaultMaxTokens is the conservative fallback applied only when a
	// request arrives with MaxTokens unset (<= 0). The driver normally
	// resolves a concrete ceiling from the session config / model
	// registry, so this is a safety net to keep the required Anthropic
	// field present, not the effective default.
	defaultMaxTokens = 4096
)

// Client is the Anthropic Messages API backend.
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

// New constructs a [Client]. apiKey must be the value of the
// ANTHROPIC_API_KEY env var; an empty key is rejected so that a
// missing credential surfaces here rather than as a 401 from the
// API.
func New(apiKey, model string) (*Client, error) {
	if apiKey == "" {
		return nil, fmt.Errorf("anthropic: ANTHROPIC_API_KEY is required")
	}
	if model == "" {
		return nil, fmt.Errorf("anthropic: model is required")
	}
	return &Client{
		apiKey:  apiKey,
		model:   model,
		baseURL: defaultBaseURL,
		http:    &http.Client{},
	}, nil
}

// Stream issues a streaming Messages API call and returns a channel
// of normalized [provider.Event] values. The channel closes when the
// stream ends (cleanly or otherwise); on a non-2xx HTTP response
// Stream returns a typed *[provider.Error] before any goroutine
// starts.
func (c *Client) Stream(ctx context.Context, req provider.Request) (<-chan provider.Event, error) {
	wireModel, oneMillion := splitOneMillionSuffix(c.model)
	body, err := json.Marshal(buildPayload(wireModel, req))
	if err != nil {
		return nil, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "marshal request"}
	}

	endpoint, err := url.JoinPath(c.baseURL, messagesPath)
	if err != nil {
		return nil, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "build endpoint"}
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, endpoint, bytes.NewReader(body))
	if err != nil {
		return nil, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "build request"}
	}
	httpReq.Header.Set("x-api-key", c.apiKey)
	httpReq.Header.Set("anthropic-version", anthropicVersion)
	httpReq.Header.Set("content-type", "application/json")
	httpReq.Header.Set("accept", "text/event-stream")
	if oneMillion {
		// R-2E6V-LAPQ: gate 1M-context variants behind the
		// documented anthropic-beta header.
		httpReq.Header.Set("anthropic-beta", contextOneMillionBeta)
	}

	// R-92NN-7DNI: log outbound request before sending.
	c.tracer.LogRequest(httpReq.Method, httpReq.URL.String(), httpReq.Header, body)

	resp, err := c.http.Do(httpReq)
	if err != nil {
		return nil, mapTransportError(err)
	}

	if resp.StatusCode != http.StatusOK {
		// R-92NN-7DNI: capture and log error response body.
		errBody, _ := io.ReadAll(resp.Body)
		resp.Body.Close()
		c.tracer.LogResponse(resp.StatusCode, resp.Header, errBody)
		return nil, mapStatus(resp.StatusCode)
	}

	// R-92NN-7DNI: log response headers; SSE pairs are logged in sseParser.
	c.tracer.LogResponse(resp.StatusCode, resp.Header, nil)

	out := make(chan provider.Event)
	go func() {
		defer close(out)
		defer resp.Body.Close()
		(&sseParser{ctx: ctx, out: out, blocks: map[int]*blockState{}, tracer: c.tracer}).run(resp.Body)
	}()
	return out, nil
}

// buildPayload translates a provider.Request into the JSON shape
// Anthropic's Messages API expects.
//
// R-4AH9-0G8M: tool_use and tool_result content blocks are
// passthrough-isomorphic to wire-format. The translation is a
// field-name normalization, not a re-encoding: the JSON input on
// a tool_use block and the content/is_error on a tool_result block
// pass through unchanged.
func buildPayload(model string, req provider.Request) map[string]any {
	// The driver resolves req.MaxTokens from the session config / model
	// registry. A zero value (e.g. an unresolved model) falls back to a
	// conservative cap so the required Anthropic field is always present.
	maxTokens := req.MaxTokens
	if maxTokens <= 0 {
		maxTokens = defaultMaxTokens
	}
	payload := map[string]any{
		"model":      model,
		"stream":     true,
		"max_tokens": maxTokens,
		"messages":   translateMessages(req.Messages),
	}
	if req.SystemPrompt != "" {
		payload["system"] = req.SystemPrompt
	}
	if len(req.Tools) > 0 {
		payload["tools"] = translateTools(req.Tools)
	}
	return payload
}

func translateMessages(msgs []provider.Message) []any {
	out := make([]any, 0, len(msgs))
	for _, m := range msgs {
		out = append(out, map[string]any{
			"role":    string(m.Role),
			"content": translateBlocks(m.Blocks),
		})
	}
	return out
}

func translateBlocks(blocks []provider.Block) []any {
	out := make([]any, 0, len(blocks))
	for _, b := range blocks {
		switch v := b.(type) {
		case provider.TextBlock:
			out = append(out, map[string]any{
				"type": "text",
				"text": v.Text,
			})
		case provider.ThinkingBlock:
			// R-ROBI-V64M: signed thinking must round-trip byte-for-byte.
			out = append(out, map[string]any{
				"type":      "thinking",
				"thinking":  v.Text,
				"signature": v.Signature,
			})
		case provider.ToolUseBlock:
			input := json.RawMessage(v.Input)
			if len(input) == 0 {
				input = json.RawMessage("{}")
			}
			out = append(out, map[string]any{
				"type":  "tool_use",
				"id":    v.ID,
				"name":  v.Name,
				"input": input,
			})
		case provider.ToolResultBlock:
			block := map[string]any{
				"type":        "tool_result",
				"tool_use_id": v.ToolUseID,
				"content":     v.Content,
			}
			if v.IsError {
				block["is_error"] = true
			}
			out = append(out, block)
		}
	}
	return out
}

// splitOneMillionSuffix returns (model-without-[1m], true) when the
// model ID carries the documented 1M-context suffix; otherwise it
// returns the input unchanged with false.
//
// R-2E6V-LAPQ.
func splitOneMillionSuffix(model string) (string, bool) {
	if stripped, ok := strings.CutSuffix(model, contextOneMillionSuffix); ok {
		return stripped, true
	}
	return model, false
}

func translateTools(tools []provider.Tool) []any {
	out := make([]any, 0, len(tools))
	for _, t := range tools {
		schema := json.RawMessage(t.InputSchema)
		if len(schema) == 0 {
			schema = json.RawMessage(`{"type":"object"}`)
		}
		out = append(out, map[string]any{
			"name":         t.Name,
			"input_schema": schema,
		})
	}
	return out
}

// blockState accumulates per-content-block state across SSE deltas
// so the parser can emit one normalized event per completed block.
type blockState struct {
	kind      string
	id        string
	name      string
	thinking  strings.Builder
	signature string
	inputJSON strings.Builder
}

// sseParser turns Anthropic's documented SSE event stream into
// provider.Event values. It owns the channel send so the caller can
// trust send-order matches arrival-order.
type sseParser struct {
	ctx                      context.Context
	out                      chan<- provider.Event
	blocks                   map[int]*blockState
	stopReason               string
	inputTokens              int
	outputTokens             int
	cacheReadInputTokens     int
	cacheCreationInputTokens int
	// R-92NN-7DNI: optional tracer; nil means no SSE-level tracing.
	tracer *trace.Tracer
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
		// R-92NN-7DNI: log SSE pair before processing.
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
	case "message_start":
		var m struct {
			Message struct {
				Usage struct {
					InputTokens              int `json:"input_tokens"`
					OutputTokens             int `json:"output_tokens"`
					CacheReadInputTokens     int `json:"cache_read_input_tokens"`
					CacheCreationInputTokens int `json:"cache_creation_input_tokens"`
				} `json:"usage"`
			} `json:"message"`
		}
		if err := json.Unmarshal([]byte(data), &m); err != nil {
			return
		}
		p.inputTokens = m.Message.Usage.InputTokens
		p.outputTokens = m.Message.Usage.OutputTokens
		p.cacheReadInputTokens = m.Message.Usage.CacheReadInputTokens
		p.cacheCreationInputTokens = m.Message.Usage.CacheCreationInputTokens

	case "content_block_start":
		var s struct {
			Index        int `json:"index"`
			ContentBlock struct {
				Type string `json:"type"`
				ID   string `json:"id"`
				Name string `json:"name"`
			} `json:"content_block"`
		}
		if err := json.Unmarshal([]byte(data), &s); err != nil {
			return
		}
		p.blocks[s.Index] = &blockState{
			kind: s.ContentBlock.Type,
			id:   s.ContentBlock.ID,
			name: s.ContentBlock.Name,
		}

	case "content_block_delta":
		var d struct {
			Index int `json:"index"`
			Delta struct {
				Type        string `json:"type"`
				Text        string `json:"text"`
				Thinking    string `json:"thinking"`
				Signature   string `json:"signature"`
				PartialJSON string `json:"partial_json"`
			} `json:"delta"`
		}
		if err := json.Unmarshal([]byte(data), &d); err != nil {
			return
		}
		b := p.blocks[d.Index]
		if b == nil {
			return
		}
		switch d.Delta.Type {
		case "text_delta":
			p.emit(provider.EventTextDelta{Text: d.Delta.Text})
		case "thinking_delta":
			b.thinking.WriteString(d.Delta.Thinking)
		case "signature_delta":
			b.signature += d.Delta.Signature
		case "input_json_delta":
			b.inputJSON.WriteString(d.Delta.PartialJSON)
		}

	case "content_block_stop":
		var s struct {
			Index int `json:"index"`
		}
		if err := json.Unmarshal([]byte(data), &s); err != nil {
			return
		}
		b := p.blocks[s.Index]
		if b == nil {
			return
		}
		delete(p.blocks, s.Index)
		switch b.kind {
		case "thinking":
			p.emit(provider.EventThinking{Text: b.thinking.String(), Signature: b.signature})
		case "tool_use":
			input := b.inputJSON.String()
			if input == "" {
				input = "{}"
			}
			p.emit(provider.EventToolUse{ID: b.id, Name: b.name, Input: json.RawMessage(input)})
		}

	case "message_delta":
		var d struct {
			Delta struct {
				StopReason string `json:"stop_reason"`
			} `json:"delta"`
			Usage struct {
				InputTokens              int `json:"input_tokens"`
				OutputTokens             int `json:"output_tokens"`
				CacheReadInputTokens     int `json:"cache_read_input_tokens"`
				CacheCreationInputTokens int `json:"cache_creation_input_tokens"`
			} `json:"usage"`
		}
		if err := json.Unmarshal([]byte(data), &d); err != nil {
			return
		}
		if d.Delta.StopReason != "" {
			p.stopReason = d.Delta.StopReason
		}
		if d.Usage.InputTokens != 0 {
			p.inputTokens = d.Usage.InputTokens
		}
		if d.Usage.OutputTokens != 0 {
			p.outputTokens = d.Usage.OutputTokens
		}
		if d.Usage.CacheReadInputTokens != 0 {
			p.cacheReadInputTokens = d.Usage.CacheReadInputTokens
		}
		if d.Usage.CacheCreationInputTokens != 0 {
			p.cacheCreationInputTokens = d.Usage.CacheCreationInputTokens
		}

	case "message_stop":
		// R-1TGL-373X: surface cache-token counters captured from
		// message_start / message_delta usage blobs.
		p.emit(provider.EventUsage{
			InputTokens:              p.inputTokens,
			OutputTokens:             p.outputTokens,
			CacheReadInputTokens:     p.cacheReadInputTokens,
			CacheCreationInputTokens: p.cacheCreationInputTokens,
		})
		sr := p.stopReason
		if sr == "" {
			sr = "end_turn"
		}
		p.emit(provider.EventDone{StopReason: sr})
	}
}

// mapStatus turns an HTTP status code into a typed [provider.Error].
// R-E2W7-K5JB: status codes and response bodies are not echoed into
// Msg.
func mapStatus(code int) *provider.Error {
	switch {
	case code == http.StatusUnauthorized, code == http.StatusForbidden:
		return &provider.Error{Kind: provider.ErrAuth, Msg: "anthropic rejected credentials"}
	case code == http.StatusTooManyRequests:
		return &provider.Error{Kind: provider.ErrRateLimit, Msg: "anthropic rate-limited the request"}
	case code >= 500:
		return &provider.Error{Kind: provider.ErrServer, Msg: "anthropic server error"}
	case code >= 400:
		return &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "anthropic rejected the request"}
	default:
		return &provider.Error{Kind: provider.ErrUnknown, Msg: "anthropic returned unexpected status"}
	}
}

func mapTransportError(err error) *provider.Error {
	if errors.Is(err, context.DeadlineExceeded) {
		return &provider.Error{Kind: provider.ErrTimeout, Msg: "anthropic request deadline exceeded"}
	}
	if errors.Is(err, context.Canceled) {
		return &provider.Error{Kind: provider.ErrUnknown, Msg: "anthropic request canceled"}
	}
	var netErr net.Error
	if errors.As(err, &netErr) && netErr.Timeout() {
		return &provider.Error{Kind: provider.ErrTimeout, Msg: "anthropic request timed out"}
	}
	return &provider.Error{Kind: provider.ErrUnknown, Msg: "anthropic transport failure"}
}
