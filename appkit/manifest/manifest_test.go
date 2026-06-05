package manifest_test

import (
	"reflect"
	"strings"
	"testing"

	"appkit/manifest"
)

func TestEmit_ServiceWithFeedAndExtras(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App:   "crm",
		Mount: "/srv/crm/",
		Port:  3001,
		MCP:   true,
		Feed:  "/feed",
		Extras: []manifest.KV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
			{Key: "OUTBOX_RETENTION_MAX_ROWS", Value: "1000000"},
		},
	})
	// Fixed order: APP, MOUNT, DEFAULT, PORT, MCP, FEED, then extras in order.
	want := "APP=crm\n" +
		"MOUNT=/srv/crm/\n" +
		"DEFAULT=false\n" +
		"PORT=3001\n" +
		"MCP=true\n" +
		"FEED=/feed\n" +
		"OUTBOX_RETENTION_DAYS=7\n" +
		"OUTBOX_RETENTION_MAX_ROWS=1000000\n"
	if got != want {
		t.Fatalf("emit mismatch\n got:\n%q\nwant:\n%q", got, want)
	}
}

func TestEmit_ConsumerCommaJoined(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App: "wiki", Mount: "/srv/wiki/", Port: 3006, MCP: true,
		Consumes: []string{"dropbox", "crm"},
		Extras: []manifest.KV{
			{Key: "WIKI_INGEST_MODEL", Value: "claude-sonnet-4-6"},
			{Key: "WIKI_INGEST_MAX_TOKENS", Value: "8192"},
		},
	})
	want := "APP=wiki\nMOUNT=/srv/wiki/\nDEFAULT=false\nPORT=3006\nMCP=true\n" +
		"CONSUMES=dropbox,crm\n" +
		"WIKI_INGEST_MODEL=claude-sonnet-4-6\nWIKI_INGEST_MAX_TOKENS=8192\n"
	if got != want {
		t.Fatalf("emit mismatch\n got:\n%q\nwant:\n%q", got, want)
	}
}

func TestEmit_ApexOmitsMCP(t *testing.T) {
	got := manifest.Emit(manifest.Fields{
		App: "dashboard", Mount: "/", Default: true, Port: 3000,
	})
	want := "APP=dashboard\nMOUNT=/\nDEFAULT=true\nPORT=3000\n"
	if got != want {
		t.Fatalf("apex emit mismatch\n got:\n%q\nwant:\n%q", got, want)
	}
	if strings.Contains(got, "MCP") {
		t.Errorf("apex manifest must not carry an MCP line: %q", got)
	}
}

func TestParse_RoundTripsEmit(t *testing.T) {
	f := manifest.Fields{
		App: "ledger", Mount: "/srv/ledger/", Port: 3002, MCP: true, Feed: "/feed",
		Extras: []manifest.KV{
			{Key: "OUTBOX_RETENTION_DAYS", Value: "7"},
		},
	}
	kv, keys, err := manifest.Parse(strings.NewReader(manifest.Emit(f)))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	want := map[string]string{
		"APP": "ledger", "MOUNT": "/srv/ledger/", "DEFAULT": "false",
		"PORT": "3002", "MCP": "true", "FEED": "/feed",
		"OUTBOX_RETENTION_DAYS": "7",
	}
	if !reflect.DeepEqual(kv, want) {
		t.Fatalf("parsed map = %v, want %v", kv, want)
	}
	wantKeys := []string{"APP", "MOUNT", "DEFAULT", "PORT", "MCP", "FEED", "OUTBOX_RETENTION_DAYS"}
	if !reflect.DeepEqual(keys, wantKeys) {
		t.Fatalf("key order = %v, want %v", keys, wantKeys)
	}
}

func TestParse_IgnoresCommentsAndStripsQuotes(t *testing.T) {
	// Matches the two on-box parsers: skip blank/'#' lines, strip one quote pair.
	in := "# a documentary comment\n" +
		"\n" +
		"APP=crm\n" +
		"  MOUNT = /srv/crm/  \n" + // surrounding whitespace trimmed
		`FEED="/feed"` + "\n" + // quote pair stripped
		"# trailing comment\n"
	kv, _, err := manifest.Parse(strings.NewReader(in))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if kv["APP"] != "crm" {
		t.Errorf("APP = %q", kv["APP"])
	}
	if kv["MOUNT"] != "/srv/crm/" {
		t.Errorf("MOUNT = %q (whitespace not trimmed)", kv["MOUNT"])
	}
	if kv["FEED"] != "/feed" {
		t.Errorf("FEED = %q (quotes not stripped)", kv["FEED"])
	}
	if _, ok := kv["#"]; ok {
		t.Error("comment line leaked into parsed map")
	}
}
