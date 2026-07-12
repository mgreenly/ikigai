package mcp

import (
	"context"
	"encoding/json"
	"net/url"
	"path"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"prompts/internal/prompt"
)

// toolPrefix brands every MCP tool name. Prompts currently exposes bare names.
const toolPrefix = ""

func tool(verb string) string { return toolPrefix + verb }

// Tools returns prompts' domain tool table. Chassis-owned tools such as health
// and reflection are supplied by appkit/mcp and must not be declared here.
func Tools(svc *prompt.Service, contentBase string) []appkitmcp.Tool {
	return []appkitmcp.Tool{
		desc(tool("describe"), "Return a detailed overview of prompts: what a prompt vs a run is, the create→run→poll→read lifecycle, full concurrency, the per-run sandbox, and LogRecord JSONL run output. Config requires provider (anthropic, openai, google, zai) and model; optional keys tune sampling (temperature, top_p), output size (max_tokens), reasoning (effort, thinking_budget, thinking_level, thinking), retry/backoff behavior (max_attempts, base_delay, max_delay, max_elapsed, ignore_retry_after), tool loops (tool_loop_limit), and provider endpoint override (base_url). Call this first if you're unfamiliar with prompts. Takes no inputs.", obj(map[string]any{}),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				return toolDescribe()
			}),

		desc(tool("create"), "Create a new prompt for the caller. A prompt is a reusable definition (user_prompt, config, optional name/system_prompt) that you run on demand or wire to event triggers. Returns the new prompt_id. Optionally attach event triggers inline. Event-triggered runs receive the triggering event in the prompt (see describe / set_trigger).", obj(map[string]any{
			"user_prompt":   typ("string"),
			"config":        configSchema(),
			"name":          typ("string"),
			"system_prompt": typ("string"),
			"triggers":      triggersSchema(),
		}, "user_prompt", "config"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
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
				triggers := make([]prompt.TriggerSpec, 0, len(in.Triggers))
				for _, t := range in.Triggers {
					triggers = append(triggers, prompt.TriggerSpec{Filter: string(t)})
				}
				p, err := svc.Create(ctx, id.OwnerEmail, prompt.CreateInput{
					Name:         in.Name,
					UserPrompt:   in.UserPrompt,
					SystemPrompt: in.SystemPrompt,
					Config:       in.Config.toConfig(),
					Triggers:     triggers,
				})
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"prompt_id": p.ID})
			}),

		desc(tool("import"), "Import a Dropbox-mirrored file as a prompt. 'source_path' is the file's path in the dropbox mirror. Fetches the current mirror bytes over loopback (valid UTF-8 under 1 MiB) and maps the file body to the prompt's user_prompt; 'name' defaults to the basename. Re-importing the same source_path updates the same prompt (upsert); system_prompt and config keep their defaults. Returns {prompt_id, name}.", obj(map[string]any{
			"source_path": typ("string"),
			"name":        typ("string"),
		}, "source_path"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					SourcePath string `json:"source_path"`
					Name       string `json:"name"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				p, err := svc.Import(ctx, id.OwnerEmail, in.SourcePath, in.Name)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"prompt_id": p.ID, "name": p.Name})
			}),

		desc(tool("list"), "List the caller's prompts, each with its running run count and latest run (last_run).", obj(map[string]any{}),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				prompts, err := svc.List(ctx, id.OwnerEmail)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"prompts": prompts})
			}),

		desc(tool("get"), "Get one of the caller's prompts, including its running run count and latest run (last_run).", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					PromptID string `json:"prompt_id"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				detail, err := svc.Get(ctx, id.OwnerEmail, in.PromptID)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(detail)
			}),

		desc(tool("update"), "Update a prompt's name, user_prompt, system_prompt, and config. Always allowed (in-flight runs read their pinned inputs from disk, so they are unaffected).", obj(map[string]any{
			"prompt_id":     typ("string"),
			"user_prompt":   typ("string"),
			"system_prompt": typ("string"),
			"config":        configSchema(),
			"name":          typ("string"),
		}, "prompt_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
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
				p, err := svc.Update(ctx, id.OwnerEmail, in.PromptID, prompt.UpdateInput{
					Name:         in.Name,
					UserPrompt:   in.UserPrompt,
					SystemPrompt: in.SystemPrompt,
					Config:       in.Config.toConfig(),
				})
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(p)
			}),

		desc(tool("delete"), "Delete one of the caller's prompts (a tombstone: the prompt row and its triggers are removed; its runs and their on-disk artifacts survive and stay readable by run_id). Always allowed.", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					PromptID string `json:"prompt_id"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				if err := svc.Delete(ctx, id.OwnerEmail, in.PromptID); err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"deleted": in.PromptID})
			}),

		desc(tool("set_trigger"), "Attach a canonical routing-key glob filter such as dropbox:create/bills/**. The literal source is before ':'; ** crosses subject path segments.", obj(map[string]any{
			"prompt_id": typ("string"), "filter": typ("string"),
		}, "prompt_id", "filter"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					PromptID string `json:"prompt_id"`
					Filter   string `json:"filter"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				trig, err := svc.SetTrigger(ctx, id.OwnerEmail, in.PromptID, in.Filter)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(trig)
			}),

		desc(tool("clear_trigger"), "Remove one canonical routing-key filter from one of the caller's prompts.", obj(map[string]any{
			"prompt_id": typ("string"), "filter": typ("string"),
		}, "prompt_id", "filter"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					PromptID string `json:"prompt_id"`
					Filter   string `json:"filter"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				if err := svc.ClearTrigger(ctx, id.OwnerEmail, in.PromptID, in.Filter); err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"cleared": in.PromptID})
			}),

		desc(tool("run"), "Start a run for one of the caller's prompts. Always allowed — runs are fully concurrent, each in its own per-run sandbox. Returns the new run_id, status (\"running\"), and start time.", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					PromptID string `json:"prompt_id"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				run, err := svc.Run(ctx, id.OwnerEmail, in.PromptID)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"run_id": run.ID, "status": run.Status, "started_at": run.StartedAt})
			}),

		desc(tool("run_list"), "List the runs of one of the caller's prompts, newest first.", obj(map[string]any{
			"prompt_id": typ("string"),
		}, "prompt_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					PromptID string `json:"prompt_id"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				runs, err := svc.RunList(ctx, id.OwnerEmail, in.PromptID)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"runs": runs})
			}),

		desc(tool("run_get"), "Get one run by run_id (the run stays readable after its prompt is deleted).", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					RunID string `json:"run_id"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				run, err := svc.RunGet(ctx, id.OwnerEmail, in.RunID)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(run)
			}),

		desc(tool("run_output"), "Read a run's output log by run_id (append-only stream-json, one event per line). offset is 1-based; limit caps the number of lines (<=0 means from start / no limit).", obj(map[string]any{
			"run_id": typ("string"),
			"offset": typ("integer"),
			"limit":  typ("integer"),
		}, "run_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					RunID  string `json:"run_id"`
					Offset int    `json:"offset"`
					Limit  int    `json:"limit"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				out, err := svc.RunOutput(ctx, id.OwnerEmail, in.RunID, in.Offset, in.Limit)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.TextResult(out), nil
			}),

		desc(tool("run_cancel"), "Cancel an in-flight run by run_id. Idempotent.", obj(map[string]any{
			"run_id": typ("string"),
		}, "run_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					RunID string `json:"run_id"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				if err := svc.RunCancel(ctx, id.OwnerEmail, in.RunID); err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.JSONResult(map[string]any{"cancelled": in.RunID})
			}),

		desc(tool("run_fs_list"), "List entries under path within a run's sandbox folder by run_id (path defaults to the sandbox root). Non-directory entries include a loopback content_url for byte fetch by services (a run's Fetch tool or dropbox put(source_url)), not by the agent.", obj(map[string]any{
			"run_id": typ("string"),
			"path":   typ("string"),
		}, "run_id"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					RunID string `json:"run_id"`
					Path  string `json:"path"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				entries, err := svc.RunFsList(ctx, id.OwnerEmail, in.RunID, in.Path)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				rendered := make([]map[string]any, 0, len(entries))
				for _, entry := range entries {
					out := map[string]any{
						"name":   entry.Name,
						"is_dir": entry.IsDir,
						"size":   entry.Size,
					}
					if !entry.IsDir {
						query := url.Values{"run_id": {in.RunID}, "path": {path.Join(in.Path, entry.Name)}}
						out["content_url"] = contentBase + "/run-content?" + query.Encode()
					}
					rendered = append(rendered, out)
				}
				return appkitmcp.JSONResult(map[string]any{"entries": rendered})
			}),

		desc(tool("run_fs_read"), "Read a file within a run's sandbox folder by run_id. offset is 1-based; limit caps the number of lines (<=0 means from start / no limit).", obj(map[string]any{
			"run_id": typ("string"),
			"path":   typ("string"),
			"offset": typ("integer"),
			"limit":  typ("integer"),
		}, "run_id", "path"),
			func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
				var in struct {
					RunID  string `json:"run_id"`
					Path   string `json:"path"`
					Offset int    `json:"offset"`
					Limit  int    `json:"limit"`
				}
				if err := parseArgs(args, &in); err != nil {
					return nil, err
				}
				out, err := svc.RunFsRead(ctx, id.OwnerEmail, in.RunID, in.Path, in.Offset, in.Limit)
				if err != nil {
					return appkitmcp.ErrorResult(err.Error()), nil
				}
				return appkitmcp.TextResult(out), nil
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

// triggersSchema is create's optional inline canonical-key filter array.
func triggersSchema() map[string]any {
	return map[string]any{
		"type":  "array",
		"items": typ("string"),
	}
}

// paramError marks genuinely unparseable tool arguments — mapped to JSON-RPC
// -32602 by appkit/mcp rather than an MCP isError tool result.
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
	Provider         string   `json:"provider"`
	Model            string   `json:"model"`
	Temperature      *float64 `json:"temperature"`
	TopP             *float64 `json:"top_p"`
	MaxTokens        int      `json:"max_tokens"`
	Effort           string   `json:"effort"`
	ThinkingBudget   *int     `json:"thinking_budget"`
	ThinkingLevel    string   `json:"thinking_level"`
	Thinking         *bool    `json:"thinking"`
	MaxAttempts      int      `json:"max_attempts"`
	BaseDelay        string   `json:"base_delay"`
	MaxDelay         string   `json:"max_delay"`
	MaxElapsed       string   `json:"max_elapsed"`
	IgnoreRetryAfter bool     `json:"ignore_retry_after"`
	ToolLoopLimit    int      `json:"tool_loop_limit"`
	BaseURL          string   `json:"base_url"`
}

func (c configInput) toConfig() prompt.Config {
	return prompt.Config{
		Provider:         c.Provider,
		Model:            c.Model,
		Temperature:      c.Temperature,
		TopP:             c.TopP,
		MaxTokens:        c.MaxTokens,
		Effort:           c.Effort,
		ThinkingBudget:   c.ThinkingBudget,
		ThinkingLevel:    c.ThinkingLevel,
		Thinking:         c.Thinking,
		MaxAttempts:      c.MaxAttempts,
		BaseDelay:        c.BaseDelay,
		MaxDelay:         c.MaxDelay,
		MaxElapsed:       c.MaxElapsed,
		IgnoreRetryAfter: c.IgnoreRetryAfter,
		ToolLoopLimit:    c.ToolLoopLimit,
		BaseURL:          c.BaseURL,
	}
}

// triggerInput maps one wire trigger object to prompt.TriggerSpec.
type triggerInput string
