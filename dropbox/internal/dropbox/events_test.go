package dropbox

import (
	"strings"
	"testing"

	"registry"
)

func TestEventsSamplesUseRegistryDropBoxContentOrigin(t *testing.T) {
	// R-QKGB-OPNE
	wantPrefix := registry.BaseURL("dropbox") + "/content?path="
	wantURL := "http://127.0.0.1:3200/content?path=%2Fnotes%2Fmeeting.md"
	wantTypes := map[string]bool{
		EventFileCreated:  true,
		EventFileModified: true,
		EventFileDeleted:  true,
	}

	for _, entry := range Events {
		if !wantTypes[entry.Type] {
			t.Fatalf("unexpected event type %q", entry.Type)
		}
		delete(wantTypes, entry.Type)

		sample, ok := entry.Sample.(filePayload)
		if !ok {
			t.Fatalf("%s sample type = %T, want filePayload", entry.Type, entry.Sample)
		}
		if !strings.HasPrefix(sample.ContentURL, wantPrefix) {
			t.Fatalf("%s content_url = %q, want prefix %q", entry.Type, sample.ContentURL, wantPrefix)
		}
		if sample.ContentURL != wantURL {
			t.Fatalf("%s content_url = %q, want %q", entry.Type, sample.ContentURL, wantURL)
		}
	}
	for eventType := range wantTypes {
		t.Fatalf("missing event type %q", eventType)
	}
}
