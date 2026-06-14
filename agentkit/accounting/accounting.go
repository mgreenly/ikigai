// Package accounting emits exactly one structured usage/cost record per
// agentkit API call — every chat Stream completion (so a multi-turn agent
// run emits one record per turn, each turn being one HTTP request) and
// every embeddings Embed call.
//
// P0c: the record lands in the application's structured log as a single JSON
// object. The sink is an injected *slog.Logger (stdlib — agentkit does NOT
// depend on appkit; the service passes its appkit/logging JSON logger in). A
// nil logger is a no-op, exactly like a nil *trace.Tracer. This accounting
// line is distinct from and orthogonal to the trace.Tracer --raw debug
// channel: trace is opt-in debugging, accounting is always-on cost telemetry.
//
// Attribution without coupling: agentkit knows nothing of a service's call
// sites. The caller pre-binds them onto the logger
// (e.g. logger.With("call_site","extract","run_id",id)) and the backend only
// appends the accounting fields below — so the record carries both the
// caller's context and the provider's usage without agentkit importing the
// caller.
package accounting

import (
	"log/slog"
	"time"
)

// Field names of the one accounting record. They are part of the report
// surface Part II's cost/latency rows consume, so they are pinned here.
const (
	FieldProvider        = "provider"
	FieldModel           = "model"
	FieldEffort          = "effort"
	FieldInputTokens     = "input_tokens"
	FieldOutputTokens    = "output_tokens"
	FieldCacheReadTokens = "cache_read_tokens"
	FieldCostUSD         = "cost_usd"
	FieldDurationMS      = "duration_ms"
	FieldStopReason      = "stop_reason"

	// msg is the constant message of every accounting record; the
	// structured fields carry the data.
	msg = "api_call"
)

// Record carries the usage/cost fields of one API call. The caller's
// call-site attribution is pre-bound on the logger, not set here.
//
// StopReason is empty for embeddings (there is no generation stop reason);
// CacheReadTokens is zero where the provider reports no cache-read tier.
type Record struct {
	Provider        string
	Model           string
	Effort          string
	InputTokens     int
	OutputTokens    int
	CacheReadTokens int
	CostUSD         float64
	DurationMS      int64
	StopReason      string
}

// Log emits exactly one slog record carrying r's fields onto logger. A nil
// logger is a no-op (the always-on accounting line costs nothing when the
// service injects no logger). StopReason is omitted when empty so embed
// records don't carry a meaningless field.
func Log(logger *slog.Logger, r Record) {
	if logger == nil {
		return
	}
	attrs := []any{
		slog.String(FieldProvider, r.Provider),
		slog.String(FieldModel, r.Model),
		slog.String(FieldEffort, r.Effort),
		slog.Int(FieldInputTokens, r.InputTokens),
		slog.Int(FieldOutputTokens, r.OutputTokens),
		slog.Int(FieldCacheReadTokens, r.CacheReadTokens),
		slog.Float64(FieldCostUSD, r.CostUSD),
		slog.Int64(FieldDurationMS, r.DurationMS),
	}
	if r.StopReason != "" {
		attrs = append(attrs, slog.String(FieldStopReason, r.StopReason))
	}
	logger.Info(msg, attrs...)
}

// DurationSince is a small helper that converts a start time into the
// integer-millisecond duration the record carries.
func DurationSince(start time.Time) int64 {
	return time.Since(start).Milliseconds()
}
