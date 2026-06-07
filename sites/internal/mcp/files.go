package mcp

import (
	"context"
	"encoding/json"

	"agentkit/tools"
	"agentkit/wire"
)

// The five file tools bridge the MCP surface to agentkit's jailed file tools.
// Each MCP file tool (ikigenba_sites_write/read/edit/glob/grep) maps to an
// agentkit canonical tool (Write/Read/Edit/Glob/Grep) executed against a
// per-site sandbox root = layout.WorkingDir(site). Confinement is NOT
// reimplemented here: agentkit/tools.Dispatch confines every path argument
// under the sandbox root via confine.go, so absolute paths and '..' escapes are
// rejected for free. This is the single trust boundary for file access.
//
// The MCP inputSchema for each file tool is agentkit's InputSchema for the
// underlying tool PLUS a required string "site" property naming the site whose
// working dir is the sandbox root. The agentkit decoders unmarshal into typed
// structs, so the extra "site" field is tolerated (ignored) and the args can be
// passed straight through.

// agentkitToolName maps an agentkit canonical tool Name to its InputSchema for
// the file-tool descriptors. Built once from the agentkit registry so the
// schemas can never drift from what Dispatch actually decodes.
func agentkitSchemas() map[string]json.RawMessage {
	out := map[string]json.RawMessage{}
	for _, d := range tools.All() {
		out[d.Name] = d.InputSchema
	}
	return out
}

// fileToolDescriptor builds an MCP descriptor for one file tool: the agentkit
// InputSchema for agentName, augmented with a required "site" string property,
// branded under the verb's ikigenba_sites_ name.
func fileToolDescriptor(verb, agentName, description string) map[string]any {
	schema := withSiteProperty(agentkitSchemas()[agentName])
	return desc(tool(verb), description, schema)
}

// withSiteProperty decodes an agentkit InputSchema object and adds a required
// "site" string property to it. The agentkit schema is a JSON object schema with
// "properties" and (usually) "required" arrays.
func withSiteProperty(raw json.RawMessage) map[string]any {
	var schema map[string]any
	_ = json.Unmarshal(raw, &schema)
	if schema == nil {
		schema = map[string]any{"type": "object"}
	}
	props, _ := schema["properties"].(map[string]any)
	if props == nil {
		props = map[string]any{}
		schema["properties"] = props
	}
	props["site"] = descTyp("string", "site slug whose working dir is the sandbox root")

	// Prepend site to required so it is mandatory. required is decoded from JSON
	// as []any; rebuild it preserving the existing entries.
	req := []any{"site"}
	if existing, ok := schema["required"].([]any); ok {
		req = append(req, existing...)
	}
	schema["required"] = req
	return schema
}

// toolFile is the shared bridge for the five file tools. agentName is the
// agentkit canonical tool name (Write/Read/Edit/Glob/Grep). The args contain the
// underlying tool's fields plus a required "site"; site selects the sandbox root
// and is otherwise ignored by the agentkit decoder.
func (h *Handler) toolFile(ctx context.Context, agentName string, raw json.RawMessage) (map[string]any, error) {
	// Pull and validate site. Get both confirms existence and rejects an empty
	// or unknown slug, mapping to the stable not_found/error envelope.
	var head struct {
		Site string `json:"site"`
	}
	if err := unmarshalArgs(raw, &head); err != nil {
		return nil, err
	}
	if head.Site == "" {
		return errResultMsg("invalid_site", "missing required \"site\" argument"), nil
	}
	if _, err := h.store.Get(ctx, head.Site); err != nil {
		return errResult(err), nil
	}

	sandboxRoot := h.layout.WorkingDir(head.Site)

	// Build the agentkit tool-use block. Input is the original args verbatim; the
	// extra "site" field is ignored by agentkit's typed decoders. A nil/empty raw
	// becomes an empty object so Dispatch can decode it.
	input := raw
	if len(input) == 0 {
		input = json.RawMessage(`{}`)
	}
	block := wire.ToolUseBlock{Type: "tool_use", ID: "sites", Name: agentName, Input: input}

	result, _, err := tools.Dispatch(ctx, sandboxRoot, block)
	if err != nil {
		return nil, err
	}
	return renderToolResultBlock(result), nil
}

// renderToolResultBlock maps an agentkit ToolResultBlock onto the MCP tool
// result shape. The block's Content is a JSON value (most often a JSON string);
// it is rendered as text. IsError selects the MCP error envelope so an agent can
// distinguish a confinement rejection / tool failure from success, consistently
// with the lifecycle tools.
func renderToolResultBlock(b wire.ToolResultBlock) map[string]any {
	text := contentText(b.Content)
	if b.IsError {
		return toolResultErr(text)
	}
	return toolResultText(text)
}

// contentText extracts a human-readable string from a tool_result Content value.
// Content is a JSON value: a JSON string unwraps to its value; anything else is
// returned as its raw JSON text.
func contentText(raw json.RawMessage) string {
	if len(raw) == 0 {
		return ""
	}
	var s string
	if err := json.Unmarshal(raw, &s); err == nil {
		return s
	}
	return string(raw)
}
