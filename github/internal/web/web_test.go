package web

import (
	"net/http"
	"net/http/httptest"
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
