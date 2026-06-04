// Package provider defines the provider-neutral abstraction the agent
// loop uses to talk to a backing model. Concrete backends (Anthropic
// today, OpenAI / Google later) implement [Client] in their own
// subpackages. The agent loop and the wire-format codec MUST NOT
// import any provider-specific subpackage.
//
// R-G0EH-D2SW: the provider abstraction layer's interface is the set
// of operations needed by the agent loop:
//   - issue a streaming generation request given (model, effort,
//     messages, tools, response-schema)
//   - stream back normalized events (assistant text deltas,
//     tool_use blocks, thinking blocks where applicable, usage
//     totals, completion signal)
//   - report errors as typed values, not raw HTTP errors
//
// R-S04B-QD3D: the abstraction is shaped so adding OpenAI / Google
// later does not require re-architecting the loop or the codec, but
// no concrete non-Anthropic backend lives in v1.
package provider

import (
	"context"
	"encoding/json"
	"strings"
)

// Role identifies the speaker of a [Message].
type Role string

const (
	RoleUser      Role = "user"
	RoleAssistant Role = "assistant"
)

// Block is one unit of a multi-part [Message]. The concrete types
// are [TextBlock], [ToolUseBlock], [ToolResultBlock], and
// [ThinkingBlock]. The set is closed; backends translate to and
// from their wire representations.
type Block interface {
	isBlock()
}

// TextBlock is a plain-text segment of a message.
type TextBlock struct {
	Text string
}

func (TextBlock) isBlock() {}

// ToolUseBlock is the model's request to invoke a tool. ID is the
// provider-issued correlation handle; the matching [ToolResultBlock]
// must echo it back.
type ToolUseBlock struct {
	ID    string
	Name  string
	Input json.RawMessage
}

func (ToolUseBlock) isBlock() {}

// ToolResultBlock is the agent loop's reply to a [ToolUseBlock].
// IsError signals that the tool itself failed; Content is the
// human-readable result string in either case.
type ToolResultBlock struct {
	ToolUseID string
	Content   string
	IsError   bool
}

func (ToolResultBlock) isBlock() {}

// ThinkingBlock carries a provider's reasoning trace plus any
// signature the provider requires to be echoed back on subsequent
// requests (Anthropic signed thinking, OpenAI encrypted reasoning,
// Gemini thoughtSignature). R-ROBI-V64M: signed thinking paired with
// tool_use MUST round-trip unchanged or the API rejects.
type ThinkingBlock struct {
	Text      string
	Signature string
}

func (ThinkingBlock) isBlock() {}

// Message is one turn of the conversation transcript fed to the
// model. Blocks preserve their order on the wire.
type Message struct {
	Role   Role
	Blocks []Block
}

// Tool is the provider-neutral advertisement of a tool the model may
// call. Backends translate this to their own tool-spec shape. The
// driver populates the slice from internal/tools.All() per
// R-B9P4-41S7.
type Tool struct {
	Name        string
	InputSchema json.RawMessage
}

// Request is one streaming generation call. ResponseSchema is nil
// when the caller has no --json-schema constraint. SystemPrompt is
// set to the framing prompt on every provider call per R-8PF6-I8FP.
//
// MaxTokens is the upper bound on output tokens for the call. The
// driver resolves it from the session config (an explicit max_tokens),
// falling back to the model's registry-pinned maximum output tokens
// when unset, so the default is "as many as the model allows" rather
// than a fixed low cap. A zero value lets the backend apply its own
// conservative fallback.
type Request struct {
	Model          string
	Effort         string
	SystemPrompt   string
	Messages       []Message
	Tools          []Tool
	ResponseSchema json.RawMessage
	MaxTokens      int
}

// Event is one normalized item in a streaming response. The
// concrete types are [EventTextDelta], [EventToolUse],
// [EventThinking], [EventUsage], and [EventDone]. Backends emit
// them in arrival order; [EventDone] is always the last event of a
// successful stream.
type Event interface {
	isEvent()
}

// EventTextDelta is an incremental chunk of assistant text.
type EventTextDelta struct {
	Text string
}

func (EventTextDelta) isEvent() {}

// EventToolUse is a fully-assembled tool_use block from the model.
// Backends buffer streaming input fragments and emit one event per
// completed block.
type EventToolUse struct {
	ID    string
	Name  string
	Input json.RawMessage
}

func (EventToolUse) isEvent() {}

// EventThinking is a fully-assembled thinking block. Backends that
// require signature round-tripping populate Signature.
type EventThinking struct {
	Text      string
	Signature string
}

func (EventThinking) isEvent() {}

// EventUsage reports cumulative token counts for the request. A
// backend may emit it once at the end or update it during the
// stream; the agent loop treats the last value as authoritative.
//
// R-1TGL-373X: CacheReadInputTokens and CacheCreationInputTokens
// carry Anthropic's prompt-caching counters. Backends that don't
// report them leave them at zero.
type EventUsage struct {
	InputTokens              int
	OutputTokens             int
	CacheReadInputTokens     int
	CacheCreationInputTokens int
}

func (EventUsage) isEvent() {}

// EventDone is the terminal event of a successful stream. StopReason
// mirrors the provider's reported reason, normalized to one of
// "end_turn", "tool_use", "max_tokens", or "stop_sequence".
type EventDone struct {
	StopReason string
}

func (EventDone) isEvent() {}

// ErrorKind classifies a provider failure without leaking transport
// details. R-E2W7-K5JB: HTTP status codes and response bodies do not
// reach stdout; the agent loop turns an Error into a result event
// with is_error=true.
type ErrorKind int

const (
	// ErrUnknown is the fallback when no more specific kind applies.
	ErrUnknown ErrorKind = iota
	// ErrAuth is a 401/403 or equivalent — invalid or missing key.
	ErrAuth
	// ErrInvalidRequest is a 4xx attributable to the request shape.
	ErrInvalidRequest
	// ErrRateLimit is a 429 or provider-specific quota signal.
	ErrRateLimit
	// ErrTimeout is a connection or read timeout.
	ErrTimeout
	// ErrServer is a 5xx or otherwise transient backend failure.
	ErrServer
)

// Error is the typed error every [Client] returns. Implementations
// MUST NOT return raw *url.Error, *http.Response, or stringified
// HTTP bodies.
type Error struct {
	Kind ErrorKind
	Msg  string
}

func (e *Error) Error() string {
	return e.Msg
}

// kindLabel is the stable short name used as the prefix of the
// iteration-error message produced by [ErrorMessage]. The set of
// labels is part of the externally-visible failure surface; do not
// rename without updating R-E2W7-K5JB callers.
func (k ErrorKind) kindLabel() string {
	switch k {
	case ErrAuth:
		return "auth"
	case ErrInvalidRequest:
		return "invalid_request"
	case ErrRateLimit:
		return "rate_limit"
	case ErrTimeout:
		return "timeout"
	case ErrServer:
		return "server"
	default:
		return "unknown"
	}
}

// ErrorMessage produces the single-line, transport-free message that
// the agent loop places in the iteration-error result event when a
// provider call fails. The kind is encoded as a stable prefix; the
// underlying Msg is collapsed to a single line so newlines from a
// stringified HTTP body cannot fan out across stdout.
//
// R-E2W7-K5JB: raw HTTP status codes and response bodies must not
// leak onto stdout — backends are responsible for stripping those
// from Msg before constructing the [Error]; this helper only
// guarantees the line-shape contract.
func ErrorMessage(e *Error) string {
	if e == nil {
		return ""
	}
	prefix := e.Kind.kindLabel()
	msg := strings.Join(strings.Fields(e.Msg), " ")
	if msg == "" {
		return prefix
	}
	return prefix + ": " + msg
}

// CloneBlocks returns a deep copy of src that preserves block order,
// kind, and all field values. R-ROBI-V64M: when an iteration uses
// tools, the agent loop appends the assistant's prior turn —
// including any [ThinkingBlock] paired with a [ToolUseBlock] — to
// the next [Request]'s Messages. Cloning ensures that downstream
// mutation of either copy cannot corrupt a thinking block's
// Signature or a tool_use block's Input, both of which the provider
// requires to round-trip byte-for-byte.
func CloneBlocks(src []Block) []Block {
	if src == nil {
		return nil
	}
	out := make([]Block, len(src))
	for i, b := range src {
		switch v := b.(type) {
		case TextBlock:
			out[i] = TextBlock{Text: v.Text}
		case ThinkingBlock:
			out[i] = ThinkingBlock{Text: v.Text, Signature: v.Signature}
		case ToolUseBlock:
			var input json.RawMessage
			if v.Input != nil {
				input = append(json.RawMessage(nil), v.Input...)
			}
			out[i] = ToolUseBlock{ID: v.ID, Name: v.Name, Input: input}
		case ToolResultBlock:
			out[i] = ToolResultBlock{ToolUseID: v.ToolUseID, Content: v.Content, IsError: v.IsError}
		default:
			out[i] = b
		}
	}
	return out
}

// Client is the contract every provider backend implements. Stream
// returns a channel that the caller reads until it is closed; the
// last event delivered on a successful call is an [EventDone]. On
// failure the channel closes after delivering whatever events were
// already produced and Stream's returned error is a *[Error].
type Client interface {
	Stream(ctx context.Context, req Request) (<-chan Event, error)
}
