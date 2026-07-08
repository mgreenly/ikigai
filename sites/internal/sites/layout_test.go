package sites

import (
	"path/filepath"
	"testing"
)

// R-QV1H-JU04
func TestLayoutSiteDirAndBaseUseVisibilitySegments(t *testing.T) {
	layout := NewLayout(filepath.Join("tmp", "sites-root"))

	if got, want := layout.SiteBase(true), filepath.Join(layout.root(), PublicSeg); got != want {
		t.Fatalf("public SiteBase = %q, want %q", got, want)
	}
	if got, want := layout.SiteBase(false), filepath.Join(layout.root(), PrivateSeg); got != want {
		t.Fatalf("private SiteBase = %q, want %q", got, want)
	}
	if got, want := layout.SiteDir(true, "blog"), filepath.Join(layout.root(), PublicSeg, "blog"); got != want {
		t.Fatalf("public SiteDir = %q, want %q", got, want)
	}
	if got, want := layout.SiteDir(false, "blog"), filepath.Join(layout.root(), PrivateSeg, "blog"); got != want {
		t.Fatalf("private SiteDir = %q, want %q", got, want)
	}
}
