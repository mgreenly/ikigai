package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

type filesystemRoute struct {
	method string
	path   string
}

// filesystemAPIRoutes is the canonical filesystem route list mounted by
// Handlers in main.go. Keep it aligned with the rt.HandleLoopback registrations.
var filesystemAPIRoutes = []filesystemRoute{
	{method: "GET", path: "/content"},
	{method: "PUT", path: "/content"},
	{method: "DELETE", path: "/content"},
	{method: "POST", path: "/mkdir"},
	{method: "POST", path: "/move"},
	{method: "GET", path: "/list"},
	{method: "GET", path: "/stat"},
}

func shippedFilesystemAPIDoc(t *testing.T) string {
	t.Helper()
	doc, err := os.ReadFile(filepath.Join("..", "..", "docs", "filesystem-api.md"))
	if err != nil {
		t.Fatalf("read shipped filesystem API documentation: %v", err)
	}
	return string(doc)
}

func undocumentedFilesystemRoutes(doc string, routes []filesystemRoute) []filesystemRoute {
	var missing []filesystemRoute
	for _, route := range routes {
		if !strings.Contains(doc, route.method+" "+route.path) {
			missing = append(missing, route)
		}
	}
	return missing
}

// R-KVL9-O1M5
func TestFilesystemAPIDocumentationListsEveryInScopeRoute(t *testing.T) {
	doc := shippedFilesystemAPIDoc(t)
	if missing := undocumentedFilesystemRoutes(doc, filesystemAPIRoutes); len(missing) != 0 {
		t.Fatalf("filesystem API documentation misses routes: %#v", missing)
	}
}

// R-KWT6-1TCU
func TestFilesystemAPIDocumentationCoversRegisteredRoutesAndExcludesPlumbing(t *testing.T) {
	doc := shippedFilesystemAPIDoc(t)
	mainSource, err := os.ReadFile("main.go")
	if err != nil {
		t.Fatalf("read composition root: %v", err)
	}
	for _, route := range filesystemAPIRoutes {
		registration := `rt.HandleLoopback("` + route.method + " " + route.path + `"`
		if !strings.Contains(string(mainSource), registration) {
			t.Fatalf("canonical filesystem route %s %s is not registered in main.go", route.method, route.path)
		}
	}

	withoutMove := strings.Replace(doc, "POST /move", "POST /renamed", 1)
	if missing := undocumentedFilesystemRoutes(withoutMove, filesystemAPIRoutes); len(missing) != 1 || missing[0] != (filesystemRoute{method: "POST", path: "/move"}) {
		t.Fatalf("coverage guard did not detect an undocumented registered route: %#v", missing)
	}

	plumbing := []filesystemRoute{
		{method: "GET", path: "/feed"},
		{method: "GET", path: "/health"},
		{method: "POST", path: "/mcp"},
		{method: "GET", path: "/.well-known/oauth-protected-resource"},
		{method: "GET", path: "/{$}"},
	}
	for _, route := range plumbing {
		if strings.Contains(doc, route.method+" "+route.path) {
			t.Fatalf("plumbing route %s %s unexpectedly became a documentation requirement", route.method, route.path)
		}
	}
}
