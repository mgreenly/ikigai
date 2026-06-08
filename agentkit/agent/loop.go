// Package agent drives a single ikigai-cli iteration: it issues one
// streaming request to a provider.Client, forwards the model's turn to
// the wire layer, and terminates the iteration with a single result
// event.
//
// R-VJBZ-S578: an iteration terminates with exactly one `result` event
// whose `structured_output` satisfies the JSON schema supplied via
// `--json-schema`.
package agent

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"agentkit/model"
	"agentkit/provider"
	"agentkit/schema"
	"agentkit/tools"
	"agentkit/trace"
	"agentkit/wire"
)

// maxStructuredAttempts caps how many times Run will issue the model
// when each attempt's structured_output fails to parse or fails schema
// validation. R-WFWM-BKWX: when the model does not produce
// schema-conforming output, the agent retries up to a bounded number of
// times before surfacing an iteration error.
const maxStructuredAttempts = 3

// Options carries the optional, behavior-shaping parameters for Run.
// The zero value reproduces the historical defaults: freeform terminal
// mode (no schema), an unconfined sandbox, no tracing, and built-in
// tools only.
type Options struct {
	// Schema constrains the terminal result's structured_output. A nil
	// Schema selects freeform terminal mode (the assistant's raw final
	// text becomes the result), which is the unchanged default.
	Schema *schema.Schema
	// SandboxRoot confines file-touching tool dispatch to a directory
	// subtree. An empty string leaves dispatch unconfined.
	SandboxRoot string
	// Tracer receives trace events for every tool dispatch. A nil
	// Tracer disables tracing.
	Tracer *trace.Tracer
	// Tools supplies a caller-provided source of additional tools. A
	// nil Tools restricts the run to the built-in tool set, preserving
	// historical behavior byte-for-byte. When non-nil, Run advertises the
	// source's descriptors alongside the built-ins and routes tool_use
	// blocks the source Owns through its Dispatch.
	Tools ToolSource
}

// ToolSource is a caller-provided source of additional tools the agent
// may advertise and dispatch. When supplied via Options.Tools, Run
// appends its Descriptors to the provider request and routes owned
// tool_use blocks through Dispatch.
type ToolSource interface {
	// Descriptors returns the provider-neutral advertisements for every
	// tool this source owns.
	Descriptors() []provider.Tool
	// Owns reports whether the named tool is dispatched by this source.
	Owns(name string) bool
	// Dispatch executes the named tool with the given input and returns
	// the resulting tool_result block.
	Dispatch(ctx context.Context, name string, input json.RawMessage) (wire.ToolResultBlock, error)
}

// Run executes one iteration. It calls client.Stream, emits an
// assistant event for the turn, and on a non-tool stop reason emits a
// result event whose structured_output is parsed from the assistant's
// final text and validated against sch.
//
// R-8293-8LCI: when the assistant stops with "tool_use", Run dispatches
// every tool_use block through internal/tools.Dispatch, emits a user
// event with the results, appends both turns to req.Messages, and
// re-invokes the provider. The tool-dispatch cycle repeats until the
// model returns a non-tool stop reason.
//
// R-WFWM-BKWX: delivering schema-conforming structured_output is
// ikigai-cli's responsibility. If the assistant's text fails to parse
// as JSON or fails to validate against sch, Run retries the model up
// to maxStructuredAttempts times before emitting an iteration error.
// Each attempt emits its own assistant event so the operator's
// transcript records the failed turns.
//
// The session must be fresh (no prior events). On any error — provider
// failure, malformed structured_output exhausting all attempts, or
// schema-validation failure exhausting all attempts — Run still emits
// exactly one result event, with is_error=true and a transport-free
// message in structured_output.
// Run executes one iteration. tracer may be nil; when non-nil it
// receives trace events for every tool dispatch.
//
// R-Y5QZ-UNB2: Run tracks turn count, wall-clock duration, and
// accumulated token usage; the terminal result event carries these
// values in num_turns, duration_ms, total_cost_usd, usage, and
// modelUsage.
func Run(ctx context.Context, client provider.Client, sess *wire.Session, req provider.Request, opts Options) error {
	sch := opts.Schema
	sandboxRoot := opts.SandboxRoot
	tracer := opts.Tracer
	startTime := time.Now()
	var lastErr error
	attempt := 0
	numTurns := 0
	var cumUsage provider.EventUsage

	// Resolve model for pricing/capacity lookup; ignore error so tests
	// with empty req.Model continue to work (zero pricing is the fallback).
	resolved, _ := model.Resolve(req.Model)
	pricing := model.ModelPricing(resolved)
	caps := model.ModelContext(resolved)

	// Advertise the caller-supplied tool source, if any, BEFORE the first
	// provider stream. Built-ins already present in req.Tools stay; the
	// source's descriptors are appended (built-ins plus suite tools). A nil
	// opts.Tools leaves req.Tools untouched, preserving historical behavior.
	if opts.Tools != nil {
		req.Tools = append(req.Tools, opts.Tools.Descriptors()...)
	}

	for {
		events, err := client.Stream(ctx, req)
		if err != nil {
			return emitIterationError(sess, err.Error())
		}

		wireBlocks, providerBlocks, text, stop, usage := drainTurn(events)
		numTurns++
		cumUsage.InputTokens += usage.InputTokens
		cumUsage.OutputTokens += usage.OutputTokens
		cumUsage.CacheReadInputTokens += usage.CacheReadInputTokens
		cumUsage.CacheCreationInputTokens += usage.CacheCreationInputTokens

		if err := sess.EmitAssistant(wire.NewAssistantEvent(wireBlocks...)); err != nil {
			return err
		}

		if stop == "tool_use" {
			// R-8293-8LCI: dispatch tools and loop back.
			req, err = dispatchTools(ctx, sess, req, wireBlocks, providerBlocks, sandboxRoot, tracer, opts.Tools)
			if err != nil {
				return err
			}
			continue
		}

		// buildStats assembles the iteration accounting block shared by
		// the freeform and structured result paths (R-Y5QZ-UNB2).
		buildStats := func() wire.IterationStats {
			costUSD := pricing.ComputeCost(
				cumUsage.InputTokens,
				cumUsage.OutputTokens,
				cumUsage.CacheReadInputTokens,
				cumUsage.CacheCreationInputTokens,
			)
			stats := wire.IterationStats{
				NumTurns:   numTurns,
				DurationMs: time.Since(startTime).Milliseconds(),
				Usage: wire.UsageTotals{
					InputTokens:              cumUsage.InputTokens,
					OutputTokens:             cumUsage.OutputTokens,
					CacheReadInputTokens:     cumUsage.CacheReadInputTokens,
					CacheCreationInputTokens: cumUsage.CacheCreationInputTokens,
				},
			}
			if req.Model != "" {
				stats.ModelUsage = map[string]wire.ModelUsageEntry{
					req.Model: {
						InputTokens:              cumUsage.InputTokens,
						OutputTokens:             cumUsage.OutputTokens,
						CacheReadInputTokens:     cumUsage.CacheReadInputTokens,
						CacheCreationInputTokens: cumUsage.CacheCreationInputTokens,
						CostUSD:                  costUSD,
						ContextWindow:            caps.ContextWindow,
						MaxOutputTokens:          caps.MaxOutputTokens,
					},
				}
			}
			return stats
		}

		// Freeform mode (sch == nil): the assistant's raw final text is
		// the run result. No JSON parse, no validation, no retry; empty
		// text is allowed.
		if sch == nil {
			ev, err := wire.NewResultEventFull(text, false, buildStats())
			if err != nil {
				return emitIterationError(sess, fmt.Sprintf("result marshal: %v", err))
			}
			return sess.EmitResult(ev)
		}

		// Structured mode: parse + validate against sch, retrying up to
		// maxStructuredAttempts. R-WFWM-BKWX.
		attempt++
		value, perr := parseAndValidate(text, sch)
		if perr == nil {
			ev, err := wire.NewResultEventFull(value, false, buildStats())
			if err != nil {
				return emitIterationError(sess, fmt.Sprintf("structured_output marshal: %v", err))
			}
			return sess.EmitResult(ev)
		}
		lastErr = perr

		if attempt >= maxStructuredAttempts {
			return emitIterationError(sess, fmt.Sprintf("structured_output failed after %d attempts: %v", maxStructuredAttempts, lastErr))
		}
	}
}

// dispatchTools dispatches every tool_use block in wireBlocks, emits one
// user event per tool_result via sess (R-EW6N-L2M1), and returns a new
// Request with the assistant turn and tool-result user turn appended to
// history. R-8293-8LCI.
func dispatchTools(ctx context.Context, sess *wire.Session, req provider.Request, wireBlocks []any, providerBlocks []provider.Block, sandboxRoot string, tracer *trace.Tracer, toolSrc ToolSource) (provider.Request, error) {
	var resultProviderBlocks []provider.Block

	for _, b := range wireBlocks {
		tu, ok := b.(wire.ToolUseBlock)
		if !ok {
			continue
		}
		// R-6EFF-GW25: trace tool dispatch before execution.
		tracer.LogToolDispatch(tu.Name, tu.Input)

		var trWire wire.ToolResultBlock
		var sidecar any
		var err error
		if toolSrc != nil && toolSrc.Owns(tu.Name) {
			// Source-dispatched tool. The ToolSource.Dispatch contract does
			// not receive the tool_use id, so the returned block lacks it;
			// attach it from tu to mirror the built-in path. A source result
			// has no sidecar. A non-nil Go error must NOT crash the run
			// (decision #9): convert it into an is_error tool_result so the
			// loop continues.
			trWire, err = toolSrc.Dispatch(ctx, tu.Name, tu.Input)
			if err != nil {
				trWire, err = wire.NewToolResultBlock(tu.ID, true, err.Error())
				if err != nil {
					return provider.Request{}, fmt.Errorf("dispatch %s: %w", tu.Name, err)
				}
			} else {
				trWire.ToolUseID = tu.ID
			}
			sidecar = nil
		} else {
			trWire, sidecar, err = tools.Dispatch(ctx, sandboxRoot, tu)
			if err != nil {
				// Dispatch returns an is_error block on failure; a Go error here
				// means NewToolResultBlock itself failed, which should never happen.
				return provider.Request{}, fmt.Errorf("dispatch %s: %w", tu.Name, err)
			}
		}

		// Unmarshal the wire content (JSON-encoded string) to obtain the
		// plain string that provider.ToolResultBlock.Content expects.
		var contentStr string
		if err := json.Unmarshal(trWire.Content, &contentStr); err != nil {
			// Fallback: use the raw JSON as the content string.
			contentStr = string(trWire.Content)
		}
		// R-6EFF-GW25: trace tool result after execution.
		tracer.LogToolResult(tu.Name, trWire.IsError, contentStr)

		// R-DPI6-73NQ / R-EW6N-L2M1: emit one user event per tool_result,
		// attaching the tool-specific sidecar when the tool provides one.
		var userEv wire.UserEvent
		if sidecar != nil {
			userEv = wire.NewUserEventWithSidecar(sidecar, trWire)
		} else {
			userEv = wire.NewUserEvent(trWire)
		}
		if err := sess.EmitUser(userEv); err != nil {
			return provider.Request{}, err
		}

		resultProviderBlocks = append(resultProviderBlocks, provider.ToolResultBlock{
			ToolUseID: trWire.ToolUseID,
			Content:   contentStr,
			IsError:   trWire.IsError,
		})
	}

	// Clone the message history and append the assistant turn (preserving
	// thinking blocks per R-ROBI-V64M) and the tool-result user turn.
	newMsgs := make([]provider.Message, len(req.Messages), len(req.Messages)+2)
	copy(newMsgs, req.Messages)
	newMsgs = append(newMsgs,
		provider.Message{Role: provider.RoleAssistant, Blocks: provider.CloneBlocks(providerBlocks)},
		provider.Message{Role: provider.RoleUser, Blocks: resultProviderBlocks},
	)
	newReq := req
	newReq.Messages = newMsgs
	return newReq, nil
}

// drainTurn reads the provider event channel until close and returns
// (1) the wire blocks for stream emission, (2) the provider blocks for
// history append (preserving signatures per R-ROBI-V64M), (3) the
// concatenated assistant text, (4) the observed stop reason, and
// (5) the last EventUsage seen (used by Run for iteration accounting).
func drainTurn(events <-chan provider.Event) (wireBlocks []any, providerBlocks []provider.Block, text string, stop string, usage provider.EventUsage) {
	var textBuf strings.Builder
	var allText strings.Builder
	flushText := func() {
		if textBuf.Len() == 0 {
			return
		}
		t := textBuf.String()
		wireBlocks = append(wireBlocks, wire.NewTextBlock(t))
		providerBlocks = append(providerBlocks, provider.TextBlock{Text: t})
		textBuf.Reset()
	}
	for ev := range events {
		switch e := ev.(type) {
		case provider.EventTextDelta:
			textBuf.WriteString(e.Text)
			allText.WriteString(e.Text)
		case provider.EventThinking:
			flushText()
			// R-FPG8-RKEP: only forward thinking blocks with non-empty text to
			// stdout. Signature-only events (empty Text) are history mechanics,
			// not human-readable output; they stay in providerBlocks for
			// round-trip but must not appear on the wire.
			if e.Text != "" {
				wireBlocks = append(wireBlocks, wire.NewThinkingBlock(e.Text))
			}
			providerBlocks = append(providerBlocks, provider.ThinkingBlock{Text: e.Text, Signature: e.Signature})
		case provider.EventToolUse:
			flushText()
			wireBlocks = append(wireBlocks, wire.ToolUseBlock{
				Type:  "tool_use",
				ID:    e.ID,
				Name:  e.Name,
				Input: e.Input,
			})
			providerBlocks = append(providerBlocks, provider.ToolUseBlock{
				ID:    e.ID,
				Name:  e.Name,
				Input: e.Input,
			})
		case provider.EventDone:
			stop = e.StopReason
		case provider.EventUsage:
			// R-Y5QZ-UNB2: capture usage for iteration accounting.
			usage = e
		}
	}
	flushText()
	text = allText.String()
	return
}

// parseAndValidate parses text as JSON and validates the result
// against sch. It returns the decoded value or a transport-free error
// describing why the text is not acceptable structured_output.
func parseAndValidate(text string, sch *schema.Schema) (any, error) {
	trimmed := strings.TrimSpace(text)
	if trimmed == "" {
		return nil, fmt.Errorf("empty assistant text; no structured_output to extract")
	}
	var value any
	if err := json.Unmarshal([]byte(trimmed), &value); err != nil {
		return nil, fmt.Errorf("parse: %v", err)
	}
	if sch != nil {
		if err := sch.Validate(value); err != nil {
			return nil, fmt.Errorf("schema: %v", err)
		}
	}
	return value, nil
}

// emitIterationError writes a result event carrying msg as the
// structured_output with is_error=true. R-E2W7-K5JB: msg is expected
// to already be transport-free.
func emitIterationError(sess *wire.Session, msg string) error {
	ev, err := wire.NewResultEvent(map[string]string{"error": msg}, true)
	if err != nil {
		return err
	}
	return sess.EmitResult(ev)
}
