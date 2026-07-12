package tools

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"testing"

	"github.com/ikigenba/agentkit"
)

func TestAllReturnsSevenBuiltInTools(t *testing.T) {
	// R-64QY-QN1H
	got := All(t.TempDir(), func(int) bool { return false })
	if len(got) != 7 {
		t.Fatalf("All returned %d tools, want 7", len(got))
	}

	gotNames := make([]string, 0, len(got))
	seen := map[string]bool{}
	for _, tool := range got {
		if tool.Name() == "" {
			t.Fatalf("tool has empty name")
		}
		if tool.Description() == "" {
			t.Fatalf("tool %q has empty description", tool.Name())
		}
		if seen[tool.Name()] {
			t.Fatalf("duplicate tool name %q", tool.Name())
		}
		seen[tool.Name()] = true
		gotNames = append(gotNames, tool.Name())

		var schema map[string]any
		if err := json.Unmarshal(tool.JSONSchema(), &schema); err != nil {
			t.Fatalf("tool %q schema is invalid JSON: %v", tool.Name(), err)
		}
		if schema["type"] != "object" {
			t.Fatalf("tool %q schema type = %v, want object", tool.Name(), schema["type"])
		}
		if _, ok := schema["properties"].(map[string]any); !ok {
			t.Fatalf("tool %q schema has no object properties: %#v", tool.Name(), schema)
		}
	}

	wantNames := []string{nameRead, nameBash, nameWrite, nameEdit, nameGlob, nameGrep, nameFetch}
	if !reflect.DeepEqual(gotNames, wantNames) {
		t.Fatalf("All tool names = %v, want %v", gotNames, wantNames)
	}
}

func TestAllThreadsSandboxRootPerCall(t *testing.T) {
	// R-K1UK-T5K6
	ctx := context.Background()
	rootA := t.TempDir()
	rootB := t.TempDir()

	toolsA := All(rootA, func(int) bool { return false })
	toolsB := All(rootB, func(int) bool { return false })

	callTool(t, ctx, findTool(t, toolsA, nameWrite), map[string]any{
		"file_path": "same.txt",
		"content":   "alpha",
	})
	callTool(t, ctx, findTool(t, toolsB, nameWrite), map[string]any{
		"file_path": "same.txt",
		"content":   "bravo",
	})

	assertFileContent(t, filepath.Join(rootA, "same.txt"), "alpha")
	assertFileContent(t, filepath.Join(rootB, "same.txt"), "bravo")

	gotA := callTool(t, ctx, findTool(t, toolsA, nameRead), map[string]any{"file_path": "same.txt"})
	if gotA != "alpha" {
		t.Fatalf("read from rootA = %q, want alpha", gotA)
	}
	gotB := callTool(t, ctx, findTool(t, toolsB, nameRead), map[string]any{"file_path": "same.txt"})
	if gotB != "bravo" {
		t.Fatalf("read from rootB = %q, want bravo", gotB)
	}

	callTool(t, ctx, findTool(t, toolsA, nameEdit), map[string]any{
		"file_path":   "same.txt",
		"old_string":  "alpha",
		"new_string":  "charlie",
		"replace_all": false,
	})
	assertFileContent(t, filepath.Join(rootA, "same.txt"), "charlie")
	assertFileContent(t, filepath.Join(rootB, "same.txt"), "bravo")

	callTool(t, ctx, findTool(t, toolsA, nameWrite), map[string]any{
		"file_path": "nested/match.txt",
		"content":   "needle\n",
	})
	gotGlob := mustStringSlice(t, callTool(t, ctx, findTool(t, toolsA, nameGlob), map[string]any{
		"path":    "nested",
		"pattern": "*.txt",
	}))
	if !reflect.DeepEqual(gotGlob, []string{"match.txt"}) {
		t.Fatalf("Glob in rootA nested dir = %v, want [match.txt]", gotGlob)
	}
	gotGrep := mustStringSlice(t, callTool(t, ctx, findTool(t, toolsA, nameGrep), map[string]any{
		"path":    "nested",
		"glob":    "*.txt",
		"pattern": "needle",
	}))
	if !reflect.DeepEqual(gotGrep, []string{"match.txt:1:needle"}) {
		t.Fatalf("Grep in rootA nested dir = %v, want [match.txt:1:needle]", gotGrep)
	}
	if _, err := os.Stat(filepath.Join(rootB, "nested", "match.txt")); !os.IsNotExist(err) {
		t.Fatalf("rootA nested write appeared in rootB: %v", err)
	}

	pwd := strings.TrimSpace(callTool(t, ctx, findTool(t, toolsA, nameBash), map[string]any{"command": "pwd"}))
	if pwd != filepath.Clean(rootA) {
		t.Fatalf("Bash pwd = %q, want sandbox root %q", pwd, filepath.Clean(rootA))
	}

	_, err := findTool(t, toolsA, nameRead).Call(ctx, mustJSON(t, map[string]any{"file_path": "../same.txt"}))
	if err == nil {
		t.Fatalf("Read of path escaping sandbox returned nil error")
	}
	if !strings.Contains(err.Error(), "escapes sandbox") {
		t.Fatalf("Read escape error = %v, want escapes sandbox", err)
	}
	if _, err := os.Stat(filepath.Join(filepath.Dir(rootA), "same.txt")); !os.IsNotExist(err) {
		t.Fatalf("escape probe found unexpected file outside sandbox: %v", err)
	}

	_, err = findTool(t, toolsA, nameWrite).Call(ctx, mustJSON(t, map[string]any{
		"file_path": "../escape.txt",
		"content":   "outside",
	}))
	if err == nil {
		t.Fatalf("Write of path escaping sandbox returned nil error")
	}
	if !strings.Contains(err.Error(), "escapes sandbox") {
		t.Fatalf("Write escape error = %v, want escapes sandbox", err)
	}
	if _, err := os.Stat(filepath.Join(filepath.Dir(rootA), "escape.txt")); !os.IsNotExist(err) {
		t.Fatalf("write escape created unexpected file outside sandbox: %v", err)
	}
}

func TestAllRejectsSymlinkEscapes(t *testing.T) {
	// R-K1UK-T5K6
	root := t.TempDir()
	outside := t.TempDir()
	if err := os.WriteFile(filepath.Join(outside, "secret.txt"), []byte("classified\n"), 0o644); err != nil {
		t.Fatalf("WriteFile outside secret: %v", err)
	}
	if err := os.Symlink(outside, filepath.Join(root, "outside")); err != nil {
		t.Skipf("symlink unavailable: %v", err)
	}

	builtins := All(root, func(int) bool { return false })
	expectEscapeError(t, findTool(t, builtins, nameRead), map[string]any{
		"file_path": "outside/secret.txt",
	})
	expectEscapeError(t, findTool(t, builtins, nameWrite), map[string]any{
		"file_path": "outside/new.txt",
		"content":   "leaked",
	})
	expectEscapeError(t, findTool(t, builtins, nameGlob), map[string]any{
		"path":    "outside",
		"pattern": "*.txt",
	})
	expectEscapeError(t, findTool(t, builtins, nameGrep), map[string]any{
		"path":    "outside",
		"pattern": "classified",
	})
	expectEscapeError(t, findTool(t, builtins, nameEdit), map[string]any{
		"file_path":   "outside/secret.txt",
		"old_string":  "classified",
		"new_string":  "published",
		"replace_all": false,
	})

	if _, err := os.Stat(filepath.Join(outside, "new.txt")); !os.IsNotExist(err) {
		t.Fatalf("write through sandbox symlink created outside file: %v", err)
	}
	assertFileContent(t, filepath.Join(outside, "secret.txt"), "classified\n")
}

func TestFetchStreamsAllowedContentIntoSandbox(t *testing.T) {
	// R-65YV-4ES6
	body := []byte("known invoice bytes\n")
	gets := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		gets++
		if r.Method != http.MethodGet {
			t.Errorf("method = %s, want GET", r.Method)
		}
		_, _ = w.Write(body)
	}))
	defer server.Close()
	port := serverPort(t, server.URL)
	root := t.TempDir()
	tool := findTool(t, All(root, func(got int) bool { return got == port }), nameFetch)
	raw := callTool(t, context.Background(), tool, map[string]any{"content_url": server.URL + "/content", "dest_path": "incoming/invoice.pdf"})
	var result struct {
		Path        string `json:"path"`
		Size        int64  `json:"size"`
		ContentHash string `json:"content_hash"`
	}
	if err := json.Unmarshal([]byte(raw), &result); err != nil {
		t.Fatalf("fetch result JSON: %v", err)
	}
	if result.Path != "incoming/invoice.pdf" || result.Size != int64(len(body)) || result.ContentHash != fmt.Sprintf("%x", sha256.Sum256(body)) {
		t.Fatalf("fetch result = %+v", result)
	}
	if gets != 1 {
		t.Fatalf("GET count = %d, want 1", gets)
	}
	assertFileContent(t, filepath.Join(root, "incoming", "invoice.pdf"), string(body))
}

func TestFetchRejectsUnconfinedURLsAndDestinationsBeforeRequest(t *testing.T) {
	// R-676R-I6IV
	// R-68EN-VY9K
	gets := 0
	server := httptest.NewServer(http.HandlerFunc(func(http.ResponseWriter, *http.Request) { gets++ }))
	defer server.Close()
	port := serverPort(t, server.URL)
	root := t.TempDir()
	tool := findTool(t, All(root, func(got int) bool { return got == port }), nameFetch)
	for _, input := range []map[string]any{
		{"content_url": "http://10.0.0.5:3202/file", "dest_path": "x"},
		{"content_url": fmt.Sprintf("http://localhost:%d/file", port), "dest_path": "x"},
		{"content_url": "http://127.0.0.1:1/file", "dest_path": "x"},
		{"content_url": server.URL + "/file", "dest_path": "../outside.bin"},
		{"content_url": server.URL + "/file", "dest_path": filepath.Join(filepath.Dir(root), "outside.bin")},
	} {
		_, err := tool.Call(context.Background(), mustJSON(t, input))
		if err == nil || !strings.HasPrefix(err.Error(), "validation:") {
			t.Fatalf("Fetch(%v) error = %v, want validation", input, err)
		}
	}
	if gets != 0 {
		t.Fatalf("rejected inputs made %d requests, want 0", gets)
	}
	callTool(t, context.Background(), tool, map[string]any{"content_url": server.URL + "/file", "dest_path": "allowed"})
	if gets != 1 {
		t.Fatalf("allowed loopback URL made %d requests, want 1", gets)
	}
	if _, err := os.Stat(filepath.Join(filepath.Dir(root), "outside.bin")); !os.IsNotExist(err) {
		t.Fatalf("escaping destination exists: %v", err)
	}
}

func TestFetchMapsSourceFailuresWithoutDestinationFile(t *testing.T) {
	// R-69MK-9Q09
	redirectTargetCalls := 0
	target := httptest.NewServer(http.HandlerFunc(func(http.ResponseWriter, *http.Request) { redirectTargetCalls++ }))
	defer target.Close()
	source := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/missing":
			w.WriteHeader(http.StatusNotFound)
		case "/conflict":
			w.WriteHeader(http.StatusConflict)
		case "/redirect":
			http.Redirect(w, r, target.URL, http.StatusFound)
		}
	}))
	defer source.Close()
	port := serverPort(t, source.URL)
	closed, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	closedPort := closed.Addr().(*net.TCPAddr).Port
	_ = closed.Close()
	root := t.TempDir()
	tool := findTool(t, All(root, func(got int) bool { return got == port || got == closedPort }), nameFetch)
	for path, prefix := range map[string]string{"/missing": "not_found:", "/conflict": "conflict:", "/redirect": "source_unavailable:"} {
		dest := "failed" + strings.ReplaceAll(path, "/", "_")
		_, err := tool.Call(context.Background(), mustJSON(t, map[string]any{"content_url": source.URL + path, "dest_path": dest}))
		if err == nil || !strings.HasPrefix(err.Error(), prefix) {
			t.Fatalf("%s error = %v, want %s", path, err, prefix)
		}
		if _, statErr := os.Stat(filepath.Join(root, dest)); !os.IsNotExist(statErr) {
			t.Fatalf("%s destination exists: %v", path, statErr)
		}
	}
	_, err = tool.Call(context.Background(), mustJSON(t, map[string]any{"content_url": fmt.Sprintf("http://127.0.0.1:%d/file", closedPort), "dest_path": "refused"}))
	if err == nil || !strings.HasPrefix(err.Error(), "source_unavailable:") {
		t.Fatalf("refused error = %v", err)
	}
	if _, statErr := os.Stat(filepath.Join(root, "refused")); !os.IsNotExist(statErr) {
		t.Fatalf("refused destination exists: %v", statErr)
	}
	if redirectTargetCalls != 0 {
		t.Fatalf("redirect target received %d requests", redirectTargetCalls)
	}
}

func serverPort(t *testing.T, rawURL string) int {
	t.Helper()
	u, err := url.Parse(rawURL)
	if err != nil {
		t.Fatal(err)
	}
	port, err := strconv.Atoi(u.Port())
	if err != nil {
		t.Fatal(err)
	}
	return port
}

func findTool(t *testing.T, tools []agentkit.Tool, name string) agentkit.Tool {
	t.Helper()
	for _, tool := range tools {
		if tool.Name() == name {
			return tool
		}
	}
	t.Fatalf("tool %q not found", name)
	return nil
}

func callTool(t *testing.T, ctx context.Context, tool agentkit.Tool, input map[string]any) string {
	t.Helper()
	out, err := tool.Call(ctx, mustJSON(t, input))
	if err != nil {
		t.Fatalf("%s.Call(%v): %v", tool.Name(), input, err)
	}
	return out
}

func expectEscapeError(t *testing.T, tool agentkit.Tool, input map[string]any) {
	t.Helper()
	_, err := tool.Call(context.Background(), mustJSON(t, input))
	if err == nil {
		t.Fatalf("%s.Call(%v) returned nil error", tool.Name(), input)
	}
	if !strings.Contains(err.Error(), "escapes sandbox") {
		t.Fatalf("%s.Call(%v) error = %v, want escapes sandbox", tool.Name(), input, err)
	}
}

func mustJSON(t *testing.T, v any) json.RawMessage {
	t.Helper()
	b, err := json.Marshal(v)
	if err != nil {
		t.Fatalf("json.Marshal: %v", err)
	}
	return b
}

func mustStringSlice(t *testing.T, raw string) []string {
	t.Helper()
	var got []string
	if err := json.Unmarshal([]byte(raw), &got); err != nil {
		t.Fatalf("json.Unmarshal(%q): %v", raw, err)
	}
	return got
}

func assertFileContent(t *testing.T, path, want string) {
	t.Helper()
	b, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("ReadFile(%s): %v", path, err)
	}
	if string(b) != want {
		t.Fatalf("%s content = %q, want %q", path, string(b), want)
	}
}
