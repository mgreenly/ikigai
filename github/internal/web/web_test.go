package web

import (
	"io/fs"
	"net/http"
	"net/http/httptest"
	"os"
	"regexp"
	"sort"
	"strings"
	"testing"
)

func TestLandingHandlerReturnsServiceAndVersion(t *testing.T) {
	// R-EVZ3-VXJZ
	rec := httptest.NewRecorder()
	LandingHandler("github", "v-test").ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	if got, want := rec.Header().Get("Content-Type"), "text/html; charset=utf-8"; got != want {
		t.Fatalf("Content-Type = %q, want %q", got, want)
	}
	body := rec.Body.String()
	for _, want := range []string{"github", "v-test"} {
		if !strings.Contains(body, want) {
			t.Fatalf("body missing %q:\n%s", want, body)
		}
	}
}

func TestLandingHandlerRendersCanonicalSuiteLayout(t *testing.T) {
	// R-7NJI-UTHM
	body := renderLanding(t, "github", "v-test")

	for _, want := range []string{
		"<title>github · github</title>",
		"GitHub connector",
		"Github connects the suite to GitHub through one shared GitHub App and exposes repository, pull request, and issue actions as MCP tools.",
		"<dt>Service</dt>",
		"<dd>github</dd>",
		"<dt>Version</dt>",
		"<dd>v-test</dd>",
		"<dt>API</dt>",
		"<code>POST /mcp</code>",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing body missing %q:\n%s", want, body)
		}
	}
}

func TestLandingHandlerEscapesInjectedServiceAndVersion(t *testing.T) {
	// R-7ORF-8L8B
	body := renderLanding(t, "<script>alert(1)</script>", "v&test")

	for _, want := range []string{
		"&lt;script&gt;alert(1)&lt;/script&gt;",
		"v&amp;test",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("landing body missing escaped value %q:\n%s", want, body)
		}
	}
	for _, raw := range []string{
		"<script>alert(1)</script>",
		"v&test",
	} {
		if strings.Contains(body, raw) {
			t.Fatalf("landing body contains raw value %q:\n%s", raw, body)
		}
	}
}

func TestLandingHandlerPlacesHomeLinkFirstInMain(t *testing.T) {
	// R-7PZB-MCZ0
	body := renderLanding(t, "github", "v-test")
	match := regexp.MustCompile(`(?s)<main>\s*(<[^>]+>[^<]*</a>)`).FindStringSubmatch(body)
	if match == nil {
		t.Fatalf("landing body missing first element inside main:\n%s", body)
	}
	if got, want := match[1], `<a class="home" href="/">Home</a>`; got != want {
		t.Fatalf("first element inside main = %q, want %q", got, want)
	}
}

func TestStaticHandlerServesTokensFontsAndRejectsMissingAssets(t *testing.T) {
	// R-EX70-9PAO
	tests := []struct {
		name        string
		path        string
		wantStatus  int
		wantType    string
		wantContent string
	}{
		{
			name:        "tokens css",
			path:        "/tokens.css",
			wantStatus:  http.StatusOK,
			wantType:    "text/css; charset=utf-8",
			wantContent: "--font-display",
		},
		{
			name:       "woff2 font",
			path:       "/fonts/space-grotesk.woff2",
			wantStatus: http.StatusOK,
			wantType:   "font/woff2",
		},
		{
			name:       "traversal",
			path:       "/../tokens.css",
			wantStatus: http.StatusNotFound,
		},
		{
			name:       "unknown",
			path:       "/missing.css",
			wantStatus: http.StatusNotFound,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			rec := httptest.NewRecorder()
			StaticHandler().ServeHTTP(rec, httptest.NewRequest(http.MethodGet, tt.path, nil))

			if rec.Code != tt.wantStatus {
				t.Fatalf("status = %d, want %d", rec.Code, tt.wantStatus)
			}
			if tt.wantType != "" {
				if got := rec.Header().Get("Content-Type"); got != tt.wantType {
					t.Fatalf("Content-Type = %q, want %q", got, tt.wantType)
				}
			}
			if tt.wantContent != "" && !strings.Contains(rec.Body.String(), tt.wantContent) {
				t.Fatalf("body missing %q", tt.wantContent)
			}
		})
	}
}

func TestLandingHandlerReferencesOnlyDefinedTokens(t *testing.T) {
	// R-WYSR-NPL3
	body := renderLanding(t, "github", "v-test")
	tokensCSS, err := fs.ReadFile(staticAssets, "tokens.css")
	if err != nil {
		t.Fatalf("read embedded tokens.css: %v", err)
	}

	defined := make(map[string]bool)
	for _, match := range regexp.MustCompile(`--([A-Za-z0-9-]+)\s*:`).FindAllStringSubmatch(string(tokensCSS), -1) {
		defined[match[1]] = true
	}

	referenced := make(map[string]bool)
	for _, match := range regexp.MustCompile(`var\(--([A-Za-z0-9-]+)\)`).FindAllStringSubmatch(body, -1) {
		referenced[match[1]] = true
	}

	var undefined []string
	for token := range referenced {
		if !defined[token] {
			undefined = append(undefined, token)
		}
	}
	sort.Strings(undefined)
	if len(undefined) > 0 {
		t.Fatalf("landing page references undefined CSS tokens: %v", undefined)
	}
}

func TestLandingHandlerMatchesGoldenCanonicalLayout(t *testing.T) {
	// R-X00O-1HBS
	got := renderLanding(t, "github", "v-test")
	want, err := os.ReadFile("testdata/landing.golden.html")
	if err != nil {
		t.Fatalf("read golden landing fixture: %v", err)
	}
	if got != string(want) {
		t.Fatalf("landing render does not match golden fixture\n--- got ---\n%s\n--- want ---\n%s", got, string(want))
	}
}

func renderLanding(t *testing.T, service, version string) string {
	t.Helper()

	rec := httptest.NewRecorder()
	LandingHandler(service, version).ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/", nil))

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rec.Code, http.StatusOK)
	}
	return rec.Body.String()
}
