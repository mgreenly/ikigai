package read

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"wiki/internal/config"
)

// TestAskPromptDefaultGate is the standing, offline prompt-default gate for the
// ask call site (see "Prompt-default validation" in docs/wiki-redesign-plan.md):
// with no key or network it asserts (a-i) the ask config-default prompt is
// non-placeholder — present, above a length floor, carrying its SIX §9.2 sections
// — and (a-ii) the answer parser schema-validates a committed, hand-authored,
// schema-faithful response fixture into the page-level citation contract.
// Deterministic, key-independent.
func TestAskPromptDefaultGate(t *testing.T) {
	p := config.DefaultAskPrompt

	if len(strings.TrimSpace(p)) < 600 {
		t.Fatalf("ask default prompt too short to be real (%d chars)", len(p))
	}
	if strings.Contains(p, "PLACEHOLDER") {
		t.Fatal("ask default prompt is still a placeholder")
	}

	// The six §9.2 sections.
	sections := []string{
		"## 1. Task framing",
		"## 2. The corpus model",
		"## 3. Procedure",
		"## 4. Answer craft",
		"## 5. Budget discipline",
		"## 6. Output schema and example",
	}
	for _, s := range sections {
		if !strings.Contains(p, s) {
			t.Errorf("ask default prompt missing section %q", s)
		}
	}

	// The load-bearing answer-contract invariants the design pins for ask.
	lp := strings.ToLower(p)
	for _, want := range []string{
		"the wiki has nothing", // abstention over fabrication (§1)
		"lookup",               // names → lookup first (§3 procedure)
		"read_source",          // only when pages disagree / exact wording (§3)
		"citation",             // page-level citation contract (§4)
	} {
		if !strings.Contains(lp, want) {
			t.Errorf("ask default prompt missing the %q obligation", want)
		}
	}

	// (a-ii) the parser + schema validate a committed, schema-faithful fixture into
	// the page-level citation contract.
	raw, err := os.ReadFile(filepath.Join("testdata", "ask_response.json"))
	if err != nil {
		t.Fatalf("read fixture: %v", err)
	}
	ans, err := ParseAnswer(string(raw))
	if err != nil {
		t.Fatalf("committed fixture must parse + validate: %v", err)
	}
	if !ans.Found || len(ans.Citations) == 0 || ans.Citations[0].Subject == "" {
		t.Fatal("fixture should exercise the page-level citation contract")
	}
}
