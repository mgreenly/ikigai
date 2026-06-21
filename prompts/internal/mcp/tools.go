package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"

	"appkit"

	"prompts/internal/prompt"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// `ikigenba` plus the service name, defined once and used in BOTH the descriptor
// list and the dispatch switch so the two sites cannot drift.
const toolPrefix = ""

// tool returns the branded MCP tool name for a verb.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the full * tool set (A8). The surface
// splits on the addressing key: prompt-addressed operations are BARE verbs keyed
// by prompt_id; run-addressed operations live under the run_* namespace keyed by
// run_id. Each maps to a prompt.Service method in dispatchTool. Schemas are
// hand-coded JSON Schema; required fields are marked so MCP clients prompt for
// them.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("describe"), "Return a detailed overview of prompts: what a prompt vs a run is, the create→run→poll→read lifecycle, full concurrency, the per-run sandbox, and LogRecord JSONL run output. Config requires provider (anthropic, openai, google, zai) and model; optional keys tune sampling (temperature, top_p), output size (max_tokens), reasoning (effort, thinking_budget, thinking_level, thinking), retry/backoff behavior (max_attempts, base_delay, max_delay, max_elapsed, ignore_retry_after), tool loops (tool_loop_limit), and provider endpoint override (base_url). Call this first if you're unfamiliar with prompts. Takes no inputs.", obj(map[string]any{})),

		desc(tool("health"), "Health + diagnostics for the prompts service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id). Takes no inputs.", obj(map[string]any{})),

		desc(tool("create"), "Create a new prompt for the caller. A prompt is a reusable definition (user_prompt, config, optional name/system_prompt) that you run on demand or wire to event triggers. Returns the new prompt_id. Optionally attach event triggers inline. Event-triggered runs receive the triggering event in the prompt (see describe / set_trigger).", obj(map[string]any{
			"user_prompt":   typ("string"),
			"config":        configSchema(),
			"name":          typ("string"),
			"system_prompt": typ("string"),
			"triggers":      triggersSchema(),
		}, "user_prompt", "config")),

		desc(tool("import"), "Import a Dropbox-mirrored file as a prompt. 'source_path' is the file's path in the dropbox mirror. Fetches the current mirror bytes over loopback (valid UTF-8 under 1 MiB) and maps the file body to the prompt's user_prompt; 'name' defaults to the basename. Re-importing the same source_path updates the same prompt (upsert); system_prompt and config keep their defaults. Returns {prompt_id, name}.", obj(map[string]any{
			"source_path": typ("string"),
			"name":        typ("string"),
		}, "source_path")),

		desc(tool("list"), "List the caller's prompts, each with its running run count and latest run (last_run).", obj(map[string]any{})),

		desc(tool("get"), "Get one of the caller's prompts, including its running run count and latest run (last_run).", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id")),

		desc(tool("update"), "Update a prompt's name, user_prompt, system_prompt, and config. Always allowed (in-flight runs read their pinned inputs from disk, so they are unaffected).", obj(map[string]any{
			"prompt_id":     typ("string"),
			"user_prompt":   typ("string"),
			"system_prompt": typ("string"),
			"config":        configSchema(),
			"name":          typ("string"),
		}, "prompt_id")),

		desc(tool("delete"), "Delete one of the caller's prompts (a tombstone: the prompt row and its triggers are removed; its runs and their on-disk artifacts survive and stay readable by run_id). Always allowed.", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id")),

		desc(tool("set_trigger"), "Attach one (source, event_filter) event trigger to one of the caller's prompts: when a matching event arrives from the named producer, prompts starts a run for the prompt. source is the producer (cron|crm|ledger|dropbox|scripts|prompts); event_filter is the event type/glob it publishes, e.g. \"cron.nightly\", \"file.created\", \"run.succeeded\". A prompt may hold several bindings — call repeatedly. An unknown source or an event_filter the producer never publishes is rejected. The run receives the triggering event as a second block in its prompt — see describe for the contract.", obj(map[string]any{
			"prompt_id":    typ("string"),
			"source":       typ("string"),
			"event_filter": typ("string"),
		}, "prompt_id", "source", "event_filter")),

		desc(tool("clear_trigger"), "Remove one (source, event_filter) event trigger from one of the caller's prompts.", obj(map[string]any{
			"prompt_id":    typ("string"),
			"source":       typ("string"),
			"event_filter": typ("string"),
		}, "prompt_id", "source", "event_filter")),

		desc(tool("run"), "Start a run for one of the caller's prompts. Always allowed — runs are fully concurrent, each in its own per-run sandbox. Returns the new run_id, status (\"running\"), and start time.", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id")),

		desc(tool("run_list"), "List the runs of one of the caller's prompts, newest first.", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id")),

		desc(tool("run_get"), "Get one run by run_id (the run stays readable after its prompt is deleted).", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id")),

		desc(tool("run_output"), "Read a run's output log by run_id (append-only stream-json, one event per line). offset is 1-based; limit caps the number of lines (<=0 means from start / no limit).", obj(map[string]any{
			"run_id": typ("string"),
			"offset": typ("integer"),
			"limit":  typ("integer"),
		}, "run_id")),

		desc(tool("run_cancel"), "Cancel an in-flight run by run_id. Idempotent.", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id")),

		desc(tool("run_fs_list"), "List entries under path within a run's sandbox folder by run_id (path defaults to the sandbox root).", obj(map[string]any{
			"run_id": typ("string"),
			"path":   typ("string"),
		}, "run_id")),

		desc(tool("run_fs_read"), "Read a file within a run's sandbox folder by run_id. offset is 1-based; limit caps the number of lines (<=0 means from start / no limit).", obj(map[string]any{
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

// configSchema is the shared prompt.Config input schema.
func configSchema() map[string]any {
	return obj(map[string]any{
		"provider":           typ("string"),
		"model":              typ("string"),
		"temperature":        typ("number"),
		"top_p":              typ("number"),
		"max_tokens":         typ("integer"),
		"effort":             typ("string"),
		"thinking_budget":    typ("integer"),
		"thinking_level":     typ("string"),
		"thinking":           typ("string"),
		"max_attempts":       typ("integer"),
		"base_delay":         typ("string"),
		"max_delay":          typ("string"),
		"max_elapsed":        typ("string"),
		"ignore_retry_after": typ("boolean"),
		"tool_loop_limit":    typ("integer"),
		"base_url":           typ("string"),
	}, "provider", "model")
}

// triggersSchema is create's optional inline triggers array: each element is a
// {source, event_filter} binding applied via SetTrigger after the prompt row is
// inserted (same validation as set_trigger).
func triggersSchema() map[string]any {
	return map[string]any{
		"type": "array",
		"items": obj(map[string]any{
			"source":       typ("string"),
			"event_filter": typ("string"),
		}, "source", "event_filter"),
	}
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
		// Domain/validation/sandbox errors surface as MCP tool errors
		// (isError:true content), per the MCP convention — not JSON-RPC
		// protocol errors. -32602 is reserved for unparseable arguments,
		// handled per-tool below.
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

// configFromInput maps the wire config object to prompt.Config.
type configInput struct {
	Model       string   `json:"model"`
	Effort      string   `json:"effort"`
	MaxTokens   int      `json:"max_tokens"`
	Temperature *float64 `json:"temperature"`
}

func (c configInput) toConfig() prompt.Config {
	return prompt.Config{
		Model:       c.Model,
		Effort:      c.Effort,
		MaxTokens:   c.MaxTokens,
		Temperature: c.Temperature,
	}
}

// triggerInput maps one wire trigger object to prompt.TriggerSpec.
type triggerInput struct {
	Source      string `json:"source"`
	EventFilter string `json:"event_filter"`
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
			UserPrompt   string         `json:"user_prompt"`
			Config       configInput    `json:"config"`
			Name         string         `json:"name"`
			SystemPrompt string         `json:"system_prompt"`
			Triggers     []triggerInput `json:"triggers"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		var triggers []prompt.TriggerSpec
		for _, t := range in.Triggers {
			triggers = append(triggers, prompt.TriggerSpec{Source: t.Source, EventFilter: t.EventFilter})
		}
		p, err := svc.Create(ctx, owner, prompt.CreateInput{
			Name:         in.Name,
			UserPrompt:   in.UserPrompt,
			SystemPrompt: in.SystemPrompt,
			Config:       in.Config.toConfig(),
			Triggers:     triggers,
		})
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"prompt_id": p.ID})

	case tool("import"):
		var in struct {
			SourcePath string `json:"source_path"`
			Name       string `json:"name"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		p, err := svc.Import(ctx, owner, in.SourcePath, in.Name)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"prompt_id": p.ID, "name": p.Name})

	case tool("list"):
		prompts, err := svc.List(ctx, owner)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"prompts": prompts})

	case tool("get"):
		var in struct {
			PromptID string `json:"prompt_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		detail, err := svc.Get(ctx, owner, in.PromptID)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(detail)

	case tool("update"):
		var in struct {
			PromptID     string      `json:"prompt_id"`
			UserPrompt   string      `json:"user_prompt"`
			SystemPrompt string      `json:"system_prompt"`
			Config       configInput `json:"config"`
			Name         string      `json:"name"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		p, err := svc.Update(ctx, owner, in.PromptID, prompt.UpdateInput{
			Name:         in.Name,
			UserPrompt:   in.UserPrompt,
			SystemPrompt: in.SystemPrompt,
			Config:       in.Config.toConfig(),
		})
		if err != nil {
			return nil, err
		}
		return toolResultJSON(p)

	case tool("delete"):
		var in struct {
			PromptID string `json:"prompt_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		if err := svc.Delete(ctx, owner, in.PromptID); err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"deleted": in.PromptID})

	case tool("set_trigger"):
		var in struct {
			PromptID    string `json:"prompt_id"`
			Source      string `json:"source"`
			EventFilter string `json:"event_filter"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		trig, err := svc.SetTrigger(ctx, owner, in.PromptID, in.Source, in.EventFilter)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(trig)

	case tool("clear_trigger"):
		var in struct {
			PromptID    string `json:"prompt_id"`
			Source      string `json:"source"`
			EventFilter string `json:"event_filter"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		if err := svc.ClearTrigger(ctx, owner, in.PromptID, in.Source, in.EventFilter); err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"cleared": in.PromptID})

	case tool("run"):
		var in struct {
			PromptID string `json:"prompt_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		run, err := svc.Run(ctx, owner, in.PromptID)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"run_id": run.ID, "status": run.Status, "started_at": run.StartedAt})

	case tool("run_list"):
		var in struct {
			PromptID string `json:"prompt_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		runs, err := svc.RunList(ctx, owner, in.PromptID)
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
			Offset int    `json:"offset"`
			Limit  int    `json:"limit"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		out, err := svc.RunOutput(ctx, owner, in.RunID, in.Offset, in.Limit)
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

// toolHealth renders the shared health envelope (status/version/service/details)
// and adds the injected caller identity (owner_email/client_id) — the gated MCP
// diagnostics surface and end-to-end auth-chain proof. details comes from the
// optional per-service reporter (nil → {}); prompts supplies none, so details is {}.
func (h *Handler) toolHealth(ctx context.Context, id Identity) (map[string]any, error) {
	details := map[string]any{}
	if h.health != nil {
		d, err := h.health(ctx)
		if err != nil {
			details = map[string]any{"error": err.Error()}
		} else if d != nil {
			details = d
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
