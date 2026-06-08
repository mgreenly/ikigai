package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"appkit"

	"sites/internal/sites"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = "ikigenba_sites_"

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

// toolDescriptors returns the ikigenba_sites_* lifecycle tool set: the two
// chassis tools (health, describe) plus the registry/publish lifecycle verbs.
// The five file tools (write/read/edit/glob/grep) are a later phase and append
// to this list (and to dispatchTool) — keep this slice append-friendly. Schemas
// are hand-coded; a full JSON Schema isn't required by MCP clients but improves
// the LLM hinting.
func toolDescriptors() []map[string]any {
	return []map[string]any{
		// ── chassis tools ───────────────────────────────────────────────
		desc(tool("health"), "Health + diagnostics for the sites service. Returns the fixed envelope (status, version, service, details) plus the authenticated caller's identity (owner_email, client_id) as established by the platform's auth gate — the end-to-end auth-chain proof. Takes no inputs.", obj(map[string]any{})),
		desc(tool("describe"), "Self-describe the sites service: how to host a static website. The lifecycle is create a site (a slug) → edit its working tree with the file tools → publish it to a tier (public or private) so the front door serves it; unpublish/delete to tear it down. Returns the concept overview and the lifecycle tool list. Takes no inputs.", obj(map[string]any{})),
		// ── lifecycle tools ─────────────────────────────────────────────
		desc(tool("create"), "Create a new site. 'name' is the slug (1–63 chars, lowercase alphanumeric + hyphen, must start alphanumeric); reserved names are rejected. Inserts the registry row and creates its empty working tree. Returns the created site.", obj(map[string]any{
			"name": descTyp("string", "the site slug (lowercase alnum + hyphen, 1–63 chars)"),
		}, "name")),
		desc(tool("list"), "List every site with its tier, published flag, and timestamps. Takes no inputs.", obj(map[string]any{})),
		desc(tool("delete"), "Delete a site: unpublish it (drop any served link), remove its working tree, then remove the registry row. Idempotent enough to tolerate an already-removed working tree.", obj(map[string]any{
			"name": descTyp("string", "the site slug to delete"),
		}, "name")),
		desc(tool("mkdir"), "Create a directory (and any missing parents) inside a site's working tree. 'path' is relative to the site's working root and is confined to it (absolute paths and any escape via '..' are rejected). Use this to make parent directories before writing files into nested paths.", obj(map[string]any{
			"name": descTyp("string", "the site slug whose working tree to create the directory in"),
			"path": descTyp("string", "directory path relative to the site's working root"),
		}, "name", "path")),
		desc(tool("publish"), "Publish a site to a tier so the front door serves it. 'tier' is 'public' or 'private'. Re-publishing to a different tier moves it (never reachable under both at once); re-publishing to the same tier is idempotent.", obj(map[string]any{
			"name": descTyp("string", "the site slug to publish"),
			"tier": descTyp("string", "'public' or 'private'"),
		}, "name", "tier")),
		desc(tool("unpublish"), "Unpublish a site: drop its served link and flip it back to unpublished. Safe to call on an already-unpublished site.", obj(map[string]any{
			"name": descTyp("string", "the site slug to unpublish"),
		}, "name")),
		// ── file tools (agentkit bridge) ────────────────────────────────
		// Each tool's inputSchema is agentkit's InputSchema for the underlying
		// jailed tool plus a required "site" property naming the sandbox root.
		desc(tool("file_write"), "writes content to file_path inside the site's working tree; creates parent dirs; overwrites by default, or appends when append:true.", obj(map[string]any{
			"site":      descTyp("string", "site slug whose working dir is the sandbox root"),
			"file_path": descTyp("string", "path relative to the site's working root (confined; absolute and '..' rejected)"),
			"content":   descTyp("string", "the bytes to write"),
			"append":    descTyp("boolean", "append to the file instead of overwriting; creates the file if missing (default false)"),
		}, "site", "file_path", "content")),
		fileToolDescriptor("file_read", "Read", "Read a file inside a site's working tree. 'site' selects the sandbox root; 'file_path' is relative to it and confined to it. Optional offset/limit page large files."),
		fileToolDescriptor("file_edit", "Edit", "Edit a file inside a site's working tree by replacing 'old_string' with 'new_string'. 'site' selects the sandbox root; 'file_path' is relative to it and confined to it. Set 'replace_all' to replace every occurrence."),
		fileToolDescriptor("file_glob", "Glob", "Glob for files inside a site's working tree. 'site' selects the sandbox root; 'path' (if given) is relative to it and confined to it, defaulting to the working root."),
		fileToolDescriptor("file_grep", "Grep", "Grep file contents inside a site's working tree. 'site' selects the sandbox root; 'path' (if given) is relative to it and confined to it, defaulting to the working root."),
		desc(tool("file_list"), "lists every regular file under the site's working tree with its size and md5, for reconciliation/verification against local files; path optionally scopes the walk; returned paths are relative to the working root.", obj(map[string]any{
			"site": descTyp("string", "site slug whose working dir is the sandbox root"),
			"path": descTyp("string", "optional subdirectory (relative to the working root) to scope the walk"),
		}, "site")),
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

func descTyp(t, description string) map[string]any {
	return map[string]any{"type": t, "description": description}
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
	res, err := h.dispatchTool(ctx, p.Name, p.Arguments, id)
	if err != nil {
		writeJSONRPCResult(w, req.ID, toolResultErr(err.Error()))
		return
	}
	writeJSONRPCResult(w, req.ID, res)
}

// dispatchTool routes a tool/call to its handler. The file-tool phase appends
// its five cases here alongside the existing lifecycle cases — the switch is the
// single dispatch point that must stay in lockstep with toolDescriptors.
func (h *Handler) dispatchTool(ctx context.Context, name string, argsRaw json.RawMessage, id Identity) (map[string]any, error) {
	switch name {
	case tool("health"):
		return h.toolHealth(ctx, id)
	case tool("describe"):
		return h.toolDescribe()
	case tool("create"):
		return h.toolCreate(ctx, argsRaw)
	case tool("list"):
		return h.toolList(ctx)
	case tool("delete"):
		return h.toolDelete(ctx, argsRaw)
	case tool("mkdir"):
		return h.toolMkdir(argsRaw)
	case tool("publish"):
		return h.toolPublish(ctx, argsRaw)
	case tool("unpublish"):
		return h.toolUnpublish(ctx, argsRaw)
	case tool("file_write"):
		return h.toolFileWrite(ctx, argsRaw)
	case tool("file_read"):
		return h.toolFile(ctx, "Read", argsRaw)
	case tool("file_edit"):
		return h.toolFile(ctx, "Edit", argsRaw)
	case tool("file_glob"):
		return h.toolFile(ctx, "Glob", argsRaw)
	case tool("file_grep"):
		return h.toolFile(ctx, "Grep", argsRaw)
	case tool("file_list"):
		return h.toolFileList(ctx, argsRaw)
	default:
		return nil, errors.New("unknown tool: " + name)
	}
}

// ── chassis tool implementations ───────────────────────────────────────────

// toolHealth renders the shared health envelope (status/version/service/details)
// via appkit.Envelope and then adds the authenticated caller's identity — the
// end-to-end auth-chain proof. sites supplies no reporter, so details renders as
// {} unless a Health hook was wired.
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
	env := appkit.Envelope(h.version, h.service, details) // status/version/service/details
	env["owner_email"] = id.OwnerEmail
	env["client_id"] = id.ClientID
	return toolResultJSON(env)
}

// toolDescribe is the self-describing tool: it explains what sites does and the
// lifecycle of hosting a static website, so an agent connecting for the first
// time can orient without out-of-band docs.
func (h *Handler) toolDescribe() (map[string]any, error) {
	return toolResultJSON(map[string]any{
		"service": "sites",
		"summary": "Host static websites. Each site is a slug with an editable working tree; publishing it to a tier (public or private) makes the nginx front door serve it.",
		"lifecycle": []string{
			"create — register a slug and create its empty working tree",
			"edit the working tree with the file tools (file_read/file_write/file_edit/file_glob/file_grep/file_list)",
			"mkdir — create parent directories inside the working tree",
			"publish — serve the site at a tier (public or private)",
			"unpublish — stop serving it",
			"delete — unpublish, remove the working tree, and drop the row",
		},
		"tiers":     []string{sites.PublicSeg, sites.PrivateSeg},
		"serves_at": h.baseURL + "<tier>/<name>/",
		"note":      "Every site carries its front-door URL as \"url\" (returned by create/list/publish/unpublish); it points at the public tier unless the site is published to the private tier.",
	})
}

// ── lifecycle tool implementations ─────────────────────────────────────────

// toolCreate validates the slug (via Store.Create), inserts the row, then creates
// the working tree. The row is inserted first, then the directory; a mkdir
// failure after a successful insert is surfaced (best-effort — the row is left in
// place so a retry/cleanup can resolve it rather than silently swallowing).
func (h *Handler) toolCreate(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	site, err := h.store.Create(ctx, a.Name)
	if err != nil {
		return errResult(err), nil
	}
	if err := os.MkdirAll(h.layout.WorkingDir(a.Name), 0o755); err != nil {
		return errResultMsg("create_working_dir", err.Error()), nil
	}
	return toolResultJSON(h.renderSite(site))
}

// toolList renders every site as structured JSON.
func (h *Handler) toolList(ctx context.Context) (map[string]any, error) {
	all, err := h.store.List(ctx)
	if err != nil {
		return nil, err
	}
	out := make([]map[string]any, 0, len(all))
	for _, s := range all {
		out = append(out, h.renderSite(s))
	}
	return toolResultJSON(map[string]any{"sites": out})
}

// toolDelete runs Unpublish → RemoveAll(working) → Delete(row) in that exact
// order: unpublish first so no dangling served symlink survives, then remove the
// working tree, then drop the row. A not-found at unpublish is tolerated so the
// teardown can still remove the directory and (attempt to) drop the row.
func (h *Handler) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if err := h.store.Unpublish(ctx, a.Name); err != nil && !errors.Is(err, sites.ErrNotFound) {
		return errResult(err), nil
	}
	if err := os.RemoveAll(h.layout.WorkingDir(a.Name)); err != nil {
		return errResultMsg("remove_working_dir", err.Error()), nil
	}
	if err := h.store.Delete(ctx, a.Name); err != nil {
		return errResult(err), nil
	}
	return toolResultJSON(map[string]any{"deleted": a.Name})
}

// toolMkdir creates a directory (and parents) confined to the site's working
// tree. The path is attacker-controlled, so it is confined with confinePath
// (replicated from agentkit/tools/confine.go — the agentkit dependency is added
// in the file-tool phase that needs the full bridge; here a minimal replica
// avoids pulling it in early). The site need not exist in the registry for mkdir
// to confine, but the working root must resolve under SITES_ROOT.
func (h *Handler) toolMkdir(raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
		Path string `json:"path"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	root := h.layout.WorkingDir(a.Name)
	confined, err := confinePath(root, a.Path)
	if err != nil {
		return errResultMsg("path_escapes_working_dir", err.Error()), nil
	}
	if err := os.MkdirAll(confined, 0o755); err != nil {
		return errResultMsg("mkdir", err.Error()), nil
	}
	return toolResultJSON(map[string]any{"created": a.Path, "site": a.Name})
}

// toolPublish delegates to Store.Publish, mapping the domain sentinels to clean
// MCP error results.
func (h *Handler) toolPublish(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
		Tier string `json:"tier"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if err := h.store.Publish(ctx, a.Name, a.Tier); err != nil {
		return errResult(err), nil
	}
	site, err := h.store.Get(ctx, a.Name)
	if err != nil {
		return nil, err
	}
	return toolResultJSON(h.renderSite(site))
}

// toolUnpublish delegates to Store.Unpublish.
func (h *Handler) toolUnpublish(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if err := h.store.Unpublish(ctx, a.Name); err != nil {
		return errResult(err), nil
	}
	site, err := h.store.Get(ctx, a.Name)
	if err != nil {
		return nil, err
	}
	return toolResultJSON(h.renderSite(site))
}

// ── confinement ─────────────────────────────────────────────────────────────

// confinePath resolves p against root and verifies the result stays inside root,
// defending against escapes via absolute paths or '..'. Replicated minimally from
// agentkit/tools/confine.go (which keeps confinePath unexported); the file-tool
// phase will route through agentkit's bridge directly. Rejects absolute p and any
// p whose cleaned join escapes root.
func confinePath(root, p string) (string, error) {
	if filepath.IsAbs(p) {
		return "", errors.New("path must be relative to the working dir: " + p)
	}
	abs := filepath.Clean(filepath.Join(root, p))
	rel, err := filepath.Rel(filepath.Clean(root), abs)
	if err != nil || rel == ".." || strings.HasPrefix(rel, ".."+string(os.PathSeparator)) {
		return "", errors.New("path escapes the working dir: " + p)
	}
	return abs, nil
}

// ── shared helpers ──────────────────────────────────────────────────────────

// unmarshalArgs decodes a tool's arguments, tolerating an absent params block.
func unmarshalArgs(raw json.RawMessage, v any) error {
	if len(raw) == 0 {
		return nil
	}
	return json.Unmarshal(raw, v)
}

// siteURL is the front-door URL a site is (or would be) served at under a tier:
// <baseURL><tier>/<name>/. baseURL already carries the trailing slash.
func (h *Handler) siteURL(tier, name string) string {
	return h.baseURL + tier + "/" + name + "/"
}

// renderSite maps a Site to its JSON projection (nil published_at omitted), always
// including "url" — the front-door URL the site is served at. The tier defaults to
// public unless the site is set to private, so an unpublished site still reports a
// concrete would-be URL rather than leaving an agent to guess the host.
func (h *Handler) renderSite(s sites.Site) map[string]any {
	tier := sites.PublicSeg
	if s.Tier == sites.PrivateSeg {
		tier = sites.PrivateSeg
	}
	m := map[string]any{
		"name":       s.Name,
		"tier":       s.Tier,
		"published":  s.Published,
		"url":        h.siteURL(tier, s.Name),
		"created_at": s.CreatedAt.UTC().Format("2006-01-02T15:04:05.000000000Z07:00"),
		"updated_at": s.UpdatedAt.UTC().Format("2006-01-02T15:04:05.000000000Z07:00"),
	}
	if s.PublishedAt != nil {
		m["published_at"] = s.PublishedAt.UTC().Format("2006-01-02T15:04:05.000000000Z07:00")
	}
	return m
}

// errResult maps a domain error to the corrective MCP error envelope, classifying
// the known sentinels into stable codes so an agent can self-correct.
func errResult(err error) map[string]any {
	code := "error"
	switch {
	case errors.Is(err, sites.ErrInvalidSlug):
		code = "invalid_slug"
	case errors.Is(err, sites.ErrReservedName):
		code = "reserved_name"
	case errors.Is(err, sites.ErrExists):
		code = "already_exists"
	case errors.Is(err, sites.ErrNotFound):
		code = "not_found"
	case errors.Is(err, sites.ErrInvalidTier):
		code = "invalid_tier"
	}
	return errResultMsg(code, err.Error())
}

// errResultMsg renders the {error:{code,message}} corrective envelope as the
// isError tool result.
func errResultMsg(code, msg string) map[string]any {
	b, _ := json.Marshal(map[string]any{"error": map[string]any{"code": code, "message": msg}})
	return toolResultErr(string(b))
}

func toolResultJSON(v any) (map[string]any, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}
	return toolResultText(string(b)), nil
}
