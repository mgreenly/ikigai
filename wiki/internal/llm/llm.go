// Package llm defines the injectable LLM boundary for later wiki phases.
package llm

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"reflect"
	"strings"

	agentkit "github.com/ikigenba/agentkit"
)

// Message is one provider-neutral chat message.
type Message struct {
	Role    string
	Content string
}

// Provider is the narrow seam domain packages consume.
type Provider interface {
	Complete(ctx context.Context, messages []Message) (string, error)
}

// New records the shared AgentKit provider and optional JSONL log sink.
func New(prov agentkit.Provider, log io.Writer) *Client {
	return &Client{prov: prov, log: log}
}

// CallSite is the per-stage generation config.
type CallSite struct {
	Model           string
	Temperature     *float64
	Reasoning       any
	System          string
	MaxParseRetries int
}

// JSON runs a tool-less structured generation and validates the decoded value.
func JSON[T any](ctx context.Context, c *Client, site CallSite, userText string, validate func(*T) error) (T, error) {
	var zero T
	if c == nil || c.prov == nil {
		return zero, fmt.Errorf("llm JSON: nil client provider")
	}

	attempts := site.MaxParseRetries + 1
	if attempts < 1 {
		attempts = 1
	}

	conv := c.Converse(site, nil)
	prompt := userText
	var lastErr error
	for attempt := 1; attempt <= attempts; attempt++ {
		text, err := sendText(ctx, conv, prompt)
		if err != nil {
			return zero, err
		}

		var out T
		if err := json.Unmarshal([]byte(ExtractJSON(text)), &out); err != nil {
			lastErr = err
		} else if validate != nil {
			lastErr = validate(&out)
		} else {
			lastErr = nil
		}
		if lastErr == nil {
			return out, nil
		}
		if attempt < attempts {
			prompt = correctivePrompt(userText, lastErr)
		}
	}

	return zero, fmt.Errorf("llm JSON: parse or validation failed after %d attempt(s): %w", attempts, lastErr)
}

// Converse builds a fresh AgentKit conversation for a stage.
func (c *Client) Converse(site CallSite, tools []agentkit.Tool) *agentkit.Conversation {
	if c == nil {
		return &agentkit.Conversation{}
	}
	gen := agentkit.GenSettings{
		Temperature: site.Temperature,
	}
	setReasoning(&gen, site.Reasoning)
	return &agentkit.Conversation{
		Provider: c.prov,
		Model:    site.Model,
		System:   site.System,
		Log:      c.log,
		Gen:      gen,
		Tools:    append([]agentkit.Tool(nil), tools...),
	}
}

func sendText(ctx context.Context, conv *agentkit.Conversation, userText string) (string, error) {
	stream := conv.Send(ctx, userText)
	var final string
	for ev := range stream.Events() {
		if done, ok := ev.(agentkit.MessageDone); ok {
			final = agentkitText(done.Message)
		}
	}
	if err := stream.Err(); err != nil {
		return "", err
	}
	return final, nil
}

func agentkitText(message agentkit.Message) string {
	var b strings.Builder
	for _, block := range message.Blocks {
		if text, ok := block.(agentkit.TextBlock); ok {
			b.WriteString(text.Text)
		}
	}
	return b.String()
}

func setReasoning(gen *agentkit.GenSettings, reasoning any) {
	if gen == nil || reasoning == nil {
		return
	}

	field := reflect.ValueOf(gen).Elem().FieldByName("Reasoning")
	value := reflect.ValueOf(reasoning)
	if value.Type().AssignableTo(field.Type()) {
		field.Set(value)
		return
	}
	if value.Type().ConvertibleTo(field.Type()) {
		field.Set(value.Convert(field.Type()))
	}
}

// ExtractJSON carves the first JSON object or array from a decorated reply.
func ExtractJSON(text string) string {
	s := strings.TrimSpace(text)
	firstObject := strings.IndexByte(s, '{')
	firstArray := strings.IndexByte(s, '[')

	start := firstObject
	close := byte('}')
	if firstArray >= 0 && (start < 0 || firstArray < start) {
		start = firstArray
		close = ']'
	}
	if start < 0 {
		return s
	}

	end := strings.LastIndexByte(s, close)
	if end < start {
		return s
	}
	return strings.TrimSpace(s[start : end+1])
}

func correctivePrompt(original string, err error) string {
	return original + "\n\nThe previous response could not be parsed and validated as the requested JSON: " +
		err.Error() + "\nReturn only valid JSON for the original request."
}
