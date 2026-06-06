package mcp

import (
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"dropbox/internal/db"
	"dropbox/internal/dropbox"

	_ "modernc.org/sqlite"
)

func newHandler(t *testing.T) *Handler {
	t.Helper()
	conn, err := sql.Open("sqlite", ":memory:")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	if _, err := conn.Exec(`PRAGMA foreign_keys = ON`); err != nil {
		t.Fatalf("fk: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return NewHandler(dropbox.NewService(conn), "v-test", "dropbox", nil)
}

// rpc drives one JSON-RPC call through ServeHTTP and returns the decoded result
// object. params is the raw JSON for "params".
func rpc(t *testing.T, h *Handler, method, params string) map[string]any {
	t.Helper()
	body := `{"jsonrpc":"2.0","id":1,"method":"` + method + `","params":` + params + `}`
	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(body))
	req.Header.Set("X-Owner-Email", "me@example.com")
	req.Header.Set("X-Client-Id", "client-123")
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("%s: status %d", method, rec.Code)
	}
	var env struct {
		Result map[string]any `json:"result"`
		Error  any            `json:"error"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &env); err != nil {
		t.Fatalf("%s: decode envelope: %v\n%s", method, err, rec.Body.String())
	}
	if env.Error != nil {
		t.Fatalf("%s: transport error %v", method, env.Error)
	}
	return env.Result
}

// callTool invokes tools/call and returns the decoded text payload plus the
// isError flag.
func callTool(t *testing.T, h *Handler, name, args string) (map[string]any, bool) {
	t.Helper()
	res := rpc(t, h, "tools/call", `{"name":"`+name+`","arguments":`+args+`}`)
	isErr, _ := res["isError"].(bool)
	content, ok := res["content"].([]any)
	if !ok || len(content) == 0 {
		t.Fatalf("%s: no content: %v", name, res)
	}
	text := content[0].(map[string]any)["text"].(string)
	var payload map[string]any
	if err := json.Unmarshal([]byte(text), &payload); err != nil {
		t.Fatalf("%s: decode payload %q: %v", name, text, err)
	}
	return payload, isErr
}

func TestToolsList_HasExactlyOne(t *testing.T) {
	h := newHandler(t)
	res := rpc(t, h, "tools/list", `{}`)
	tools, _ := res["tools"].([]any)
	if len(tools) != 1 {
		t.Fatalf("tools/list returned %d tools, want 1", len(tools))
	}
	names := map[string]bool{}
	for _, tl := range tools {
		names[tl.(map[string]any)["name"].(string)] = true
	}
	if !names["ikigenba_dropbox_health"] {
		t.Errorf("tools/list missing ikigenba_dropbox_health (got %v)", names)
	}
	// whoami is folded away — it must not reappear.
	if names["dropbox_whoami"] || names["ikigenba_dropbox_whoami"] {
		t.Errorf("tools/list still advertises a whoami tool: %v", names)
	}
}

func TestHealth_Envelope(t *testing.T) {
	h := newHandler(t)
	p, isErr := callTool(t, h, "ikigenba_dropbox_health", `{}`)
	if isErr {
		t.Fatal("health isError")
	}
	// Envelope required top-level keys + identity (no reporter here → details {}).
	if p["status"] != "ok" || p["version"] != "v-test" || p["service"] != "dropbox" {
		t.Errorf("health envelope keys = %v", p)
	}
	if p["owner_email"] != "me@example.com" || p["client_id"] != "client-123" {
		t.Errorf("health identity = %v", p)
	}
	d, ok := p["details"].(map[string]any)
	if !ok {
		t.Fatalf("details missing or not an object: %v", p["details"])
	}
	if len(d) != 0 {
		t.Errorf("details = %v, want empty {} with no reporter", d)
	}
}

func TestHealth_ReporterPopulatesDetails(t *testing.T) {
	// Build a Service with a real DB + mirror and wire a Health reporter (the
	// Spec.Health path) so the telemetry lands UNDER details — not splatted at
	// the top level.
	conn, err := sql.Open("sqlite", "file:"+t.TempDir()+"/dropbox.db?_pragma=foreign_keys(ON)")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	conn.SetMaxOpenConns(1)
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	mirror, err := dropbox.NewMirror(t.TempDir())
	if err != nil {
		t.Fatalf("mirror: %v", err)
	}
	svc := dropbox.NewService(conn)
	svc.Mirror = mirror

	// The reporter mirrors cmd/dropbox/main.go's Spec.Health: telemetry only,
	// no identity.
	reporter := func(ctx context.Context) (map[string]any, error) {
		info, err := svc.Health("", "")
		if err != nil {
			return nil, err
		}
		return map[string]any{
			"mirror_bytes":     info.MirrorBytes,
			"disk_free_bytes":  info.DiskFreeBytes,
			"disk_total_bytes": info.DiskTotalBytes,
			"failed_files":     info.FailedFiles,
		}, nil
	}
	h := NewHandler(svc, "v-test", "dropbox", reporter)

	p, isErr := callTool(t, h, "ikigenba_dropbox_health", `{}`)
	if isErr {
		t.Fatal("health isError")
	}
	if p["owner_email"] != "me@example.com" || p["client_id"] != "client-123" {
		t.Errorf("health identity = %v", p)
	}
	d, ok := p["details"].(map[string]any)
	if !ok {
		t.Fatalf("details missing or not an object: %v", p["details"])
	}
	for _, k := range []string{"mirror_bytes", "disk_free_bytes", "disk_total_bytes", "failed_files"} {
		if _, ok := d[k]; !ok {
			t.Errorf("details missing telemetry field %q (details %v)", k, d)
		}
		// Telemetry must NOT splat at the top level (DECISIONS §3).
		if _, top := p[k]; top {
			t.Errorf("telemetry field %q splatted at top level, want only under details", k)
		}
	}
	// Disk numbers must be plausible/non-zero for a real filesystem.
	if dt, _ := d["disk_total_bytes"].(float64); dt <= 0 {
		t.Errorf("details.disk_total_bytes = %v, want > 0", d["disk_total_bytes"])
	}
	if df, _ := d["disk_free_bytes"].(float64); df <= 0 {
		t.Errorf("details.disk_free_bytes = %v, want > 0", d["disk_free_bytes"])
	}
}

func TestUnknownTool_IsToolError(t *testing.T) {
	h := newHandler(t)
	res := rpc(t, h, "tools/call", `{"name":"dropbox_bogus","arguments":{}}`)
	if isErr, _ := res["isError"].(bool); !isErr {
		t.Errorf("expected isError for unknown tool, got %v", res)
	}
}
