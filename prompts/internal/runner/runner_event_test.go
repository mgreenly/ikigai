package runner

import (
	"strings"
	"testing"
)

func TestBuildUserText_NoEventKeepsPromptVerbatim(t *testing.T) {
	got := buildUserText("do the thing", nil)

	if got != "do the thing" {
		t.Errorf("buildUserText = %q, want verbatim user prompt", got)
	}
}

func TestBuildUserText_EventAppendsPreambleAndJSON(t *testing.T) {
	event := []byte(`{"source":"crm","type":"contact.created","event_id":"01J","payload":{"id":"c1"}}`)
	got := buildUserText("do the thing", event)

	if !strings.HasPrefix(got, "do the thing\n\n") {
		t.Fatalf("buildUserText prefix = %q, want user prompt first", got)
	}
	if !strings.Contains(got, eventPreamble) {
		t.Errorf("buildUserText missing event preamble; got %q", got)
	}
	if !strings.Contains(got, `"type": "contact.created"`) {
		t.Errorf("buildUserText missing pretty event content; got %q", got)
	}
}
