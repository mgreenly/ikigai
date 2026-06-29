package main

import (
	"errors"
	"os"
	"os/exec"
	"strings"
	"testing"
)

func TestMain(m *testing.M) {
	if os.Getenv("OPSCTL_TEST_MAIN") == "1" {
		main()
		return
	}
	os.Exit(m.Run())
}

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
			if form, ok := synopsisOf(v.name); !ok || form != v.synopsis {
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

// TestUsageWidthCapped asserts every rendered line of the grouped --help is
// ≤100 columns (runes), so the help stays readable in a standard terminal.
func TestUsageWidthCapped(t *testing.T) {
	for _, line := range strings.Split(usage(), "\n") {
		if n := len([]rune(line)); n > 100 {
			t.Errorf("usage() line exceeds 100 cols (%d): %q", n, line)
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

func TestInvalidVersionInputsExitNonZero(t *testing.T) {
	// R-439X-OQXO
	for _, bad := range []string{"0.7.1", "v1", "v1.2", "not-semver"} {
		for _, tc := range []struct {
			name string
			args []string
		}{
			{name: "stage", args: []string{"stage", "ledger", bad, "--artifact", "artifact"}},
			{name: "deploy", args: []string{"deploy", "ledger", bad}},
			{name: "rollback", args: []string{"rollback", "ledger", bad}},
		} {
			t.Run(tc.name+"/"+bad, func(t *testing.T) {
				cmd := exec.Command(os.Args[0], tc.args...)
				cmd.Env = append(os.Environ(), "OPSCTL_TEST_MAIN=1", "OPSCTL_ROOT="+t.TempDir())
				out, err := cmd.CombinedOutput()
				if err == nil {
					t.Fatalf("opsctl %s exited 0, want non-zero; output:\n%s", strings.Join(tc.args, " "), out)
				}
				var exitErr *exec.ExitError
				if !errors.As(err, &exitErr) {
					t.Fatalf("opsctl %s failed without an exit status: %v; output:\n%s", strings.Join(tc.args, " "), err, out)
				}
				if exitErr.ExitCode() == 0 {
					t.Fatalf("opsctl %s exit code = 0, want non-zero; output:\n%s", strings.Join(tc.args, " "), out)
				}
				got := string(out)
				if !strings.Contains(got, "invalid") || !strings.Contains(got, bad) {
					t.Fatalf("opsctl %s output = %q, want invalid-version refusal naming %q", strings.Join(tc.args, " "), got, bad)
				}
			})
		}
	}
}
