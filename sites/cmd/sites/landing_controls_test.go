package main

import (
	"net/http/httptest"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

// R-NKTP-317P
func TestWWWLandingInteractiveControlsUseStyledClasses(t *testing.T) {
	rec := httptest.NewRecorder()
	if err := loadWWW(t).Render(rec, "landing.html", landingView{
		Service: "sites",
		Version: "control-style-test",
		Sites:   []siteRow{{Slug: "atlas"}},
	}); err != nil {
		t.Fatalf("render landing.html with controls: %v", err)
	}
	stylesheet := rec.Body.String()
	tokens, err := os.ReadFile(filepath.Join(wwwRoot(t), "static", "tokens.css"))
	if err != nil {
		t.Fatalf("read linked token stylesheet: %v", err)
	}
	stylesheet += string(tokens)

	controls := []struct {
		element string
		id      string
	}{
		{element: "input", id: "site-search"},
		{element: "button", id: "site-clear"},
		{element: "button", id: "pager-prev"},
		{element: "button", id: "pager-next"},
	}
	for _, control := range controls {
		tag := regexp.MustCompile(`<` + control.element + `\b[^>]*\bid="` + control.id + `"[^>]*>`).FindString(rec.Body.String())
		if tag == "" {
			t.Fatalf("landing HTML missing %s control %q:\n%s", control.element, control.id, rec.Body.String())
		}
		classMatch := regexp.MustCompile(`\bclass="([^"]+)"`).FindStringSubmatch(tag)
		if len(classMatch) != 2 || strings.TrimSpace(classMatch[1]) == "" {
			t.Fatalf("landing control %q has no styling class: %s", control.id, tag)
		}
		for _, class := range strings.Fields(classMatch[1]) {
			selector := regexp.MustCompile(`\.` + regexp.QuoteMeta(class) + `(?:[[:space:]:.{,#\[]|$)`)
			if !selector.MatchString(stylesheet) {
				t.Fatalf("landing control %q class %q has no stylesheet rule", control.id, class)
			}
		}
	}
}
