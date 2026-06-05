// Package schema embeds the wiki's agent-facing schema doc (SCHEMA.md) so it is
// compiled into the binary and retrievable as a string. The Phase-4 ingest agent
// loads it as part of its system prompt: it is the wiki's CLAUDE.md/AGENTS.md
// equivalent — the type set, frontmatter conventions, index-first navigation,
// and the four invariants the agent must obey.
package schema

import _ "embed"

//go:embed SCHEMA.md
var doc string

// Doc returns the embedded schema document. The ingest agent uses this as (part
// of) its system prompt; callers must not mutate the wiki's conventions at
// runtime — the doc is the single source of truth, co-evolved with this package.
func Doc() string { return doc }
