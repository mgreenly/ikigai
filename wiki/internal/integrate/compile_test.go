package integrate

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"agentkit/provider"

	"wiki/internal/config"
)

func loadCompileFixture(t *testing.T) string {
	t.Helper()
	b, err := os.ReadFile(filepath.Join("testdata", "compile_response.json"))
	if err != nil {
		t.Fatalf("read fixture: %v", err)
	}
	return string(b)
}

func compileEvents() []EventRow {
	return []EventRow{
		{ID: "01EVTA", Source: "crm:deal.stage_changed", Payload: []byte(`{"deal":"Globex renewal","to":"negotiation"}`)},
		{ID: "01EVTB", Source: "crm:deal.stage_changed", Payload: []byte(`{"deal":"Globex renewal","to":"closed_won"}`)},
	}
}

// TestCompileGolden drives the full Compile path with a mocked LLM against the
// committed schema-faithful fixture and asserts the parsed []Subject: required
// fields present, per-claim cites resolve to presented event ids, events carry
// occurred_at, the event pile is rendered with [ids] visible, and the injected
// triple flows through unchanged (obligation 1).
func TestCompileGolden(t *testing.T) {
	mock := &mockCaller{resp: loadCompileFixture(t)}
	site := config.CallSite{Name: "compile", Prompt: config.DefaultCompilePrompt, Model: "claude-sonnet-4-6", Effort: "medium"}
	c := NewCompiler(mock, site)

	subs, err := c.Compile(context.Background(), compileEvents())
	if err != nil {
		t.Fatalf("Compile: %v", err)
	}
	if len(subs) != 2 {
		t.Fatalf("got %d subjects, want 2", len(subs))
	}

	// The injected triple flowed through to the caller unchanged (obligation 1).
	if mock.gotSite.Model != "claude-sonnet-4-6" || mock.gotSite.Effort != "medium" {
		t.Errorf("triple not injected: got %+v", mock.gotSite)
	}
	// The event pile is rendered with [ids] visible (design §5).
	user := mockUserText(t, mock)
	if !strings.Contains(user, "[01EVTA]") || !strings.Contains(user, "[01EVTB]") {
		t.Errorf("event ids not rendered into the pile; got:\n%s", user)
	}

	// First subject: the event with per-claim cites + occurred_at.
	ev := subs[0]
	if ev.Type != TypeEvent || ev.OccurredAt != "2024-05-23" {
		t.Errorf("subject 0 wrong: %+v", ev)
	}
	if len(ev.Claims) != 1 || len(ev.Claims[0].Cites) != 2 {
		t.Errorf("event claim cites = %+v, want 2 cites", ev.Claims)
	}
	// Resolution annotations are unset — that is resolve/assemble's job.
	if ev.SubjectID != "" || ev.TargetPage != "" || ev.BaseVersion != 0 {
		t.Errorf("compile must not set resolution annotations: %+v", ev)
	}

	// Second subject: an entity must NOT carry occurred_at (events-only — §4.1/§5).
	if subs[1].Type != TypeEntity || subs[1].OccurredAt != "" {
		t.Errorf("non-event must not carry occurred_at: %+v", subs[1])
	}
}

func mockUserText(t *testing.T, m *mockCaller) string {
	t.Helper()
	if len(m.gotMsgs) == 0 {
		t.Fatal("no message captured")
	}
	return m.gotMsgs[0].Blocks[0].(provider.TextBlock).Text
}

func TestParseCompileRejectsUncitedClaim(t *testing.T) {
	valid := map[string]struct{}{"01EVTA": {}}
	_, err := ParseCompile(`{"subjects":[{"type":"entity","name":"x","aliases":[],"claims":[{"text":"y","cites":[]}]}]}`, valid)
	if err == nil {
		t.Fatal("expected error for an uncited digest claim (§5 every claim cites)")
	}
}

func TestParseCompileRejectsFabricatedCite(t *testing.T) {
	valid := map[string]struct{}{"01EVTA": {}}
	_, err := ParseCompile(`{"subjects":[{"type":"entity","name":"x","aliases":[],"claims":[{"text":"y","cites":["01NOTREAL"]}]}]}`, valid)
	if err == nil {
		t.Fatal("expected error for a fabricated citation (id not in the event pile)")
	}
}

func TestParseCompileAcceptsValidCite(t *testing.T) {
	valid := map[string]struct{}{"01EVTA": {}}
	subs, err := ParseCompile(`{"subjects":[{"type":"entity","name":"x","aliases":[],"claims":[{"text":"y","cites":["01EVTA"]}]}]}`, valid)
	if err != nil {
		t.Fatalf("valid cite should parse: %v", err)
	}
	if len(subs) != 1 || len(subs[0].Claims[0].Cites) != 1 {
		t.Fatalf("unexpected parse: %+v", subs)
	}
}

func TestParseCompileRejectsBadType(t *testing.T) {
	valid := map[string]struct{}{"01EVTA": {}}
	_, err := ParseCompile(`{"subjects":[{"type":"widget","name":"x","claims":[{"text":"y","cites":["01EVTA"]}]}]}`, valid)
	if err == nil {
		t.Fatal("expected error for invalid type")
	}
}

func TestParseCompileEmptySubjects(t *testing.T) {
	subs, err := ParseCompile(`{"subjects":[]}`, map[string]struct{}{})
	if err != nil {
		t.Fatalf("empty subjects should parse: %v", err)
	}
	if len(subs) != 0 {
		t.Fatalf("want 0 subjects, got %d", len(subs))
	}
}

func TestCompileSchemaIsValidJSON(t *testing.T) {
	var v any
	if err := json.Unmarshal(CompileSchema, &v); err != nil {
		t.Fatalf("CompileSchema is not valid JSON: %v", err)
	}
}

// --- Standing prompt-default gate (offline, no key — see Prompt-default
// validation). Asserts the compile config-default prompt is non-placeholder and
// carries extract's six-section skeleton with its four §5 deltas, and that the
// parser schema-validates a committed fixture into subjects[] with per-claim cites
// + occurred_at. ---

func TestCompilePromptDefaultGate(t *testing.T) {
	p := config.DefaultCompilePrompt

	if len(strings.TrimSpace(p)) < 400 {
		t.Fatalf("compile default prompt too short to be real (%d chars)", len(p))
	}
	if strings.Contains(p, "PLACEHOLDER") {
		t.Fatal("compile default prompt is still a placeholder")
	}

	// Extract's six-section skeleton is present.
	sections := []string{
		"## 1. Task framing",
		"## 2. Subject schema",
		"## 3. Salience",
		"## 4. Identity discipline",
		"## 5. Claims discipline",
		"## 6. Output recap",
	}
	for _, s := range sections {
		if !strings.Contains(p, s) {
			t.Errorf("compile default prompt missing section %q", s)
		}
	}
	// The four §5 deltas: aggregation framing, micro-facts-are-noise salience, the
	// cites obligation, and the occurred_at obligation.
	low := strings.ToLower(p)
	for _, delta := range []string{"aggregation", "noise", "cites", "occurred_at"} {
		if !strings.Contains(low, delta) {
			t.Errorf("compile default prompt missing §5 delta marker %q", delta)
		}
	}

	// The parser + schema validate the committed RECORDED real-model fixture (P11k)
	// into subjects[] carrying per-claim cites + occurred_at — so parser/schema
	// drift is caught against real output.
	valid := map[string]struct{}{"01EVTA": {}, "01EVTB": {}}
	subs, err := ParseCompile(loadRecordedFixture(t, "compile_recorded.json"), valid)
	if err != nil {
		t.Fatalf("recorded fixture must parse: %v", err)
	}
	if len(subs) == 0 {
		t.Fatal("committed fixture parsed to zero subjects")
	}
	if len(subs[0].Claims) == 0 || len(subs[0].Claims[0].Cites) == 0 {
		t.Fatal("committed fixture's claims must carry per-claim cites")
	}
	sawOccurred := false
	for _, s := range subs {
		if s.Type == TypeEvent && s.OccurredAt != "" {
			sawOccurred = true
		}
	}
	if !sawOccurred {
		t.Fatal("committed fixture must carry an event with occurred_at")
	}
}
