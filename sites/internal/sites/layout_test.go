package sites

import (
	"os"
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

// R-QW9D-XLQT
func TestLayoutMoveRelocatesVisibilityDirectories(t *testing.T) {
	layout := NewLayout(t.TempDir())
	privateDir := layout.SiteDir(false, "blog")
	publicDir := layout.SiteDir(true, "blog")
	if err := os.MkdirAll(privateDir, 0o755); err != nil {
		t.Fatalf("mkdir private: %v", err)
	}
	privateFile := filepath.Join(privateDir, "index.html")
	if err := os.WriteFile(privateFile, []byte("hello"), 0o644); err != nil {
		t.Fatalf("write private file: %v", err)
	}

	if err := layout.Move("blog", true); err != nil {
		t.Fatalf("move public: %v", err)
	}
	publicFile := filepath.Join(publicDir, "index.html")
	if got, err := os.ReadFile(publicFile); err != nil || string(got) != "hello" {
		t.Fatalf("public file = %q, %v; want hello, nil", got, err)
	}
	if _, err := os.Stat(privateDir); !os.IsNotExist(err) {
		t.Fatalf("private dir after public move: want missing, got %v", err)
	}

	before, err := os.Stat(publicFile)
	if err != nil {
		t.Fatalf("stat public before no-op: %v", err)
	}
	if err := layout.Move("blog", true); err != nil {
		t.Fatalf("move already public: %v", err)
	}
	after, err := os.Stat(publicFile)
	if err != nil {
		t.Fatalf("stat public after no-op: %v", err)
	}
	if !after.ModTime().Equal(before.ModTime()) {
		t.Fatalf("no-op move changed file mod time: before %v after %v", before.ModTime(), after.ModTime())
	}

	if err := layout.Move("blog", false); err != nil {
		t.Fatalf("move private: %v", err)
	}
	if got, err := os.ReadFile(privateFile); err != nil || string(got) != "hello" {
		t.Fatalf("private file = %q, %v; want hello, nil", got, err)
	}
	if _, err := os.Stat(publicDir); !os.IsNotExist(err) {
		t.Fatalf("public dir after private move: want missing, got %v", err)
	}
	if err := layout.Move("empty", true); err != nil {
		t.Fatalf("move missing source: %v", err)
	}
}
