package mcp

// sync.go wires the `sync` verb: it ties together the pieces Phase 6 built —
// the dropbox loopback MirrorClient (enumerate /list + fetch /content) and the
// pure sites.Reconcile routine — behind the MCP surface. sync adopts a Dropbox
// mirror subtree as a site's current public/private directory: create-or-reuse
// the site row + tree, enumerate the subtree, overwrite every upstream file and
// delete every site file absent upstream (the subtree owns the tree). It leaves
// visibility unchanged.

import (
	"context"
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"path"
	"path/filepath"
	"strings"

	appkitmcp "appkit/mcp"

	"sites/internal/sites"
)

// toolSync reconciles a dropbox mirror subtree into a site's current directory. It
// requires source_path; slug defaults to the source_path basename (and must be
// a valid slug, else the caller must pass slug explicitly — ADR "Slug
// derivation"). The flow: validate → create-or-reuse the row + tree (row first,
// then dir, matching toolCreate) → stamp source_path → enumerate upstream →
// fetch each file's bytes keyed by its path relative to source_path → walk the
// current site directory for the existing path set → Reconcile.
func (h *toolHandlers) toolSync(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		SourcePath string `json:"source_path"`
		Slug       string `json:"slug"`
	}
	if err := unmarshalArgs(raw, &a); err != nil {
		return nil, err
	}
	if h.mirror == nil {
		return errResultMsg("sync_unconfigured", "sync is not wired: no dropbox mirror client (DROPBOX_BASE_URL)"), nil
	}
	if a.SourcePath == "" {
		return errResultMsg("validation", "missing required \"source_path\" argument"), nil
	}

	// Derive the slug from the source-path basename when none was given, then
	// pre-check it against the slug grammar. Store.Create enforces the same
	// grammar, but a pre-check turns a bad auto-derived slug into a clean
	// validation error that tells the caller to pass slug explicitly rather than
	// surfacing a raw create failure.
	slug := a.Slug
	derived := false
	if slug == "" {
		slug = path.Base(a.SourcePath)
		derived = true
	}
	if err := sites.ValidateSlug(slug); err != nil {
		if derived {
			return errResultMsg("validation",
				"derived slug "+jsonQuote(slug)+" from source_path is not a valid slug; pass \"slug\" explicitly"), nil
		}
		return errResult(err), nil
	}

	// Create-or-reuse: if the site is absent, create the row then its private dir
	// (row-then-dir order, matching toolCreate). A present site is reused as-is.
	site, err := h.store.Get(ctx, slug)
	if err != nil {
		if !errors.Is(err, sites.ErrNotFound) {
			return errResult(err), nil
		}
		created, cerr := h.store.Create(ctx, slug, "")
		if cerr != nil {
			return errResult(cerr), nil
		}
		site = created
		if mderr := os.MkdirAll(h.layout.SiteDir(false, slug), 0o755); mderr != nil {
			return errResultMsg("create_site_dir", mderr.Error()), nil
		}
	}
	// Stamp the originating subtree on the row (marks the site import-managed and
	// records provenance — ADR Decision 2). Done after create-or-reuse so it
	// applies to a reused site too (the subtree may have moved).
	if err := h.store.SetSourcePath(ctx, slug, a.SourcePath); err != nil {
		return errResult(err), nil
	}

	// Enumerate the subtree upstream and fetch each file's current bytes, keyed by
	// its path RELATIVE to source_path (that relative key is the in-working-tree
	// path Reconcile writes to). The client follows /list's cursor to completion.
	files, err := h.mirror.List(ctx, a.SourcePath)
	if err != nil {
		return errResultMsg("list_upstream", err.Error()), nil
	}
	desired := make(map[string][]byte, len(files))
	for _, f := range files {
		rel := relUnder(a.SourcePath, f.Path)
		data, ferr := h.mirror.Fetch(ctx, f.Path)
		if ferr != nil {
			return errResultMsg("fetch_upstream", ferr.Error()), nil
		}
		desired[rel] = data
	}

	// Walk the site directory for its current relative file set (the path set is all
	// Reconcile needs for delete-absent — overwrite-all means no md5 compare; same
	// WalkDir + Rel/ToSlash pattern as toolFileList).
	workingDir := h.layout.SiteDir(site.Public, slug)
	var existingRel []string
	walkErr := filepath.WalkDir(workingDir, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() || !d.Type().IsRegular() {
			return nil
		}
		rel, rerr := filepath.Rel(workingDir, p)
		if rerr != nil {
			return rerr
		}
		existingRel = append(existingRel, filepath.ToSlash(rel))
		return nil
	})
	if walkErr != nil && !errors.Is(walkErr, fs.ErrNotExist) {
		return errResultMsg("walk_working", walkErr.Error()), nil
	}

	written, deleted, rerr := sites.Reconcile(workingDir, desired, existingRel)
	if rerr != nil {
		return errResultMsg("reconcile", rerr.Error()), nil
	}

	return appkitmcp.JSONResult(map[string]any{"slug": slug, "written": written, "deleted": deleted})
}

// relUnder returns child's path relative to prefix (both mirror paths, slash-
// separated). When child sits directly under prefix the result is the suffix
// with leading slashes trimmed; when prefix is a non-prefix (defensive) the
// child's basename is used so nothing escapes the working tree.
func relUnder(prefix, child string) string {
	p := strings.TrimRight(prefix, "/")
	if p == "" {
		return strings.TrimLeft(child, "/")
	}
	if strings.HasPrefix(child, p+"/") {
		return strings.TrimPrefix(child, p+"/")
	}
	// child is not under prefix (defensive — the upstream list is scoped to
	// prefix): fall back to the basename so nothing escapes the working tree.
	return path.Base(child)
}

// jsonQuote renders s as a JSON string literal (with surrounding quotes) for
// embedding the offending slug in a validation message safely.
func jsonQuote(s string) string {
	b, _ := json.Marshal(s)
	return string(b)
}
