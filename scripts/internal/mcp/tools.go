package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"

	"appkit"

	"scripts/internal/script"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// `ikigenba` plus the service name, defined once and used in BOTH the descriptor
// list and the dispatch switch so the two sites cannot drift.
const toolPrefix = ""

// tool returns the branded MCP tool name for a verb.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the full 16-tool surface (PLAN.md
// §A10 / ARCHITECTURE.md §6): health + describe + script CRUD + triggers + run
// lifecycle + run-scoped fs readers. Each maps to a script.Service method in
// dispatchTool.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("describe"), "Return a detailed overview of scripts: what a script is, the create→run→poll→read lifecycle, triggers, and the runtime contract. Call this first if you're unfamiliar with scripts. Takes no inputs.", obj(map[string]any{})),

		desc(tool("health"), "Health + diagnostics for the scripts service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). details is the static runtime contract (python_version, bash_version, network, packages). Takes no inputs.", obj(map[string]any{})),

		desc(tool("create"), "Create a new script for the caller. body is the Python source. Returns the new script_id.", obj(map[string]any{
			"name":   typ("string"),
			"body":   typ("string"),
			"config": configSchema(),
		}, "name", "body")),

		desc(tool("list"), "List the caller's scripts, each with its derived running_count and last_run.", obj(map[string]any{})),

		desc(tool("get"), "Get one of the caller's scripts, including running_count and last_run.", obj(map[string]any{
			"script_id": typ("string"),
		}, "script_id")),

		desc(tool("update"), "Update a script's name, body, and/or config. Any field may be omitted to leave it unchanged.", obj(map[string]any{
			"script_id": typ("string"),
			"name":      typ("string"),
			"body":      typ("string"),
			"config":    configSchema(),
		}, "script_id")),

		desc(tool("delete"), "Delete one of the caller's scripts (tombstone): the script row and its triggers are removed, but its run history and on-disk artifacts survive.", obj(map[string]any{
			"script_id": typ("string"),
		}, "script_id")),

		desc(tool("set_trigger"), "Bind a script to an upstream event. source is one of cron|crm|ledger|dropbox|prompts; event_filter is a glob over that producer's event types (e.g. \"contact.created\", \"contact.*\", \"cron.nightly\"). When a matching event fires, scripts starts a run.", obj(map[string]any{
			"script_id":    typ("string"),
			"source":       typ("string"),
			"event_filter": typ("string"),
		}, "script_id", "source", "event_filter")),

		desc(tool("clear_trigger"), "Remove an event trigger from a script.", obj(map[string]any{
			"script_id":    typ("string"),
			"source":       typ("string"),
			"event_filter": typ("string"),
		}, "script_id", "source", "event_filter")),

		desc(tool("run"), "Start a manual run of one of the caller's scripts. Always allowed — runs are fully concurrent. Returns the run_id and start time.", obj(map[string]any{
			"script_id": typ("string"),
		}, "script_id")),

		desc(tool("run_list"), "List runs, optionally filtered by script_id and/or status (running|succeeded|failed|cancelled). Each carries elapsed_secs.", obj(map[string]any{
			"script_id": typ("string"),
			"status":    typ("string"),
		})),

		desc(tool("run_get"), "Get one run by run_id, including status, exit_code, and elapsed_secs.", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id")),

		desc(tool("run_output"), "Read a run's captured output. stream is stdout|stderr|both (default both). offset is 1-based; limit caps lines (<=0 means from start / no limit).", obj(map[string]any{
			"run_id": typ("string"),
			"stream": typ("string"),
			"offset": typ("integer"),
			"limit":  typ("integer"),
		}, "run_id")),

		desc(tool("run_cancel"), "Cancel an in-flight run by run_id (kills the process group). Idempotent.", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id")),

		desc(tool("run_fs_list"), "List entries under path within a run's persisted dir tree (path defaults to the run root).", obj(map[string]any{
			"run_id": typ("string"),
			"path":   typ("string"),
		}, "run_id")),

		desc(tool("run_fs_read"), "Read a file within a run's persisted dir. offset is 1-based; limit caps lines (<=0 means from start / no limit).", obj(map[string]any{
			"run_id": typ("string"),
			"path":   typ("string"),
			"offset": typ("integer"),
			"limit":  typ("integer"),
		}, "run_id", "path")),
	}
}

func desc(name, description string, schema map[string]any) map[string]any {
	return map[string]any{"name": name, "description": description, "inputSchema": schema}
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

// ── dispatch ──────────────────────────────────────────────────────────────

type toolCallParams struct {
	Name      string          `json:"name"`
	Arguments json.RawMessage `json:"arguments"`
}

func (h *Handler) handleToolCall(ctx context.Context, w http.ResponseWriter, req jsonRPCRequest, id Identity) {
	var p toolCallParams
	if err := json.Unmarshal(req.Params, &p); err != nil {
		writeJSONRPCError(w, req.ID, -32602, "invalid params")
		return
	}
	res, err := h.dispatchTool(ctx, p.Name, id, p.Arguments)
	if err != nil {
		// Domain/validation errors surface as MCP tool errors (isError:true
		// content). -32602 is reserved for unparseable arguments.
		var pe *paramError
		if errors.As(err, &pe) {
			writeJSONRPCError(w, req.ID, -32602, pe.Error())
			return
		}
		writeJSONRPCResult(w, req.ID, toolResultErr(err.Error()))
		return
	}
	writeJSONRPCResult(w, req.ID, res)
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

func (h *Handler) dispatchTool(ctx context.Context, name string, id Identity, args json.RawMessage) (map[string]any, error) {
	svc := h.svc
	owner := id.OwnerEmail
	switch name {
	case tool("describe"):
		return toolDescribe()

	case tool("health"):
		return h.toolHealth(ctx, id)

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
			return nil, err
		}
		return toolResultJSON(map[string]any{"script_id": sc.ID})

	case tool("list"):
		scripts, err := svc.List(ctx, owner)
		if err != nil {
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
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
			return nil, err
		}
		return toolResultText(out), nil

	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── tool implementations ─────────────────────────────────────────────────

// runtimeContract is the static runtime contract reported under health.details
// (PLAN.md §A10 / README.md): Python 3.11+, bash 5.x via subprocess, open
// network, and Python standard library only (no per-run pip install). It is the
// authoring contract a script body may assume — defined here, not in main.go,
// because scripts publishes a fixed runtime rather than probing the host.
func runtimeContract() map[string]any {
	return map[string]any{
		"python_version": ">=3.11",
		"bash_version":   ">=5.0",
		"network":        true,
		"packages":       "stdlib",
	}
}

// toolHealth renders the shared health envelope (status/version/service/details)
// and adds the injected caller identity (owner_email/client_id). details is the
// static runtime contract (PLAN.md §A10); any reporter the chassis wires is
// merged on top so an explicit main.go reporter could extend (never silently
// drop) the contract keys.
func (h *Handler) toolHealth(ctx context.Context, id Identity) (map[string]any, error) {
	details := runtimeContract()
	if h.health != nil {
		d, err := h.health(ctx)
		if err != nil {
			details["error"] = err.Error()
		} else {
			for k, v := range d {
				details[k] = v
			}
		}
	}
	env := appkit.Envelope(h.version, h.service, details)
	env["owner_email"] = id.OwnerEmail
	env["client_id"] = id.ClientID
	return toolResultJSON(env)
}

// ── shared helpers ──────────────────────────────────────────────────────

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
