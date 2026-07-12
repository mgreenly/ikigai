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
		if !wantTypes[entry.Kind] {
			t.Fatalf("unexpected event kind %q", entry.Kind)
		}
		delete(wantTypes, entry.Kind)

		sample, ok := entry.Sample.(filePayload)
		if !ok {
			t.Fatalf("%s sample type = %T, want filePayload", entry.Kind, entry.Sample)
		}
		if !strings.HasPrefix(sample.ContentURL, wantPrefix) {
			t.Fatalf("%s content_url = %q, want prefix %q", entry.Kind, sample.ContentURL, wantPrefix)
		}
		if sample.ContentURL != wantURL {
			t.Fatalf("%s content_url = %q, want %q", entry.Kind, sample.ContentURL, wantURL)
		}
		if sample.Origin != OriginDropbox {
			t.Fatalf("%s origin = %q, want %q", entry.Kind, sample.Origin, OriginDropbox)
		}
	}
	for eventType := range wantTypes {
		t.Fatalf("missing event type %q", eventType)
	}
}
