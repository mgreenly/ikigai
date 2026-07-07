package mcp

import (
	"context"
	"encoding/json"
	"errors"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"scripts/internal/script"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// `ikigenba` plus the service name, defined once and used in BOTH the descriptor
// list and the dispatch switch so the two sites cannot drift.
const toolPrefix = ""

// tool returns the branded MCP tool name for a verb.
func tool(verb string) string { return toolPrefix + verb }

type toolHandlers struct {
	svc *script.Service
}

// Tools returns scripts' service-owned MCP tool declarations. The shared appkit
// MCP transport appends the chassis health and reflection tools.
func Tools(svc *script.Service) []appkitmcp.Tool {
	if svc == nil {
		panic("mcp: script service is required")
	}
	h := &toolHandlers{svc: svc}
	return []appkitmcp.Tool{
		desc(tool("describe"), "Return a detailed overview of scripts: what a script is, the create→run→poll→read lifecycle, triggers, and the runtime contract. Call this first if you're unfamiliar with scripts. Takes no inputs.", obj(map[string]any{}), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("describe"), id, args)
		}),

		desc(tool("create"), "Create a new script for the caller. body is the Python source. Returns the new script_id.", obj(map[string]any{
			"name":   typ("string"),
			"body":   typ("string"),
			"config": configSchema(),
		}, "name", "body"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("create"), id, args)
		}),

		desc(tool("import"), "Import a Dropbox-mirrored file as a script. 'source_path' is the file's path in the dropbox mirror (e.g. \"/scripts/nightly.py\"). Fetches the current mirror bytes over loopback, requires valid UTF-8 text under 1 MiB, and upserts on source_path: re-importing the same path updates the same script instead of creating a duplicate. 'name' defaults to the file's basename. Returns {script_id, name}.", obj(map[string]any{
			"source_path": typ("string"),
			"name":        typ("string"),
		}, "source_path"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("import"), id, args)
		}),

		desc(tool("list"), "List the caller's scripts, each with its derived running_count and last_run.", obj(map[string]any{}), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("list"), id, args)
		}),

		desc(tool("get"), "Get one of the caller's scripts, including running_count and last_run.", obj(map[string]any{
			"script_id": typ("string"),
		}, "script_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("get"), id, args)
		}),

		desc(tool("update"), "Update a script's name, body, and/or config. Any field may be omitted to leave it unchanged.", obj(map[string]any{
			"script_id": typ("string"),
			"name":      typ("string"),
			"body":      typ("string"),
			"config":    configSchema(),
		}, "script_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("update"), id, args)
		}),

		desc(tool("delete"), "Delete one of the caller's scripts (tombstone): the script row and its triggers are removed, but its run history and on-disk artifacts survive.", obj(map[string]any{
			"script_id": typ("string"),
		}, "script_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("delete"), id, args)
		}),

		desc(tool("set_trigger"), "Bind a script to an upstream event. source is one of cron|crm|ledger|dropbox|prompts; event_filter is a glob over that producer's event types (e.g. \"contact.created\", \"contact.*\", \"cron.nightly\"). When a matching event fires, scripts starts a run.", obj(map[string]any{
			"script_id":    typ("string"),
			"source":       typ("string"),
			"event_filter": typ("string"),
		}, "script_id", "source", "event_filter"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("set_trigger"), id, args)
		}),

		desc(tool("clear_trigger"), "Remove an event trigger from a script.", obj(map[string]any{
			"script_id":    typ("string"),
			"source":       typ("string"),
			"event_filter": typ("string"),
		}, "script_id", "source", "event_filter"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("clear_trigger"), id, args)
		}),

		desc(tool("run"), "Start a manual run of one of the caller's scripts. Always allowed — runs are fully concurrent. Returns the run_id and start time.", obj(map[string]any{
			"script_id": typ("string"),
		}, "script_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("run"), id, args)
		}),

		desc(tool("run_list"), "List runs, optionally filtered by script_id and/or status (running|succeeded|failed|cancelled). Each carries elapsed_secs.", obj(map[string]any{
			"script_id": typ("string"),
			"status":    typ("string"),
		}), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("run_list"), id, args)
		}),

		desc(tool("run_get"), "Get one run by run_id, including status, exit_code, and elapsed_secs.", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("run_get"), id, args)
		}),

		desc(tool("run_output"), "Read a run's captured output. stream is stdout|stderr|both (default both). offset is 1-based; limit caps lines (<=0 means from start / no limit).", obj(map[string]any{
			"run_id": typ("string"),
			"stream": typ("string"),
			"offset": typ("integer"),
			"limit":  typ("integer"),
		}, "run_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("run_output"), id, args)
		}),

		desc(tool("run_cancel"), "Cancel an in-flight run by run_id (kills the process group). Idempotent.", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("run_cancel"), id, args)
		}),

		desc(tool("run_fs_list"), "List entries under path within a run's persisted dir tree (path defaults to the run root).", obj(map[string]any{
			"run_id": typ("string"),
			"path":   typ("string"),
		}, "run_id"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("run_fs_list"), id, args)
		}),

		desc(tool("run_fs_read"), "Read a file within a run's persisted dir. offset is 1-based; limit caps lines (<=0 means from start / no limit).", obj(map[string]any{
			"run_id": typ("string"),
			"path":   typ("string"),
			"offset": typ("integer"),
			"limit":  typ("integer"),
		}, "run_id", "path"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.dispatchTool(ctx, tool("run_fs_read"), id, args)
		}),
	}
}

func desc(name, description string, schema map[string]any, handler func(context.Context, json.RawMessage, server.Identity) (map[string]any, error)) appkitmcp.Tool {
	return appkitmcp.Tool{Name: name, Description: description, InputSchema: schema, Handler: handler}
}

func obj(props map[string]any, required ...string) map[string]any {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	return o
}

func typ(t string) map[string]any { return map[string]any{"type": t} }

// configSchema is the shared script.Config input schema (minimal day-one).
func configSchema() map[string]any {
	return obj(map[string]any{
		"interpreter":  typ("string"),
		"timeout_secs": typ("integer"),
	})
}

// paramError marks genuinely unparseable tool arguments — mapped to JSON-RPC
// -32602 rather than an MCP isError tool result.
type paramError struct{ err error }

func (e *paramError) Error() string { return "invalid params: " + e.err.Error() }

func parseArgs(args json.RawMessage, v any) error {
	if len(args) == 0 {
		return nil
	}
	if err := json.Unmarshal(args, v); err != nil {
		return &paramError{err}
	}
	return nil
}

// configInput maps the wire config object to script.Config.
type configInput struct {
	Interpreter string `json:"interpreter"`
	TimeoutSecs int    `json:"timeout_secs"`
}

func (c configInput) toConfig() script.Config {
	return script.Config{
		Interpreter: c.Interpreter,
		TimeoutSecs: c.TimeoutSecs,
	}
}

func (h *toolHandlers) dispatchTool(ctx context.Context, name string, id server.Identity, args json.RawMessage) (map[string]any, error) {
	svc := h.svc
	owner := id.OwnerEmail
	switch name {
	case tool("describe"):
		return toolDescribe()

	case tool("create"):
		var in struct {
			Name   string      `json:"name"`
			Body   string      `json:"body"`
			Config configInput `json:"config"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		sc, err := svc.Create(ctx, owner, script.CreateInput{
			Name:   in.Name,
			Body:   in.Body,
			Config: in.Config.toConfig(),
		})
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"script_id": sc.ID})

	case tool("import"):
		var in struct {
			SourcePath string `json:"source_path"`
			Name       string `json:"name"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		sc, err := svc.Import(ctx, owner, in.SourcePath, in.Name)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"script_id": sc.ID, "name": sc.Name})

	case tool("list"):
		scripts, err := svc.List(ctx, owner)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"scripts": scripts})

	case tool("get"):
		var in struct {
			ScriptID string `json:"script_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		detail, err := svc.Get(ctx, owner, in.ScriptID)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(detail)

	case tool("update"):
		var in struct {
			ScriptID string       `json:"script_id"`
			Name     *string      `json:"name"`
			Body     *string      `json:"body"`
			Config   *configInput `json:"config"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		upd := script.UpdateInput{Name: in.Name, Body: in.Body}
		if in.Config != nil {
			c := in.Config.toConfig()
			upd.Config = &c
		}
		sc, err := svc.Update(ctx, owner, in.ScriptID, upd)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(sc)

	case tool("delete"):
		var in struct {
			ScriptID string `json:"script_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		if err := svc.Delete(ctx, owner, in.ScriptID); err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"deleted": in.ScriptID})

	case tool("set_trigger"):
		var in struct {
			ScriptID    string `json:"script_id"`
			Source      string `json:"source"`
			EventFilter string `json:"event_filter"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		trig, err := svc.SetTrigger(ctx, owner, in.ScriptID, in.Source, in.EventFilter)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(trig)

	case tool("clear_trigger"):
		var in struct {
			ScriptID    string `json:"script_id"`
			Source      string `json:"source"`
			EventFilter string `json:"event_filter"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		if err := svc.ClearTrigger(ctx, owner, in.ScriptID, in.Source, in.EventFilter); err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"cleared": in.ScriptID})

	case tool("run"):
		var in struct {
			ScriptID string `json:"script_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		run, err := svc.Run(ctx, owner, in.ScriptID)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"run_id": run.ID, "status": run.Status, "started_at": run.StartedAt})

	case tool("run_list"):
		var in struct {
			ScriptID string `json:"script_id"`
			Status   string `json:"status"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		runs, err := svc.RunList(ctx, owner, in.ScriptID, in.Status)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"runs": runs})

	case tool("run_get"):
		var in struct {
			RunID string `json:"run_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		run, err := svc.RunGet(ctx, owner, in.RunID)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(run)

	case tool("run_output"):
		var in struct {
			RunID  string `json:"run_id"`
			Stream string `json:"stream"`
			Offset int    `json:"offset"`
			Limit  int    `json:"limit"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		out, err := svc.RunOutput(ctx, owner, in.RunID, in.Stream, in.Offset, in.Limit)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultText(out), nil

	case tool("run_cancel"):
		var in struct {
			RunID string `json:"run_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		if err := svc.RunCancel(ctx, owner, in.RunID); err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"cancelled": in.RunID})

	case tool("run_fs_list"):
		var in struct {
			RunID string `json:"run_id"`
			Path  string `json:"path"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		entries, err := svc.RunFsList(ctx, owner, in.RunID, in.Path)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultJSON(map[string]any{"entries": entries})

	case tool("run_fs_read"):
		var in struct {
			RunID  string `json:"run_id"`
			Path   string `json:"path"`
			Offset int    `json:"offset"`
			Limit  int    `json:"limit"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		out, err := svc.RunFsRead(ctx, owner, in.RunID, in.Path, in.Offset, in.Limit)
		if err != nil {
			return toolResultErr(err.Error()), nil
		}
		return toolResultText(out), nil

	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── shared helpers ──────────────────────────────────────────────────────

func toolResultText(text string) map[string]any {
	return appkitmcp.TextResult(text)
}

func toolResultErr(msg string) map[string]any {
	return appkitmcp.ErrorResult(msg)
}

func toolResultJSON(v any) (map[string]any, error) {
	return appkitmcp.JSONResult(v)
}
