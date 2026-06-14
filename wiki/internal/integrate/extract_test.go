package integrate

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"agentkit/provider"

	"wiki/internal/config"
)

// mockCaller is the unit gate's mocked LLM: it records the request it was handed
// and returns a canned response. The unit gate mocks every LLM from P6a on, so
// extract is exercised here without a key or network.
type mockCaller struct {
	gotSite   config.CallSite
	gotSchema json.RawMessage
	gotMsgs   []provider.Message
	resp      string
	err       error
}

func (m *mockCaller) Structured(ctx context.Context, site config.CallSite, schema json.RawMessage, msgs []provider.Message) (string, error) {
	m.gotSite = site
	m.gotSchema = schema
	m.gotMsgs = msgs
	return m.resp, m.err
}

func loadFixture(t *testing.T) string {
	t.Helper()
	b, err := os.ReadFile(filepath.Join("testdata", "extract_response.json"))
	if err != nil {
		t.Fatalf("read fixture: %v", err)
	}
	return string(b)
}

// loadRecordedFixture reads a P11k recorded real-model fixture (captured from the
// live triple by capture_integration_test.go). The offline (a-ii) prompt-default
// gates parse THESE so parser/schema drift is caught against real output; the
// content-pinned golden tests keep using the deterministic *_response.json fixtures.
func loadRecordedFixture(t *testing.T, name string) string {
	t.Helper()
	b, err := os.ReadFile(filepath.Join("testdata", name))
	if err != nil {
		t.Fatalf("read recorded fixture %s: %v", name, err)
	}
	return string(b)
}

// TestExtractGolden drives the full Extract path with a mocked LLM against the
// committed schema-faithful fixture and asserts the parsed []Subject (the §4.2
// contract / the P6b seam): required fields present, claims stamped with the
// causing inbox id, events carry occurred_at, non-events do not.
func TestExtractGolden(t *testing.T) {
	mock := &mockCaller{resp: loadFixture(t)}
	site := config.CallSite{Name: "extract", Prompt: config.DefaultExtractPrompt, Model: "claude-sonnet-4-6", Effort: "medium"}
	ex := NewExtractor(mock, site)

	hdr := DocumentHeader{
		Source:     "dropbox",
		Title:      "Globex memo",
		Tags:       []string{"corp", "m&a"},
		ReceivedAt: time.Date(2024, 3, 4, 0, 0, 0, 0, time.UTC),
	}
	const inboxID = "01HX4ZTESTINBOXROWID0000001"
	subs, err := ex.Extract(context.Background(), hdr, "Globex Corporation (Globex) acquired Initech.", inboxID)
	if err != nil {
		t.Fatalf("Extract: %v", err)
	}

	if len(subs) != 4 {
		t.Fatalf("got %d subjects, want 4", len(subs))
	}

	// The injected triple flowed through to the caller unchanged (obligation 1).
	if mock.gotSite.Model != "claude-sonnet-4-6" || mock.gotSite.Effort != "medium" {
		t.Errorf("triple not injected: got %+v", mock.gotSite)
	}
	// The header is rendered into the user message with "received on", never
	// "today is" (design §4.2).
	userText := mock.gotMsgs[0].Blocks[0].(provider.TextBlock).Text
	if !strings.Contains(userText, "received on: 2024-03-04") {
		t.Errorf("header not rendered with received-on date; got:\n%s", userText)
	}
	if strings.Contains(userText, "today is") {
		t.Errorf("header must not say 'today is'")
	}

	// First subject: the company with aliases and two claims, all cited.
	globex := subs[0]
	if globex.Type != TypeEntity || globex.Name != "Globex Corporation" {
		t.Errorf("subject 0 wrong: %+v", globex)
	}
	if len(globex.Aliases) != 2 {
		t.Errorf("subject 0 aliases = %v, want 2", globex.Aliases)
	}
	for _, c := range globex.Claims {
		if len(c.Cites) != 1 || c.Cites[0] != inboxID {
			t.Errorf("claim not cited to inbox id: %+v", c)
		}
	}
	if globex.OccurredAt != "" {
		t.Errorf("non-event must not carry occurred_at, got %q", globex.OccurredAt)
	}

	// Resolution annotations are unset — that is P6b/P6b2's job.
	if globex.SubjectID != "" || globex.TargetPage != "" || globex.BaseVersion != 0 {
		t.Errorf("extract must not set resolution annotations: %+v", globex)
	}

	// The event subject carries occurred_at.
	var sawEvent bool
	for _, s := range subs {
		if s.Type == TypeEvent {
			sawEvent = true
			if s.OccurredAt != "2021" {
				t.Errorf("event occurred_at = %q, want 2021", s.OccurredAt)
			}
		}
	}
	if !sawEvent {
		t.Error("expected an event subject in the fixture")
	}
}

func TestParseExtractRejectsBadType(t *testing.T) {
	_, err := ParseExtract(`{"subjects":[{"type":"widget","name":"x","claims":[{"text":"y"}]}]}`, "id")
	if err == nil {
		t.Fatal("expected error for invalid type")
	}
}

func TestParseExtractRejectsMissingName(t *testing.T) {
	_, err := ParseExtract(`{"subjects":[{"type":"entity","name":"","claims":[{"text":"y"}]}]}`, "id")
	if err == nil {
		t.Fatal("expected error for empty name")
	}
}

func TestParseExtractRejectsNoClaims(t *testing.T) {
	_, err := ParseExtract(`{"subjects":[{"type":"entity","name":"x","claims":[]}]}`, "id")
	if err == nil {
		t.Fatal("expected error for claim-less subject (salience gate)")
	}
}

func TestParseExtractStripsCodeFence(t *testing.T) {
	fenced := "```json\n{\"subjects\":[{\"type\":\"entity\",\"name\":\"x\",\"claims\":[{\"text\":\"y\"}]}]}\n```"
	subs, err := ParseExtract(fenced, "id")
	if err != nil {
		t.Fatalf("fenced JSON should parse: %v", err)
	}
	if len(subs) != 1 || subs[0].Name != "x" {
		t.Fatalf("unexpected parse: %+v", subs)
	}
}

func TestParseExtractEmptySubjects(t *testing.T) {
	subs, err := ParseExtract(`{"subjects":[]}`, "id")
	if err != nil {
		t.Fatalf("empty subjects should parse: %v", err)
	}
	if len(subs) != 0 {
		t.Fatalf("want 0 subjects, got %d", len(subs))
	}
}

func TestParseExtractRejectsMalformedJSON(t *testing.T) {
	_, err := ParseExtract(`not json`, "id")
	if err == nil {
		t.Fatal("expected parse error for non-JSON")
	}
}

// --- Standing prompt-default gate (offline, no key — see Prompt-default
// validation). Asserts the extract config-default prompt is non-placeholder and
// carries its six §4.2 sections, and that the parser schema-validates a
// committed, hand-authored, schema-faithful response fixture into []Subject. ---

func TestExtractPromptDefaultGate(t *testing.T) {
	p := config.DefaultExtractPrompt

	// (a-i) non-placeholder: present, above a length floor, no placeholder marker.
	if len(strings.TrimSpace(p)) < 400 {
		t.Fatalf("extract default prompt too short to be real (%d chars)", len(p))
	}
	if strings.Contains(p, "PLACEHOLDER") {
		t.Fatal("extract default prompt is still a placeholder")
	}

	// (a-i) the six §4.2 sections the design pins for extract are all present.
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
			t.Errorf("extract default prompt missing section %q", s)
		}
	}
	// The dialog-awareness clause (§4.2) rides here.
	if !strings.Contains(strings.ToLower(p), "dialog") {
		t.Error("extract default prompt missing the dialog-awareness clause")
	}

	// (a-ii) the parser + schema validate the committed RECORDED real-model fixture
	// (P11k) — so parser/schema drift is caught against real output.
	subs, err := ParseExtract(loadRecordedFixture(t, "extract_recorded.json"), "01HXBLUNTFIXTUREINBOXROW001")
	if err != nil {
		t.Fatalf("recorded fixture must parse: %v", err)
	}
	if len(subs) == 0 {
		t.Fatal("recorded fixture parsed to zero subjects")
	}
}

// TestExtractSchemaIsValidJSON guards the structured-output schema itself: it
// must be valid JSON the backend can hand to its native structured-output mode.
func TestExtractSchemaIsValidJSON(t *testing.T) {
	var v any
	if err := json.Unmarshal(ExtractSchema, &v); err != nil {
		t.Fatalf("ExtractSchema is not valid JSON: %v", err)
	}
}
