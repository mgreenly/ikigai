package search

import (
	"context"
	"fmt"
	"strings"
)

// PageEntry mirrors store.PageEntry's RelPath field — the part this package
// needs from a walked page tree. (Declared locally so internal/search does not
// import internal/store; *store.Store's WalkPages return value satisfies the
// PageWalker interface structurally via the adapter below.)
type PageEntry struct {
	RelPath string
}

// PageSource is the subset of *store.Store the (re)indexer consumes: enumerate a
// collection's curated pages and read each page's bytes. *store.Store satisfies
// this directly (WalkPages returns []store.PageEntry, ReadPage returns []byte),
// so callers pass the store with a thin adapter — see ReindexCollection's doc.
type PageSource interface {
	// WalkPages returns the collection's curated page set as collection-relative
	// paths (sources/concepts/entities/events/synthesis/*.md + index.md).
	WalkPages(owner, collection string) ([]PageEntry, error)
	// ReadPage returns the full bytes of the page at the collection-relative path.
	ReadPage(owner, collection, relPath string) ([]byte, error)
}

// ReindexCollection (re)indexes a whole collection by walking src's curated page
// tree and upserting every page into idx. It is the integration hook the Phase-4
// ingest core calls after a successful integration pass; because IndexPages
// upserts by (owner, collection, path), re-ingest never duplicates rows.
//
// src is the page provider; in production it is a *store.Store wrapped to satisfy
// PageSource. ReadPage's bytes are indexed verbatim as the whole-page body; the
// title is derived (frontmatter title, first H1, else the path).
func ReindexCollection(ctx context.Context, idx Index, src PageSource, owner, collection string) error {
	entries, err := src.WalkPages(owner, collection)
	if err != nil {
		return fmt.Errorf("search: walk pages: %w", err)
	}
	pages := make([]Page, 0, len(entries))
	for _, e := range entries {
		body, err := src.ReadPage(owner, collection, e.RelPath)
		if err != nil {
			return fmt.Errorf("search: read page %q: %w", e.RelPath, err)
		}
		pages = append(pages, Page{
			Path:  e.RelPath,
			Title: DeriveTitle(string(body), e.RelPath),
			Body:  string(body),
		})
	}
	if err := idx.IndexPages(ctx, owner, collection, pages); err != nil {
		return err
	}
	return nil
}

// DeriveTitle picks a human title for a page: the frontmatter `title:` value if
// present, else the first markdown H1 (`# ...`), else the relative path. Exported
// so the Phase-4 ingest wiring and Task-5.1 search verb can reuse the same rule.
func DeriveTitle(body, relPath string) string {
	if t := frontmatterTitle(body); t != "" {
		return t
	}
	if t := firstH1(body); t != "" {
		return t
	}
	return relPath
}

// frontmatterTitle extracts `title:` from a leading `---`-fenced YAML block.
func frontmatterTitle(body string) string {
	lines := strings.Split(body, "\n")
	if len(lines) == 0 || strings.TrimSpace(lines[0]) != "---" {
		return ""
	}
	for _, ln := range lines[1:] {
		if strings.TrimSpace(ln) == "---" {
			break
		}
		if strings.HasPrefix(ln, "title:") {
			v := strings.TrimSpace(strings.TrimPrefix(ln, "title:"))
			return strings.Trim(v, `"'`)
		}
	}
	return ""
}

// firstH1 returns the text of the first markdown H1 (`# ...`), skipping a leading
// frontmatter block.
func firstH1(body string) string {
	lines := strings.Split(body, "\n")
	i := 0
	if len(lines) > 0 && strings.TrimSpace(lines[0]) == "---" {
		i = 1
		for ; i < len(lines); i++ {
			if strings.TrimSpace(lines[i]) == "---" {
				i++
				break
			}
		}
	}
	for ; i < len(lines); i++ {
		ln := strings.TrimSpace(lines[i])
		if strings.HasPrefix(ln, "# ") {
			return strings.TrimSpace(strings.TrimPrefix(ln, "# "))
		}
	}
	return ""
}
