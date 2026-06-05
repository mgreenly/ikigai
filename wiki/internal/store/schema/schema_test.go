package schema

import (
	"strings"
	"testing"
)

func TestDoc_EmbedsAndIsReachable(t *testing.T) {
	d := Doc()
	if strings.TrimSpace(d) == "" {
		t.Fatal("embedded schema doc is empty")
	}
	// It must document the default type set and the four invariants — the
	// content the ingest agent depends on in its system prompt.
	for _, want := range []string{
		"source", "concept", "entity", "event", "synthesis",
		"Provenance", "Immutable raw", "Flag, don't overwrite", "Append, don't destroy",
		"index.md",
	} {
		if !strings.Contains(d, want) {
			t.Errorf("schema doc missing required content: %q", want)
		}
	}
}
