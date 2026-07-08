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
		desc(tool("describe"), "Self-describe the sites service: how to host a static website. The lifecycle is create a site (a slug) → edit its working tree with the file tools → publish it to a tier (public or private) so the front door serves it; unpublish/delete to tear it down. Returns the concept overview and the lifecycle tool list. Takes no inputs.", obj(map[string]any{}), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolDescribe()
		}),
		desc(tool("create"), "Create a new site. 'name' is the slug (1–63 chars, lowercase alphanumeric + hyphen, must start alphanumeric); reserved names are rejected. Inserts the registry row and creates its empty working tree. Returns the created site.", obj(map[string]any{
			"name": descTyp("string", "the site slug (lowercase alnum + hyphen, 1–63 chars)"),
		}, "name"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolCreate(ctx, args)
		}),
		desc(tool("list"), "List every site with its tier, published flag, and timestamps. Takes no inputs.", obj(map[string]any{}), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolList(ctx)
		}),
		desc(tool("delete"), "Delete a site: unpublish it (drop any served link), remove its working tree, then remove the registry row. Idempotent: tolerates an already-removed working tree.", obj(map[string]any{
			"name": descTyp("string", "the site slug to delete"),
		}, "name"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolDelete(ctx, args)
		}),
		desc(tool("mkdir"), "Create a directory (and any missing parents) inside a site's working tree. 'path' is relative to the site's working root and is confined to it (absolute paths and any escape via '..' are rejected). file_write already creates parent dirs, so this is only needed to make an empty directory.", obj(map[string]any{
			"name": descTyp("string", "the site slug whose working tree to create the directory in"),
			"path": descTyp("string", "directory path relative to the site's working root"),
		}, "name", "path"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolMkdir(args)
		}),
		desc(tool("publish"), "Publish a site to a tier so the front door serves it. 'tier' is 'public' or 'private'. Re-publishing to a different tier moves it (never reachable under both at once); re-publishing to the same tier is idempotent.", obj(map[string]any{
			"name": descTyp("string", "the site slug to publish"),
			"tier": descTyp("string", "'public' or 'private'"),
		}, "name", "tier"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolPublish(ctx, args)
		}),
		desc(tool("unpublish"), "Unpublish a site: drop its served link and flip it back to unpublished. Safe to call on an already-unpublished site.", obj(map[string]any{
			"name": descTyp("string", "the site slug to unpublish"),
		}, "name"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolUnpublish(ctx, args)
		}),
		desc(tool("sync"), "Sync a Dropbox-mirrored subtree into a static site's working tree. 'source_path' is the mirror folder to sync from (e.g. \"/sites/marketing\"); 'slug' names the target site and defaults to the source_path basename when that is a valid slug, else it is required. Creates the site if absent, then reconciles its working tree to match the subtree: every upstream file is (over)written and every working file absent upstream is deleted (the subtree owns the tree). Does NOT publish — call publish(tier) once to expose it; an already-published site updates live. Returns {slug, written, deleted}.", obj(map[string]any{
			"source_path": descTyp("string", "the mirror folder path to sync from"),
			"slug":        descTyp("string", "target site slug; defaults to the source_path basename"),
		}, "source_path"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolSync(ctx, args)
		}),
		desc(tool("file_write"), "Write content to file_path inside the site's working tree. Creates parent dirs; overwrites by default, or appends when append:true.", obj(map[string]any{
			"site":      descTyp("string", "site slug whose working dir is the sandbox root"),
			"file_path": descTyp("string", "path relative to the site's working root (confined; absolute and '..' rejected)"),
			"content":   descTyp("string", "the bytes to write"),
			"append":    descTyp("boolean", "append to the file instead of overwriting; creates the file if missing (default false)"),
		}, "site", "file_path", "content"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileWrite(ctx, args)
		}),
		desc(tool("file_read"), "Read a file inside a site's working tree. Optional offset/limit page large files.", obj(map[string]any{
			"site":      descTyp("string", "site slug whose working dir is the sandbox root"),
			"file_path": descTyp("string", "path relative to the site's working root (confined; absolute and '..' rejected)"),
			"offset":    descTyp("number", "1-based line offset to start reading from"),
			"limit":     descTyp("number", "maximum number of lines to return"),
		}, "site", "file_path"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileRead(ctx, args)
		}),
		desc(tool("file_edit"), "Edit a file inside a site's working tree by replacing old_string with new_string.", obj(map[string]any{
			"site":        descTyp("string", "site slug whose working dir is the sandbox root"),
			"file_path":   descTyp("string", "path relative to the site's working root (confined; absolute and '..' rejected)"),
			"old_string":  descTyp("string", "existing text to replace"),
			"new_string":  descTyp("string", "replacement text"),
			"replace_all": descTyp("boolean", "replace every occurrence instead of only the first"),
		}, "site", "file_path", "old_string", "new_string"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileEdit(ctx, args)
		}),
		desc(tool("file_glob"), "Glob for files inside a site's working tree.", obj(map[string]any{
			"site":    descTyp("string", "site slug whose working dir is the sandbox root"),
			"pattern": descTyp("string", "glob pattern to match"),
			"path":    descTyp("string", "optional directory path relative to the site's working root"),
		}, "site", "pattern"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileGlob(ctx, args)
		}),
		desc(tool("file_grep"), "Grep file contents inside a site's working tree.", obj(map[string]any{
			"site":    descTyp("string", "site slug whose working dir is the sandbox root"),
			"pattern": descTyp("string", "regular expression to search for"),
			"path":    descTyp("string", "optional file or directory path relative to the site's working root"),
			"glob":    descTyp("string", "optional filename glob filter"),
		}, "site", "pattern"), func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
			return h.toolFileGrep(ctx, args)
		}),
		desc(tool("file_list"), "List every regular file under the site's working tree with its size and md5, for reconciliation against local files. 'path' optionally scopes the walk; returned paths are relative to the working root.", obj(map[string]any{
			"site": descTyp("string", "site slug whose working dir is the sandbox root"),
			"path": descTyp("string", "optional subdirectory (relative to the working root) to scope the walk"),
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
func (h *toolHandlers) toolCreate(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	site, err := h.store.Create(ctx, a.Name, "")
	if err != nil {
		return errResult(err), nil
	}
	if err := os.MkdirAll(h.layout.WorkingDir(a.Name), 0o755); err != nil {
		return errResultMsg("create_working_dir", err.Error()), nil
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

// toolDelete runs Unpublish → RemoveAll(working) → Delete(row) in that exact
// order: unpublish first so no dangling served symlink survives, then remove the
// working tree, then drop the row. A not-found at unpublish is tolerated so the
// teardown can still remove the directory and (attempt to) drop the row.
func (h *toolHandlers) toolDelete(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(map[string]any{"deleted": a.Name})
}

// toolMkdir creates a directory (and parents) confined to the site's working
// tree. The path is attacker-controlled, so confinement is delegated to
// internal/files.
func (h *toolHandlers) toolMkdir(raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Name string `json:"name"`
		Path string `json:"path"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	root := h.layout.WorkingDir(a.Name)
	if err := sitefiles.Mkdir(root, a.Path); err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("mkdir", err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]any{"created": a.Path, "site": a.Name})
}

// toolPublish delegates to Store.Publish, mapping the domain sentinels to clean
// MCP error results.
func (h *toolHandlers) toolPublish(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(h.renderSite(site))
}

// toolUnpublish delegates to Store.Unpublish.
func (h *toolHandlers) toolUnpublish(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	return appkitmcp.JSONResult(h.renderSite(site))
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
func (h *toolHandlers) siteURL(tier, name string) string {
	return h.baseURL + tier + "/" + name + "/"
}

// renderSite maps a Site to its JSON projection (nil published_at omitted), always
// including "url" — the front-door URL the site is served at. The tier defaults to
// public unless the site is set to private, so an unpublished site still reports a
// concrete would-be URL rather than leaving an agent to guess the host.
func (h *toolHandlers) renderSite(s sites.Site) map[string]any {
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
	return appkitmcp.ErrorResult(string(b))
}
