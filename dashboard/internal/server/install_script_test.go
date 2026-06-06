package server

import (
	"net/http"
	"strings"
	"testing"
)

// TestInstallScript: GET /install serves a text/plain bash script that emits an
// idempotent, user-scoped `claude mcp add` for every MCP=true service on the box
// (crm + ledger), self-templated to the request host, and omits the dashboard
// (no MCP=true). It is public — no session cookie is supplied.
func TestInstallScript(t *testing.T) {
	root := t.TempDir()
	writeManifest(t, root, "dashboard", "APP=dashboard\nMOUNT=/\nDEFAULT=true\n")
	writeManifest(t, root, "crm", "APP=crm\nMOUNT=/srv/crm/\nMCP=true\n")
	writeManifest(t, root, "ledger", "APP=ledger\nMOUNT=/srv/ledger/\nMCP=true\n")

	opts := newServerDeps(t).opts()
	opts.ManifestRoot = root
	srv, err := New(opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}

	rec := do(t, srv, "GET", "https://int.ikigenba.com/install",
		map[string]string{"X-Forwarded-Proto": "https"})
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200", rec.Code)
	}
	if ct := rec.Header().Get("Content-Type"); !strings.HasPrefix(ct, "text/plain") {
		t.Errorf("Content-Type = %q, want text/plain", ct)
	}
	body := rec.Body.String()

	wantLines := []string{
		"#!/usr/bin/env bash",
		"set -euo pipefail",
		"claude mcp remove --scope user crm >/dev/null 2>&1 || true",
		"claude mcp add --scope user --transport http crm https://int.ikigenba.com/srv/crm/mcp",
		"claude mcp remove --scope user ledger >/dev/null 2>&1 || true",
		"claude mcp add --scope user --transport http ledger https://int.ikigenba.com/srv/ledger/mcp",
	}
	for _, want := range wantLines {
		if !strings.Contains(body, want) {
			t.Errorf("script missing %q:\n%s", want, body)
		}
	}
	if strings.Contains(body, "dashboard") {
		t.Errorf("dashboard (no MCP) leaked into install script:\n%s", body)
	}
}
