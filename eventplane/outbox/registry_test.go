package outbox

import (
	"context"
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
			Type:        "contact.created",
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
			Type:        "contact.updated",
			Description: "a contact was updated",
			Sample:      samplePayload{ID: "c2"},
		},
	}
}

func TestIndexReturnsEveryType(t *testing.T) {
	idx := testRegistry().Index()
	want := []string{"contact.created", "contact.updated"}
	if len(idx) != len(want) {
		t.Fatalf("Index len = %d, want %d", len(idx), len(want))
	}
	for i, w := range want {
		if idx[i]["type"] != w {
			t.Errorf("Index[%d].type = %v, want %q", i, idx[i]["type"], w)
		}
		if idx[i]["description"] == "" {
			t.Errorf("Index[%d] missing description", i)
		}
	}
}

func TestDetailSchemaMatchesTags(t *testing.T) {
	d, err := testRegistry().Detail("contact.created")
	if err != nil {
		t.Fatalf("Detail: %v", err)
	}
	if d["type"] != "contact.created" {
		t.Errorf("type = %v", d["type"])
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
	d, err := testRegistry().Detail("contact.created")
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
}

func TestDetailUnknownTypeCorrectiveError(t *testing.T) {
	_, err := testRegistry().Detail("nope")
	if err == nil {
		t.Fatal("expected error for unknown type")
	}
	var ute *UnknownEventTypeError
	if !errors.As(err, &ute) {
		t.Fatalf("error is not *UnknownEventTypeError: %T", err)
	}
	if ute.Type != "nope" {
		t.Errorf("ute.Type = %q", ute.Type)
	}
	want := []string{"contact.created", "contact.updated"}
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

func TestAppendRejectsUnregisteredTypeWithRegistry(t *testing.T) {
	reg := testRegistry()
	o, db := newMemOutbox(t, func(opts *Options) { opts.Registry = reg })

	// A declared type is accepted.
	tx, _ := db.BeginTx(context.Background(), nil)
	if err := o.Append(tx, Event{Kind: "contact.created", Payload: json.RawMessage(`{}`)}); err != nil {
		t.Fatalf("declared type rejected: %v", err)
	}
	_ = tx.Commit()

	// An undeclared type is rejected.
	tx2, _ := db.BeginTx(context.Background(), nil)
	defer tx2.Rollback()
	err := o.Append(tx2, Event{Kind: "contact.deleted", Payload: json.RawMessage(`{}`)})
	if err == nil {
		t.Fatal("expected Append to reject an unregistered type")
	}
}

func TestAppendUnchangedWithEmptyRegistry(t *testing.T) {
	o, db := newMemOutbox(t) // no registry
	tx, _ := db.BeginTx(context.Background(), nil)
	defer tx.Rollback()
	if err := o.Append(tx, Event{Kind: "anything.goes", Payload: json.RawMessage(`{}`)}); err != nil {
		t.Fatalf("empty registry must not constrain Append: %v", err)
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
