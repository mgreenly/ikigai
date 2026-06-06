package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"

	"appkit"

	"ralph/internal/session"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// `ikigenba` plus the service name, defined once and used in BOTH the descriptor
// list and the dispatch switch so the two sites cannot drift.
const toolPrefix = "ikigenba_ralph_"

// tool returns the branded MCP tool name for a verb.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the full ikigenba_ralph_* tool set:
// ikigenba_ralph_health (the auth proof + diagnostics) plus the ten
// session/run/fs tools, each mapped to a session.Service method in dispatchTool.
// Schemas are hand-coded JSON Schema; required fields are marked so MCP clients
// prompt for them.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		desc(tool("describe"), "Return a detailed overview of ralph: what a session is, the create→run→poll→read lifecycle, the stream-json output format, the sandbox model, and a worked example. Call this first if you're unfamiliar with ralph. Takes no inputs.", obj(map[string]any{})),

		desc(tool("health"), "Health + diagnostics for the ralph service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id) as established by the platform's auth gate — the end-to-end auth-chain proof. Takes no inputs.", obj(map[string]any{})),

		desc(tool("session_create"), "Create a new idle agent session for the caller. Returns the new session_id and its status.", obj(map[string]any{
			"prompt":        typ("string"),
			"config":        configSchema(),
			"name":          typ("string"),
			"system_prompt": typ("string"),
		}, "prompt", "config")),

		desc(tool("session_list"), "List the caller's sessions.", obj(map[string]any{})),

		desc(tool("session_get"), "Get one of the caller's sessions, including its latest run (last_run).", obj(map[string]any{
			"session_id": typ("string"),
		}, "session_id")),

		desc(tool("session_update"), "Update a session's name, prompt, system_prompt, and config. Rejected while the session is running.", obj(map[string]any{
			"session_id":    typ("string"),
			"prompt":        typ("string"),
			"system_prompt": typ("string"),
			"config":        configSchema(),
			"name":          typ("string"),
		}, "session_id")),

		desc(tool("session_delete"), "Delete one of the caller's sessions (and its sandbox + run logs). Rejected while running.", obj(map[string]any{
			"session_id": typ("string"),
		}, "session_id")),

		desc(tool("session_run"), "Start a run for one of the caller's sessions. Rejected if a run is already in flight. Returns the run status and start time.", obj(map[string]any{
			"session_id": typ("string"),
		}, "session_id")),

		desc(tool("session_cancel"), "Cancel the in-flight run for one of the caller's sessions. Idempotent.", obj(map[string]any{
			"session_id": typ("string"),
		}, "session_id")),

		desc(tool("session_output"), "Read the latest run's output log (append-only stream-json, one event per line). offset is 1-based; limit caps the number of lines (<=0 means from start / no limit).", obj(map[string]any{
			"session_id": typ("string"),
			"offset":     typ("integer"),
			"limit":      typ("integer"),
		}, "session_id")),

		desc(tool("session_fs_list"), "List entries under path within a session's sandbox folder (path defaults to the session root).", obj(map[string]any{
			"session_id": typ("string"),
			"path":       typ("string"),
		}, "session_id")),

		desc(tool("session_fs_read"), "Read a file within a session's sandbox folder. offset is 1-based; limit caps the number of lines (<=0 means from start / no limit).", obj(map[string]any{
			"session_id": typ("string"),
			"path":       typ("string"),
			"offset":     typ("integer"),
			"limit":      typ("integer"),
		}, "session_id", "path")),
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

// configSchema is the shared session.Config input schema (model required).
func configSchema() map[string]any {
	return obj(map[string]any{
		"model":       typ("string"),
		"effort":      typ("string"),
		"max_tokens":  typ("integer"),
		"temperature": typ("number"),
	}, "model")
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

// configFromInput maps the wire config object to session.Config.
type configInput struct {
	Model       string   `json:"model"`
	Effort      string   `json:"effort"`
	MaxTokens   int      `json:"max_tokens"`
	Temperature *float64 `json:"temperature"`
}

func (c configInput) toConfig() session.Config {
	return session.Config{
		Model:       c.Model,
		Effort:      c.Effort,
		MaxTokens:   c.MaxTokens,
		Temperature: c.Temperature,
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

	case tool("session_create"):
		var in struct {
			Prompt       string      `json:"prompt"`
			Config       configInput `json:"config"`
			Name         string      `json:"name"`
			SystemPrompt string      `json:"system_prompt"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		sess, err := svc.Create(ctx, owner, session.CreateInput{
			Name:         in.Name,
			Prompt:       in.Prompt,
			SystemPrompt: in.SystemPrompt,
			Config:       in.Config.toConfig(),
		})
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"session_id": sess.ID, "status": sess.Status})

	case tool("session_list"):
		sessions, err := svc.List(ctx, owner)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"sessions": sessions})

	case tool("session_get"):
		var in struct {
			SessionID string `json:"session_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		detail, err := svc.Get(ctx, owner, in.SessionID)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(detail)

	case tool("session_update"):
		var in struct {
			SessionID    string      `json:"session_id"`
			Prompt       string      `json:"prompt"`
			SystemPrompt string      `json:"system_prompt"`
			Config       configInput `json:"config"`
			Name         string      `json:"name"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		sess, err := svc.Update(ctx, owner, in.SessionID, session.UpdateInput{
			Name:         in.Name,
			Prompt:       in.Prompt,
			SystemPrompt: in.SystemPrompt,
			Config:       in.Config.toConfig(),
		})
		if err != nil {
			return nil, err
		}
		return toolResultJSON(sess)

	case tool("session_delete"):
		var in struct {
			SessionID string `json:"session_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		if err := svc.Delete(ctx, owner, in.SessionID); err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"deleted": in.SessionID})

	case tool("session_run"):
		var in struct {
			SessionID string `json:"session_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		run, err := svc.Run(ctx, owner, in.SessionID)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"status": "running", "started_at": run.StartedAt, "run_id": run.ID})

	case tool("session_cancel"):
		var in struct {
			SessionID string `json:"session_id"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		if err := svc.Cancel(ctx, owner, in.SessionID); err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"cancelled": in.SessionID})

	case tool("session_output"):
		var in struct {
			SessionID string `json:"session_id"`
			Offset    int    `json:"offset"`
			Limit     int    `json:"limit"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		out, err := svc.Output(ctx, owner, in.SessionID, in.Offset, in.Limit)
		if err != nil {
			return nil, err
		}
		return toolResultText(out), nil

	case tool("session_fs_list"):
		var in struct {
			SessionID string `json:"session_id"`
			Path      string `json:"path"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		entries, err := svc.FsList(ctx, owner, in.SessionID, in.Path)
		if err != nil {
			return nil, err
		}
		return toolResultJSON(map[string]any{"entries": entries})

	case tool("session_fs_read"):
		var in struct {
			SessionID string `json:"session_id"`
			Path      string `json:"path"`
			Offset    int    `json:"offset"`
			Limit     int    `json:"limit"`
		}
		if err := parseArgs(args, &in); err != nil {
			return nil, err
		}
		out, err := svc.FsRead(ctx, owner, in.SessionID, in.Path, in.Offset, in.Limit)
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
// optional per-service reporter (nil → {}); ralph supplies none, so details is {}.
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
