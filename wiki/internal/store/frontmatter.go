package store

import (
	"fmt"
	"strings"
)

// Frontmatter is the minimal YAML-ish header the store stamps onto a stored raw
// document. It is intentionally a fixed, flat shape — the store hand-rolls the
// (de)serialization rather than pulling in a YAML dependency (no service in the
// suite uses one, and bin/build requires every dep be pure-Go). The values the
// store controls (Sha256, IngestedAt, Collection) plus the caller-supplied
// provenance (Title, Source, Tags) are written between `---` fences ahead of the
// document body, matching the wiki's frontmatter convention (see SCHEMA.md).
type Frontmatter struct {
	Sha256     string   // content hash; the raw-store key
	IngestedAt string   // RFC3339 timestamp, stamped by the store
	Title      string   // caller-supplied human title ("" if none)
	Source     string   // caller-supplied provenance ("" if none)
	Tags       []string // caller-supplied tags (nil/empty if none)
	Collection string   // the collection this raw doc was ingested into
}

// render serializes the frontmatter as a `---`-fenced YAML block followed by a
// blank line. Output is deterministic (fixed key order) so identical inputs
// produce byte-identical output — important for the immutability invariant. The
// type field is fixed to "source": a stored raw doc is the provenance anchor for
// exactly one source page.
func (f Frontmatter) render() string {
	var b strings.Builder
	b.WriteString("---\n")
	b.WriteString("type: source\n")
	b.WriteString(yamlKV("sha256", f.Sha256))
	b.WriteString(yamlKV("ingested_at", f.IngestedAt))
	b.WriteString(yamlKV("title", f.Title))
	b.WriteString(yamlKV("source", f.Source))
	b.WriteString(yamlList("tags", f.Tags))
	b.WriteString(yamlKV("collection", f.Collection))
	b.WriteString("---\n\n")
	return b.String()
}

// yamlKV renders one scalar `key: value` line, quoting the value so embedded
// special characters (`:`, `#`, leading/trailing space, etc.) cannot corrupt
// the block. An empty value renders as an empty quoted string.
func yamlKV(key, value string) string {
	return fmt.Sprintf("%s: %s\n", key, yamlQuote(value))
}

// yamlList renders a key with a flow-style list value (`key: [a, b]`). An empty
// list renders as `key: []`.
func yamlList(key string, items []string) string {
	if len(items) == 0 {
		return fmt.Sprintf("%s: []\n", key)
	}
	quoted := make([]string, len(items))
	for i, it := range items {
		quoted[i] = yamlQuote(it)
	}
	return fmt.Sprintf("%s: [%s]\n", key, strings.Join(quoted, ", "))
}

// yamlQuote double-quotes a scalar and escapes backslashes and quotes so the
// emitted line is always a valid single YAML double-quoted scalar.
func yamlQuote(s string) string {
	s = strings.ReplaceAll(s, `\`, `\\`)
	s = strings.ReplaceAll(s, `"`, `\"`)
	s = strings.ReplaceAll(s, "\n", `\n`)
	return `"` + s + `"`
}
