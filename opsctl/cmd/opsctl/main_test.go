package main

import (
	"strings"
	"testing"
)

// TestHelpCoversDispatchTable is the core coverage guard: the dispatch table
// (`runners`, what `opsctl <verb>` can actually run) and the help registry
// (`groups`, what --help documents) MUST share an identical key set. This stops
// help from silently drifting from dispatch in either direction — a verb wired
// into `runners` but not documented, or documented but not dispatchable.
func TestHelpCoversDispatchTable(t *testing.T) {
	documented := map[string]bool{}
	for _, g := range groups {
		for _, v := range g.verbs {
			documented[v.name] = true
		}
	}
	// Every dispatchable verb is documented...
	for name := range runners {
		if !documented[name] {
			t.Errorf("verb %q is in the dispatch table but absent from --help (groups)", name)
		}
	}
	// ...and every documented verb is dispatchable.
	for name := range documented {
		if _, ok := runners[name]; !ok {
			t.Errorf("verb %q is documented in --help but has no runner in the dispatch table", name)
		}
	}
}

// TestUsageNamesEveryDispatchableVerb confirms the rendered usage text actually
// names each dispatchable verb (not just that the registries agree).
func TestUsageNamesEveryDispatchableVerb(t *testing.T) {
	help := usage()
	for name := range runners {
		needle := "opsctl " + name
		if !strings.Contains(help, needle) {
			t.Errorf("usage() does not name verb %q (missing %q)", name, needle)
		}
	}
}

// TestEveryVerbHasSynopsis ensures each documented verb carries a well-formed
// synopsis (printed by `opsctl <verb> --help`).
func TestEveryVerbHasSynopsis(t *testing.T) {
	for _, g := range groups {
		if g.title == "" {
			t.Errorf("group with empty title")
		}
		for _, v := range g.verbs {
			if v.name == "" {
				t.Errorf("verb with empty name in group %q", g.title)
			}
			if strings.TrimSpace(v.synopsis) == "" {
				t.Errorf("verb %q has no synopsis", v.name)
			}
			if !strings.HasPrefix(v.synopsis, "opsctl "+v.name) {
				t.Errorf("verb %q synopsis does not start with 'opsctl %s': %q", v.name, v.name, v.synopsis)
			}
			if s, ok := synopsisOf(v.name); !ok || s != v.synopsis {
				t.Errorf("synopsisOf(%q) did not round-trip", v.name)
			}
		}
	}
}

// TestUsageHasAllGroupTitles confirms the five planned --help sections are
// present, so the grouping structure can't silently collapse.
func TestUsageHasAllGroupTitles(t *testing.T) {
	help := usage()
	for _, want := range []string{
		"Deploy lifecycle", "Inspect", "Service control", "Provisioning",
	} {
		if !strings.Contains(help, want) {
			t.Errorf("usage() missing group heading %q", want)
		}
	}
	for _, want := range []string{"OPSCTL_ROOT", "OPSCTL_SYSROOT"} {
		if !strings.Contains(help, want) {
			t.Errorf("usage() missing env var %q", want)
		}
	}
}

// TestNoDuplicateVerbNames keeps the registry unambiguous (verbByName would
// otherwise silently shadow).
func TestNoDuplicateVerbNames(t *testing.T) {
	seen := map[string]bool{}
	for _, g := range groups {
		for _, v := range g.verbs {
			if seen[v.name] {
				t.Errorf("duplicate verb name %q", v.name)
			}
			seen[v.name] = true
		}
	}
}
