// Package llm defines the injectable LLM boundary for later wiki phases.
package llm

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"reflect"
	"strings"
	"time"
	"unsafe"

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

// CallRecord is one provider round-trip's footprint.
type CallRecord struct {
	ID        string
	Stage     string
	JobID     string
	Attempt   int
	Provider  string
	Model     string
	Params    string
	Request   string
	Response  string
	Usage     string
	Err       string
	StartedAt time.Time
	EndedAt   time.Time
}

// Recorder persists one call record. A nil Recorder is a no-op.
type Recorder interface {
	Record(ctx context.Context, rec CallRecord) error
}

type jobIDContextKey struct{}

// WithJobID returns a context carrying the current ingest job id.
func WithJobID(ctx context.Context, id string) context.Context {
	if ctx == nil {
		ctx = context.Background()
	}
	return context.WithValue(ctx, jobIDContextKey{}, strings.TrimSpace(id))
}

// JobID reports the current ingest job id, or "" when none is attached.
func JobID(ctx context.Context) string {
	if ctx == nil {
		return ""
	}
	id, _ := ctx.Value(jobIDContextKey{}).(string)
	return id
}

// New records the shared AgentKit provider and optional JSONL log sink.
func New(prov agentkit.Provider, log io.Writer, recorders ...Recorder) *Client {
	c := &Client{prov: prov, log: log}
	if len(recorders) > 0 {
		c.recorder = recorders[0]
	}
	c.setDefaults()
	return c
}

// CallSite is the per-stage generation config.
type CallSite struct {
	Stage           string
	Model           string
	Temperature     *float64
	Reasoning       any
	System          string
	MaxParseRetries int
}

type disabledReasoning struct{}

// DisableReasoning requests explicit reasoning-off generation for a call site.
func DisableReasoning() any {
	return disabledReasoning{}
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
		text, err := sendText(ctx, c, site, conv, attempt, prompt)
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

func sendText(ctx context.Context, c *Client, site CallSite, conv *agentkit.Conversation, attempt int, userText string) (string, error) {
	startedAt := callNow(c)
	stream := conv.Send(ctx, userText)
	var final string
	for ev := range stream.Events() {
		if done, ok := ev.(agentkit.MessageDone); ok {
			final = agentkitText(done.Message)
		}
	}
	err := stream.Err()
	endedAt := callNow(c)
	if recErr := recordCall(ctx, c, site, conv, attempt, userText, final, stream.Usage(), err, startedAt, endedAt); recErr != nil {
		return "", recErr
	}
	if err != nil {
		return "", err
	}
	return final, nil
}

func recordCall(ctx context.Context, c *Client, site CallSite, conv *agentkit.Conversation, attempt int, userText, final string, usage agentkit.Usage, callErr error, startedAt, endedAt time.Time) error {
	if c == nil || c.recorder == nil {
		return nil
	}
	c.setDefaults()
	provider := ""
	if conv != nil && conv.Provider != nil {
		provider = conv.Provider.Name()
	}
	errText := ""
	if callErr != nil {
		errText = callErr.Error()
	}
	rec := CallRecord{
		ID:        c.newID(),
		Stage:     site.Stage,
		JobID:     JobID(ctx),
		Attempt:   attempt,
		Provider:  provider,
		Model:     site.Model,
		Params:    mustJSON(callParamsFor(site)),
		Request:   mustJSON(callRequest{System: site.System, User: userText}),
		Response:  final,
		Usage:     usageJSON(usage),
		Err:       errText,
		StartedAt: startedAt,
		EndedAt:   endedAt,
	}
	return c.recorder.Record(ctx, rec)
}

type callRequest struct {
	System string `json:"system"`
	User   string `json:"user"`
}

type callParams struct {
	Temperature *float64 `json:"temperature,omitempty"`
	Reasoning   any      `json:"reasoning,omitempty"`
}

func callParamsFor(site CallSite) callParams {
	reasoning := site.Reasoning
	if _, ok := reasoning.(disabledReasoning); ok {
		reasoning = "disabled"
	}
	return callParams{Temperature: site.Temperature, Reasoning: reasoning}
}

func usageJSON(usage agentkit.Usage) string {
	if usage == (agentkit.Usage{}) {
		return ""
	}
	return mustJSON(usage)
}

func mustJSON(v any) string {
	raw, err := json.Marshal(v)
	if err != nil {
		return ""
	}
	return string(raw)
}

func callNow(c *Client) time.Time {
	if c == nil {
		return time.Now()
	}
	c.setDefaults()
	return c.now()
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
	if _, ok := reasoning.(disabledReasoning); ok {
		setDisabledReasoning(field)
		return
	}
	value := reflect.ValueOf(reasoning)
	if value.Type().AssignableTo(field.Type()) {
		field.Set(value)
		return
	}
	if value.Type().ConvertibleTo(field.Type()) {
		field.Set(value.Convert(field.Type()))
	}
}

func setDisabledReasoning(field reflect.Value) {
	if !field.IsValid() || !field.CanSet() {
		return
	}

	if isIntKind(field.Kind()) {
		field.SetInt(1)
		return
	}

	if field.Kind() != reflect.Struct {
		return
	}
	tag := field.FieldByName("tag")
	if !tag.IsValid() || !tag.CanAddr() || !isIntKind(tag.Kind()) {
		return
	}
	reflect.NewAt(tag.Type(), unsafe.Pointer(tag.UnsafeAddr())).Elem().SetInt(3)
}

func isIntKind(k reflect.Kind) bool {
	return k >= reflect.Int && k <= reflect.Int64
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
