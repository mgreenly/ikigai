package mcp

import (
	"context"
	"crypto/md5"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io"
	"io/fs"
	"os"
	"path/filepath"

	"agentkit/tools"
	"agentkit/wire"
)

// The five file tools bridge the MCP surface to agentkit's jailed file tools.
// Each MCP file tool (ikigenba_sites_file_write/file_read/file_edit/file_glob/file_grep) maps to an
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

// toolFileList walks a site's working tree and returns every regular file with
// its path (relative to the working root), size, and md5. It is native (not an
// agentkit bridge) because no agentkit tool returns md5. An optional "path"
// scopes the walk to a subdirectory, confined to the working root; a missing
// scope dir yields an empty list rather than an error.
func (h *Handler) toolFileList(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Site string `json:"site"`
		Path string `json:"path"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.Site == "" {
		return errResultMsg("invalid_site", "missing required \"site\" argument"), nil
	}
	if _, err := h.store.Get(ctx, a.Site); err != nil {
		return errResult(err), nil
	}

	root := h.layout.WorkingDir(a.Site)
	scope := root
	if a.Path != "" {
		s, err := confinePath(root, a.Path)
		if err != nil {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		scope = s
	}

	files := make([]map[string]any, 0)
	walkErr := filepath.WalkDir(scope, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() || !d.Type().IsRegular() {
			return nil
		}
		md5hex, size, herr := hashFile(p)
		if herr != nil {
			return herr
		}
		rel, _ := filepath.Rel(root, p)
		rel = filepath.ToSlash(rel)
		files = append(files, map[string]any{"path": rel, "size": size, "md5": md5hex})
		return nil
	})
	if walkErr != nil && !errors.Is(walkErr, fs.ErrNotExist) {
		return errResultMsg("walk", walkErr.Error()), nil
	}

	return toolResultJSON(map[string]any{"site": a.Site, "files": files})
}

// toolFileWrite is the NATIVE handler for ikigenba_sites_file_write. Unlike the
// other four file tools, write is not bridged through agentkit: agentkit's Write
// has no append mode and silently drops unknown fields, so the "append" flag
// could not survive the bridge. It writes (truncate by default, append when
// append:true) to a path confined under the site's working dir.
func (h *Handler) toolFileWrite(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Site     string `json:"site"`
		FilePath string `json:"file_path"`
		Content  string `json:"content"`
		Append   bool   `json:"append"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.Site == "" {
		return errResultMsg("invalid_site", "missing required \"site\" argument"), nil
	}
	if _, err := h.store.Get(ctx, a.Site); err != nil {
		return errResult(err), nil
	}

	path, err := confinePath(h.layout.WorkingDir(a.Site), a.FilePath)
	if err != nil {
		return errResultMsg("path_escapes_working_dir", err.Error()), nil
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return errResultMsg("write", err.Error()), nil
	}

	flag := os.O_CREATE | os.O_WRONLY | os.O_TRUNC
	if a.Append {
		flag = os.O_CREATE | os.O_WRONLY | os.O_APPEND
	}
	f, err := os.OpenFile(path, flag, 0o644)
	if err != nil {
		return errResultMsg("write", err.Error()), nil
	}
	_, werr := f.Write([]byte(a.Content))
	cerr := f.Close()
	if werr != nil {
		return errResultMsg("write", werr.Error()), nil
	}
	if cerr != nil {
		return errResultMsg("write", cerr.Error()), nil
	}

	return toolResultJSON(map[string]any{"written": a.FilePath, "site": a.Site, "appended": a.Append})
}

// hashFile streams the file at path through md5, returning the hex digest and
// the byte count.
func hashFile(path string) (md5hex string, size int64, err error) {
	f, err := os.Open(path)
	if err != nil {
		return "", 0, err
	}
	defer f.Close()
	h := md5.New()
	n, err := io.Copy(h, f)
	if err != nil {
		return "", 0, err
	}
	return hex.EncodeToString(h.Sum(nil)), n, nil
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
