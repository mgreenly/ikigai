package mcp

import (
	"context"
	"encoding/json"
	"errors"

	appkitmcp "appkit/mcp"

	sitefiles "sites/internal/files"
)

// toolFileList walks a site's current public/private directory and returns every
// regular file with its path (relative to the site root), size, and md5. An
// optional "path" scopes the walk to a subdirectory, confined to the site root; a missing
// scope dir yields an empty list rather than an error.
func (h *toolHandlers) toolFileList(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	root, env := h.siteRoot(ctx, a.Site)
	if env != nil {
		return env, nil
	}

	scope := root
	if a.Path != "" {
		s, err := sitefiles.ConfinePath(root, a.Path)
		if err != nil {
			if errors.Is(err, sitefiles.ErrEscapes) {
				return errResultMsg("path_escapes_working_dir", err.Error()), nil
			}
			return errResultMsg("walk", err.Error()), nil
		}
		scope = s
	}

	listed, err := sitefiles.List(root, scope)
	if err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("walk", err.Error()), nil
	}
	files := make([]map[string]any, 0, len(listed))
	for _, f := range listed {
		files = append(files, map[string]any{"path": f.Path, "size": f.Size, "md5": f.Md5})
	}

	return appkitmcp.JSONResult(map[string]any{"site": a.Site, "files": files})
}

// toolFileWrite writes content to a confined path in the site's current
// public/private directory, truncating by default or appending when append:true.
func (h *toolHandlers) toolFileWrite(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
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
	root, env := h.siteRoot(ctx, a.Site)
	if env != nil {
		return env, nil
	}

	if err := sitefiles.Write(root, a.FilePath, a.Content, a.Append); err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("write", err.Error()), nil
	}

	return appkitmcp.JSONResult(map[string]any{"written": a.FilePath, "site": a.Site, "appended": a.Append})
}

func (h *toolHandlers) toolFileRead(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Site     string `json:"site"`
		FilePath string `json:"file_path"`
		Offset   int    `json:"offset"`
		Limit    int    `json:"limit"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.Site == "" {
		return errResultMsg("invalid_site", "missing required \"site\" argument"), nil
	}
	root, env := h.siteRoot(ctx, a.Site)
	if env != nil {
		return env, nil
	}
	content, err := sitefiles.Read(root, a.FilePath, a.Offset, a.Limit)
	if err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("read", err.Error()), nil
	}
	return appkitmcp.TextResult(content), nil
}

func (h *toolHandlers) toolFileEdit(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Site       string `json:"site"`
		FilePath   string `json:"file_path"`
		OldString  string `json:"old_string"`
		NewString  string `json:"new_string"`
		ReplaceAll bool   `json:"replace_all"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.Site == "" {
		return errResultMsg("invalid_site", "missing required \"site\" argument"), nil
	}
	root, env := h.siteRoot(ctx, a.Site)
	if env != nil {
		return env, nil
	}
	replaced, err := sitefiles.Edit(root, a.FilePath, a.OldString, a.NewString, a.ReplaceAll)
	if err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("edit", err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]any{"edited": a.FilePath, "site": a.Site, "replaced": replaced})
}

func (h *toolHandlers) toolFileGlob(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Site    string `json:"site"`
		Pattern string `json:"pattern"`
		Path    string `json:"path"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.Site == "" {
		return errResultMsg("invalid_site", "missing required \"site\" argument"), nil
	}
	root, env := h.siteRoot(ctx, a.Site)
	if env != nil {
		return env, nil
	}
	matches, err := sitefiles.Glob(root, a.Pattern, a.Path)
	if err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("glob", err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]any{"site": a.Site, "matches": matches})
}

func (h *toolHandlers) toolFileGrep(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Site    string `json:"site"`
		Pattern string `json:"pattern"`
		Path    string `json:"path"`
		Glob    string `json:"glob"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if a.Site == "" {
		return errResultMsg("invalid_site", "missing required \"site\" argument"), nil
	}
	root, env := h.siteRoot(ctx, a.Site)
	if env != nil {
		return env, nil
	}
	matches, err := sitefiles.Grep(root, a.Pattern, a.Path, a.Glob)
	if err != nil {
		if errors.Is(err, sitefiles.ErrEscapes) {
			return errResultMsg("path_escapes_working_dir", err.Error()), nil
		}
		return errResultMsg("grep", err.Error()), nil
	}
	out := make([]map[string]any, 0, len(matches))
	for _, m := range matches {
		out = append(out, map[string]any{"path": m.Path, "line": m.Line, "text": m.Text})
	}
	return appkitmcp.JSONResult(map[string]any{"site": a.Site, "matches": out})
}

func (h *toolHandlers) siteRoot(ctx context.Context, slug string) (string, map[string]any) {
	site, err := h.store.Get(ctx, slug)
	if err != nil {
		return "", errResult(err)
	}
	return h.layout.SiteDir(site.Public, slug), nil
}
