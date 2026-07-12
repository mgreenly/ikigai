package outbox

import (
	"encoding/json"
	"errors"
	"reflect"
	"testing"

	_ "modernc.org/sqlite"
)

// sampleTag is a nested struct used to exercise inline-object reflection.
type sampleTag struct {
	Name  string `json:"name"`
	Color string `json:"color,omitempty"`
}

// samplePayload exercises every shape the reflector supports: string, bool,
// integer, pointer (optional), slice-of-struct, and a json:"-" skip.
type samplePayload struct {
	ID       string      `json:"id"`
	Count    int         `json:"count"`
	Active   bool        `json:"active"`
	Note     *string     `json:"note,omitempty"`
	Tags     []sampleTag `json:"tags"`
	Internal string      `json:"-"`
}

func testRegistry() Registry {
	note := "hello"
	return Registry{
		{
			Kind:        "create",
			Subject:     "/<contact id>",
			Description: "a contact was created",
			Sample: samplePayload{
				ID:     "c1",
				Count:  3,
				Active: true,
				Note:   &note,
				Tags:   []sampleTag{{Name: "vip"}},
			},
		},
		{
			Kind:        "update",
			Subject:     "any contact",
			Description: "a contact was updated",
			Sample:      samplePayload{ID: "c2"},
		},
	}
}

func TestIndexReturnsEveryType(t *testing.T) {
	// R-3QI1-0H4G
	idx := testRegistry().Index()
	want := []map[string]any{
		{"kind": "create", "subject": "/<contact id>", "description": "a contact was created"},
		{"kind": "update", "subject": "any contact", "description": "a contact was updated"},
	}
	if len(idx) != len(want) {
		t.Fatalf("Index len = %d, want %d", len(idx), len(want))
	}
	if !reflect.DeepEqual(idx, want) {
		t.Errorf("Index = %#v, want %#v", idx, want)
	}
}

func TestDetailSchemaMatchesTags(t *testing.T) {
	d, err := testRegistry().Detail("create")
	if err != nil {
		t.Fatalf("Detail: %v", err)
	}
	if d["kind"] != "create" {
		t.Errorf("kind = %v", d["kind"])
	}

	sch := d["schema"].(map[string]any)
	if sch["type"] != "object" {
		t.Errorf("schema type = %v, want object", sch["type"])
	}
	props := sch["properties"].(map[string]any)
	wantProps := []string{"id", "count", "active", "note", "tags"}
	for _, p := range wantProps {
		if _, ok := props[p]; !ok {
			t.Errorf("missing property %q", p)
		}
	}
	if _, ok := props["Internal"]; ok {
		t.Error("json:\"-\" field leaked into schema")
	}
	if _, ok := props["-"]; ok {
		t.Error("json:\"-\" field leaked into schema as \"-\"")
	}

	// required = non-pointer, non-omitempty fields.
	req := toStringSet(sch["required"])
	for _, r := range []string{"id", "count", "active", "tags"} {
		if !req[r] {
			t.Errorf("expected %q in required, got %v", r, sch["required"])
		}
	}
	if req["note"] {
		t.Error("optional pointer field note must not be required")
	}

	// property kinds.
	if props["id"].(map[string]any)["type"] != "string" {
		t.Errorf("id type = %v", props["id"])
	}
	if props["count"].(map[string]any)["type"] != "integer" {
		t.Errorf("count type = %v", props["count"])
	}
	if props["active"].(map[string]any)["type"] != "boolean" {
		t.Errorf("active type = %v", props["active"])
	}
	tags := props["tags"].(map[string]any)
	if tags["type"] != "array" {
		t.Errorf("tags type = %v", tags["type"])
	}
	items := tags["items"].(map[string]any)
	if items["type"] != "object" {
		t.Errorf("tags items type = %v", items["type"])
	}
	if _, ok := items["properties"].(map[string]any)["name"]; !ok {
		t.Error("nested struct missing 'name' property")
	}
}

func TestDetailExampleRoundTrips(t *testing.T) {
	// R-3RPX-E8V5
	d, err := testRegistry().Detail("create")
	if err != nil {
		t.Fatalf("Detail: %v", err)
	}
	b, err := json.Marshal(d["example"])
	if err != nil {
		t.Fatalf("marshal example: %v", err)
	}
	var got samplePayload
	if err := json.Unmarshal(b, &got); err != nil {
		t.Fatalf("example does not round-trip through samplePayload: %v", err)
	}
	if got.ID != "c1" || got.Count != 3 || !got.Active {
		t.Errorf("round-tripped example = %+v", got)
	}
	if got.Note == nil || *got.Note != "hello" {
		t.Errorf("note did not round-trip: %v", got.Note)
	}
	if len(got.Tags) != 1 || got.Tags[0].Name != "vip" {
		t.Errorf("tags did not round-trip: %+v", got.Tags)
	}
	props := d["schema"].(map[string]any)["properties"].(map[string]any)
	for field := range d["example"].(map[string]any) {
		if _, ok := props[field]; !ok {
			t.Errorf("example field %q absent from schema", field)
		}
	}

	_, err = testRegistry().Detail("nope")
	if err == nil {
		t.Fatal("expected error for unknown kind")
	}
	var ute *UnknownKindError
	if !errors.As(err, &ute) {
		t.Fatalf("error is not *UnknownKindError: %T", err)
	}
	if ute.Kind != "nope" {
		t.Errorf("ute.Kind = %q", ute.Kind)
	}
	want := []string{"create", "update"}
	if !reflect.DeepEqual(ute.Valid, want) {
		t.Errorf("ute.Valid = %v, want %v", ute.Valid, want)
	}
}

func TestSchemaPanicsOnUnsupportedKind(t *testing.T) {
	defer func() {
		if recover() == nil {
			t.Fatal("expected panic on unsupported kind")
		}
	}()
	type bad struct {
		Ch chan int `json:"ch"`
	}
	_ = schema(bad{})
}

func TestCouldMatchFamilyKindWithOpenSubject(t *testing.T) {
	// R-3SXT-S0LU
	r := Registry{{Kind: "create", Subject: "/<mirror path>"}}
	for _, tc := range []struct {
		filter string
		want   bool
	}{
		{"dropbox:create/bills/**", true}, {"dropbox:delete/**", false}, {"dropbox:*", true},
	} {
		got, err := r.CouldMatch("dropbox", tc.filter)
		if err != nil || got != tc.want {
			t.Errorf("CouldMatch(%q) = %v, %v; want %v", tc.filter, got, err, tc.want)
		}
	}
	if _, err := r.CouldMatch("dropbox", "dropbox:["); err == nil {
		t.Error("malformed filter returned nil error")
	}
}

func TestCouldMatchLeavesSubjectOpen(t *testing.T) {
	// R-3U5Q-5SCJ
	got, err := (Registry{{Kind: "tick", Subject: "documented schedules only"}}).CouldMatch("cron", "cron:tick/some-schedule-nobody-declared")
	if err != nil || !got {
		t.Fatalf("CouldMatch open subject = %v, %v; want true", got, err)
	}
}

func toStringSet(v any) map[string]bool {
	out := map[string]bool{}
	if v == nil {
		return out
	}
	for _, s := range v.([]string) {
		out[s] = true
	}
	return out
}
