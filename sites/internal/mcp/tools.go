package mcp

import (
	"context"
	"encoding/json"
	"errors"
	"os"

	appkitmcp "appkit/mcp"
	"appkit/server"

	sitefiles "sites/internal/files"
	"sites/internal/sites"
)

// toolPrefix brands every MCP tool name (DECISIONS §1). It is the suite name
// ikigenba + the service name; HTTP route paths are NOT branded.
const toolPrefix = ""

// tool returns the branded, fully-qualified MCP tool name. Used by BOTH
// toolDescriptors and dispatchTool so the two sites cannot drift.
func tool(verb string) string { return toolPrefix + verb }

type toolHandlers struct {
	store   *sites.Store
	layout  sites.Layout
	baseURL string
	mirror  sites.MirrorClient
}

// Tools returns sites's service-owned MCP tool declarations. The shared appkit
// MCP transport prepends the chassis health and reflection tools.
func Tools(store *sites.Store, layout sites.Layout, baseURL string, mirror sites.MirrorClient) []appkitmcp.Tool {
	if store == nil {
		panic("mcp: sites store is required")
	}
	h := &toolHandlers{store: store, layout: layout, baseURL: baseURL, mirror: mirror}
	return []appkitmcp.Tool{
		desc(tool("describe"), "Self-describe the sites service: how to host a static website. The lifecycle is create a private site (a slug), edit its files, set visibility public/private, and delete it. Returns the concept overview and the lifecycle tool list. Takes no inputs.", obj(map[string]any{}), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolDescribe()
		}),
		desc(tool("create"), "Create a new private site owned by the authenticated caller. 'name' is the slug (1-63 chars, lowercase alphanumeric + hyphen, must start alphanumeric); reserved names are rejected. Inserts the registry row and creates its empty private directory. Returns the created site.", obj(map[string]any{
			"name": descTyp("string", "the site slug (lowercase alnum + hyphen, 1-63 chars)"),
		}, "name"), func(ctx context.Context, args json.RawMessage, id server.Identity) (map[string]any, error) {
			return h.toolCreate(ctx, args, id)
		}),
		desc(tool("list"), "List every site with its public/private visibility, creator, URL, and timestamps. Takes no inputs.", obj(map[string]any{}), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolList(ctx)
		}),
		desc(tool("delete"), "Delete a site: remove its registry row and its current public/private directory. Idempotent: tolerates an already-removed directory or row.", obj(map[string]any{
			"name": descTyp("string", "the site slug to delete"),
		}, "name"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolDelete(ctx, args)
		}),
		desc(tool("mkdir"), "Create a directory (and any missing parents) inside a site's current public/private directory. 'path' is relative to that site root and is confined to it (absolute paths and any escape via '..' are rejected). file_write already creates parent dirs, so this is only needed to make an empty directory.", obj(map[string]any{
			"name": descTyp("string", "the site slug whose directory to create the directory in"),
			"path": descTyp("string", "directory path relative to the site's current root"),
		}, "name", "path"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolMkdir(ctx, args)
		}),
		desc(tool("set_visibility"), "Set a site's visibility. public:true moves it to the public tree; public:false moves it to the private tree. Returns the site with its updated URL.", obj(map[string]any{
			"name":   descTyp("string", "the site slug"),
			"public": descTyp("boolean", "true for public, false for private"),
		}, "name", "public"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolSetVisibility(ctx, args)
		}),
		desc(tool("sync"), "Sync a Dropbox-mirrored subtree into a static site's current public/private directory. 'source_path' is the mirror folder to sync from (e.g. \"/sites/marketing\"); 'slug' names the target site and defaults to the source_path basename when that is a valid slug, else it is required. Creates the site if absent as private, then reconciles its current site directory to match the subtree: every upstream file is (over)written and every site file absent upstream is deleted. Visibility is unchanged. Returns {slug, written, deleted}.", obj(map[string]any{
			"source_path": descTyp("string", "the mirror folder path to sync from"),
			"slug":        descTyp("string", "target site slug; defaults to the source_path basename"),
		}, "source_path"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolSync(ctx, args)
		}),
		desc(tool("file_write"), "Write content to file_path inside the site's current public/private directory. Creates parent dirs; overwrites by default, or appends when append:true.", obj(map[string]any{
			"site":      descTyp("string", "site slug whose current directory is the sandbox root"),
			"file_path": descTyp("string", "path relative to the site's current root (confined; absolute and '..' rejected)"),
			"content":   descTyp("string", "the bytes to write"),
			"append":    descTyp("boolean", "append to the file instead of overwriting; creates the file if missing (default false)"),
		}, "site", "file_path", "content"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileWrite(ctx, args)
		}),
		desc(tool("file_read"), "Read a file inside a site's current public/private directory. Optional offset/limit page large files.", obj(map[string]any{
			"site":      descTyp("string", "site slug whose current directory is the sandbox root"),
			"file_path": descTyp("string", "path relative to the site's current root (confined; absolute and '..' rejected)"),
			"offset":    descTyp("number", "1-based line offset to start reading from"),
			"limit":     descTyp("number", "maximum number of lines to return"),
		}, "site", "file_path"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileRead(ctx, args)
		}),
		desc(tool("file_edit"), "Edit a file inside a site's current public/private directory by replacing old_string with new_string.", obj(map[string]any{
			"site":        descTyp("string", "site slug whose current directory is the sandbox root"),
			"file_path":   descTyp("string", "path relative to the site's current root (confined; absolute and '..' rejected)"),
			"old_string":  descTyp("string", "existing text to replace"),
			"new_string":  descTyp("string", "replacement text"),
			"replace_all": descTyp("boolean", "replace every occurrence instead of only the first"),
		}, "site", "file_path", "old_string", "new_string"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileEdit(ctx, args)
		}),
		desc(tool("file_glob"), "Glob for files inside a site's current public/private directory.", obj(map[string]any{
			"site":    descTyp("string", "site slug whose current directory is the sandbox root"),
			"pattern": descTyp("string", "glob pattern to match"),
			"path":    descTyp("string", "optional directory path relative to the site's current root"),
		}, "site", "pattern"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileGlob(ctx, args)
		}),
		desc(tool("file_grep"), "Grep file contents inside a site's current public/private directory.", obj(map[string]any{
			"site":    descTyp("string", "site slug whose current directory is the sandbox root"),
			"pattern": descTyp("string", "regular expression to search for"),
			"path":    descTyp("string", "optional file or directory path relative to the site's current root"),
			"glob":    descTyp("string", "optional filename glob filter"),
		}, "site", "pattern"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileGrep(ctx, args)
		}),
		desc(tool("file_list"), "List every regular file under the site's current public/private directory with its size and md5, for reconciliation against local files. 'path' optionally scopes the walk; returned paths are relative to the site root.", obj(map[string]any{
			"site": descTyp("string", "site slug whose current directory is the sandbox root"),
			"path": descTyp("string", "optional subdirectory (relative to the current root) to scope the walk"),
		}, "site"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileList(ctx, args)
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

func descTyp(t, description string) map[string]any {
	return map[string]any{"type": t, "description": description}
}

// toolDescribe is the self-describing tool: it explains what sites does and the
// lifecycle of hosting a static website, so an agent connecting for the first
// time can orient without out-of-band docs.
func (h *toolHandlers) toolDescribe() (map[string]any, error) {
	return appkitmcp.JSONResult(map[string]any{
		"service": "sites",
		"summary": "Host static websites. Each site is a slug with a public/private visibility flag, creator provenance, and files under its current visibility directory.",
		"lifecycle": []string{
			"create - register a private slug for the authenticated creator",
			"edit the current site directory with the file tools (file_read/file_write/file_edit/file_glob/file_grep/file_list)",
			"mkdir - create parent directories inside the current site directory",
			"set_visibility - move the site between private and public",
			"delete - remove the current site directory and drop the row",
		},
		"visibility": []string{sites.PrivateSeg, sites.PublicSeg},
		"serves_at":  h.baseURL + "<public|private>/<name>/",
		"note":       "Every site carries its front-door URL as \"url\" (returned by create/list/set_visibility); it follows the site's public/private visibility.",
	})
}

// toolCreate validates the slug (via Store.Create), inserts the row, then
// creates the private site directory.
func (h *toolHandlers) toolCreate(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	site, err := h.store.Create(ctx, a.Name, id.OwnerEmail)
	if err != nil {
		return errResult(err), nil
	}
	if err := os.MkdirAll(h.layout.SiteDir(false, a.Name), 0o755); err != nil {
		return errResultMsg("create_site_dir", err.Error()), nil
	}
	return appkitmcp.JSONResult(h.renderSite(site))
}

// toolList renders every site as structured JSON.
func (h *toolHandlers) toolList(ctx context.Context) (map[string]any, error) {
	all, err := h.store.List(ctx)
	if err != nil {
		return nil, err
	}
	out := make([]map[string]any, 0, len(all))
	for _, s := range all {
		out = append(out, h.renderSite(s))
	}
	return appkitmcp.JSONResult(map[string]any{"sites": out})
}

// toolDelete removes the row and current visibility directory. A missing row or
// directory is a successful idempotent delete at the MCP surface.
func (h *toolHandlers) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	site, err := h.store.Get(ctx, a.Name)
	if err != nil {
		if errors.Is(err, sites.ErrNotFound) {
			return appkitmcp.JSONResult(map[string]any{"deleted": a.Name})
		}
		return errResult(err), nil
	}
	if err := h.store.Delete(ctx, a.Name); err != nil && !errors.Is(err, sites.ErrNotFound) {
		return errResult(err), nil
	}
	if err := os.RemoveAll(h.layout.SiteDir(site.Public, a.Name)); err != nil {
		return errResultMsg("remove_site_dir", err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]any{"deleted": a.Name})
}

// toolMkdir creates a directory (and parents) confined to the current site
// directory. The path is attacker-controlled, so confinement is delegated to
// internal/files.
func (h *toolHandlers) toolMkdir(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
		Path string `json:"path"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	site, err := h.store.Get(ctx, a.Name)
	if err != nil {
		return errResult(err), nil
	}
	root := h.layout.SiteDir(site.Public, a.Name)
	if err := sitefiles.Mkdir(root, a.Path); err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("mkdir", err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]any{"created": a.Path, "site": a.Name})
}

// toolSetVisibility flips the row's public flag and moves the site directory to
// the matching public/private parent.
func (h *toolHandlers) toolSetVisibility(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name   string `json:"name"`
		Public bool   `json:"public"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if err := h.store.SetVisibility(ctx, a.Name, a.Public); err != nil {
		return errResult(err), nil
	}
	if err := h.layout.Move(a.Name, a.Public); err != nil {
		return errResultMsg("move_site_dir", err.Error()), nil
	}
	site, err := h.store.Get(ctx, a.Name)
	if err != nil {
		return nil, err
	}
	return appkitmcp.JSONResult(h.renderSite(site))
}

// unmarshalArgs decodes a tool's arguments, tolerating an absent params block.
func unmarshalArgs(raw json.RawMessage, v any) error {
	if len(raw) == 0 {
		return nil
	}
	return json.Unmarshal(raw, v)
}

// siteURL is the front-door URL for a site under a visibility segment:
// <baseURL><public|private>/<name>/. baseURL already carries the trailing slash.
func (h *toolHandlers) siteURL(tier, name string) string {
	return h.baseURL + tier + "/" + name + "/"
}

// renderSite maps a Site to its MCP JSON projection.
func (h *toolHandlers) renderSite(s sites.Site) map[string]any {
	tier := sites.PublicSeg
	if !s.Public {
		tier = sites.PrivateSeg
	}
	return map[string]any{
		"name":       s.Name,
		"public":     s.Public,
		"created_by": s.CreatedBy,
		"url":        h.siteURL(tier, s.Name),
		"created_at": s.CreatedAt.UTC().Format("2006-01-02T15:04:05.000000000Z07:00"),
		"updated_at": s.UpdatedAt.UTC().Format("2006-01-02T15:04:05.000000000Z07:00"),
	}
}

// errResult maps a domain error to the corrective MCP error envelope,
// classifying the known sentinels into stable codes so an agent can
// self-correct.
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
	}
	return errResultMsg(code, err.Error())
}

// errResultMsg renders the {error:{code,message}} corrective envelope as the
// isError tool result.
func errResultMsg(code, msg string) map[string]any {
	b, _ := json.Marshal(map[string]any{"error": map[string]any{"code": code, "message": msg}})
	return appkitmcp.ErrorResult(string(b))
}
