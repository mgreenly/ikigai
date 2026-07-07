package files

import (
	"crypto/md5"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"slices"
	"testing"
)

func TestConfinePathResolvesAndRejectsEscapes(t *testing.T) {
	root := t.TempDir()
	insideDir := filepath.Join(root, "inside")
	if err := os.Mkdir(insideDir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(insideDir, filepath.Join(root, "inside-link")); err != nil {
		t.Fatal(err)
	}
	outside := t.TempDir()
	if err := os.Symlink(outside, filepath.Join(root, "outside-link")); err != nil {
		t.Fatal(err)
	}

	got, err := ConfinePath(root, "inside-link/new.txt")
	if err != nil {
		t.Fatalf("ConfinePath inside symlink: %v", err)
	}
	want := filepath.Join(insideDir, "new.txt")
	if got != want {
		t.Fatalf("ConfinePath resolved path = %q, want %q", got, want)
	}
	absoluteInside := filepath.Join(root, "absolute.txt")
	got, err = ConfinePath(root, absoluteInside)
	if err != nil {
		t.Fatalf("ConfinePath absolute inside: %v", err)
	}
	if got != absoluteInside {
		t.Fatalf("ConfinePath absolute inside = %q, want %q", got, absoluteInside)
	}

	// R-027Y-BQ1I
	for _, path := range []string{"../escape.txt", filepath.Join(root, "outside-link", "escape.txt"), filepath.Join(outside, "escape.txt")} {
		_, err := ConfinePath(root, path)
		if !errors.Is(err, ErrEscapes) {
			t.Fatalf("ConfinePath(%q) error = %v, want ErrEscapes", path, err)
		}
	}
}

func TestWriteCreatesParentsTruncatesAndAppends(t *testing.T) {
	root := t.TempDir()

	// R-03FU-PHS7
	if err := Write(root, "nested/page.txt", "first", false); err != nil {
		t.Fatalf("Write create: %v", err)
	}
	if err := Write(root, "nested/page.txt", " second", true); err != nil {
		t.Fatalf("Write append: %v", err)
	}
	if err := Write(root, "nested/page.txt", "last", false); err != nil {
		t.Fatalf("Write truncate: %v", err)
	}
	got, err := os.ReadFile(filepath.Join(root, "nested/page.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != "last" {
		t.Fatalf("written content = %q, want truncate to last write", got)
	}
	if err := Write(root, "nested/page.txt", "+tail", true); err != nil {
		t.Fatalf("Write second append: %v", err)
	}
	got, err = os.ReadFile(filepath.Join(root, "nested/page.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != "last+tail" {
		t.Fatalf("appended content = %q, want last+tail", got)
	}
}

func TestReadWholeFileAndLineWindow(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "page.txt"), []byte("one\ntwo\nthree\nfour\n"), 0o644); err != nil {
		t.Fatal(err)
	}

	// R-04NR-39IW
	whole, err := Read(root, "page.txt", 0, 0)
	if err != nil {
		t.Fatalf("Read whole: %v", err)
	}
	if whole != "one\ntwo\nthree\nfour\n" {
		t.Fatalf("whole read = %q", whole)
	}
	window, err := Read(root, "page.txt", 2, 2)
	if err != nil {
		t.Fatalf("Read window: %v", err)
	}
	if window != "two\nthree\n" {
		t.Fatalf("window read = %q, want %q", window, "two\nthree\n")
	}
}

func TestEditReplacesFirstAllAndErrorsWithoutWrite(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "page.txt")
	if err := os.WriteFile(path, []byte("alpha beta beta"), 0o644); err != nil {
		t.Fatal(err)
	}

	// R-05VN-H19L
	replaced, err := Edit(root, "page.txt", "beta", "gamma", false)
	if err != nil {
		t.Fatalf("Edit first: %v", err)
	}
	if replaced != 1 {
		t.Fatalf("first replace count = %d, want 1", replaced)
	}
	got, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != "alpha gamma beta" {
		t.Fatalf("first edit content = %q", got)
	}
	replaced, err = Edit(root, "page.txt", "a", "A", true)
	if err != nil {
		t.Fatalf("Edit all: %v", err)
	}
	if replaced != 5 {
		t.Fatalf("replace all count = %d, want 5", replaced)
	}
	before, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := Edit(root, "page.txt", "missing", "x", false); err == nil {
		t.Fatal("Edit missing old string succeeded, want error")
	}
	after, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if string(after) != string(before) {
		t.Fatalf("missing edit wrote file: before %q after %q", before, after)
	}
}

func TestGlobReturnsSearchBaseRelativeMatches(t *testing.T) {
	root := t.TempDir()
	if err := os.MkdirAll(filepath.Join(root, "assets"), 0o755); err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"assets/a.css", "assets/b.css", "assets/c.txt"} {
		if err := os.WriteFile(filepath.Join(root, name), []byte(name), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.Symlink(t.TempDir(), filepath.Join(root, "assets", "outside")); err != nil {
		t.Fatal(err)
	}

	// R-073J-UT0A
	got, err := Glob(root, "*.css", "assets")
	if err != nil {
		t.Fatalf("Glob: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"a.css", "b.css"}) {
		t.Fatalf("Glob matches = %#v, want a.css and b.css relative to assets", got)
	}
}

func TestGlobRecursiveDoubleStarMatchesAllDepths(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-3ZP8-T0GP
	css, err := Glob(root, "**/*.css", "")
	if err != nil {
		t.Fatalf("Glob css: %v", err)
	}
	if !reflect.DeepEqual(css, []string{"a.css", "assets/css/style.css", "deep/a/b/c.css"}) {
		t.Fatalf("Glob recursive css = %#v", css)
	}
	html, err := Glob(root, "**/*.html", "")
	if err != nil {
		t.Fatalf("Glob html: %v", err)
	}
	if !reflect.DeepEqual(html, []string{"index.html"}) {
		t.Fatalf("Glob recursive html = %#v", html)
	}
	assets, err := Glob(root, "assets/**", "")
	if err != nil {
		t.Fatalf("Glob assets: %v", err)
	}
	if !reflect.DeepEqual(assets, []string{"assets/css/style.css", "assets/js/app.js"}) {
		t.Fatalf("Glob recursive assets = %#v", assets)
	}
	prefixedDeep, err := Glob(root, "deep/**/c.css", "")
	if err != nil {
		t.Fatalf("Glob prefixed deep recursive css: %v", err)
	}
	if !reflect.DeepEqual(prefixedDeep, []string{"deep/a/b/c.css"}) {
		t.Fatalf("Glob prefixed deep recursive css = %#v", prefixedDeep)
	}
	all, err := Glob(root, "**", "")
	if err != nil {
		t.Fatalf("Glob all: %v", err)
	}
	if !reflect.DeepEqual(all, []string{"a.css", "assets/css/style.css", "assets/js/app.js", "deep/a/b/c.css", "index.html"}) {
		t.Fatalf("Glob bare recursive = %#v", all)
	}
}

func TestGlobDoubleStarKeepsSegmentAndBaseBoundaries(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-40X5-6S7E
	baseCSS, err := Glob(root, "*.css", "")
	if err != nil {
		t.Fatalf("Glob base css: %v", err)
	}
	if !reflect.DeepEqual(baseCSS, []string{"a.css"}) {
		t.Fatalf("Glob base css = %#v", baseCSS)
	}
	scopedCSS, err := Glob(root, "**/*.css", "assets")
	if err != nil {
		t.Fatalf("Glob scoped css: %v", err)
	}
	if !reflect.DeepEqual(scopedCSS, []string{"css/style.css"}) {
		t.Fatalf("Glob scoped css = %#v", scopedCSS)
	}
	scopedAll, err := Glob(root, "**", "assets")
	if err != nil {
		t.Fatalf("Glob scoped all: %v", err)
	}
	if !reflect.DeepEqual(scopedAll, []string{"css/style.css", "js/app.js"}) {
		t.Fatalf("Glob scoped all = %#v", scopedAll)
	}
	missing, err := Glob(root, "**/*.md", "")
	if err != nil {
		t.Fatalf("Glob missing: %v", err)
	}
	if missing == nil || len(missing) != 0 {
		t.Fatalf("Glob missing = %#v, want empty non-nil slice", missing)
	}
}

func TestGlobScopedDoubleStarMatchesBaseAndNestedFiles(t *testing.T) {
	root := t.TempDir()
	for _, path := range []string{
		"assets/app.css",
		"assets/css/style.css",
		"assets/js/app.js",
		"outside.css",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	// R-40X5-6S7E
	recursive, err := Glob(root, "**/*.css", "assets")
	if err != nil {
		t.Fatalf("Glob scoped recursive css: %v", err)
	}
	if !reflect.DeepEqual(recursive, []string{"app.css", "css/style.css"}) {
		t.Fatalf("Glob scoped recursive css = %#v", recursive)
	}
	direct, err := Glob(root, "*.css", "assets")
	if err != nil {
		t.Fatalf("Glob scoped direct css: %v", err)
	}
	if !reflect.DeepEqual(direct, []string{"app.css"}) {
		t.Fatalf("Glob scoped direct css = %#v", direct)
	}
}

func TestGlobRecursiveDoubleStarReturnsSortedSlashRelativeMatches(t *testing.T) {
	root := t.TempDir()
	for _, path := range []string{
		"zeta.css",
		"assets/z.css",
		"assets/css/a.css",
		"alpha.css",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	got, err := Glob(root, "**/*.css", "")
	if err != nil {
		t.Fatalf("Glob recursive css: %v", err)
	}
	want := []string{"alpha.css", "assets/css/a.css", "assets/z.css", "zeta.css"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("Glob recursive sorted slash-relative css = %#v, want %#v", got, want)
	}
}

func TestGlobRecursiveDoubleStarPreservesNestedSearchBase(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-40X5-6S7E
	got, err := Glob(root, "**/*.css", "assets/css")
	if err != nil {
		t.Fatalf("Glob nested scoped css: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"style.css"}) {
		t.Fatalf("Glob nested scoped css = %#v", got)
	}
	if _, err := Glob(root, "../**/*.css", "assets/css"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob nested scoped escaping pattern error = %v, want ErrEscapes", err)
	}
}

func TestGlobRepeatedDoubleStarKeepsSingleBaseRelativeMatches(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-3ZP8-T0GP
	css, err := Glob(root, "**/**/*.css", "")
	if err != nil {
		t.Fatalf("Glob repeated recursive css: %v", err)
	}
	if !reflect.DeepEqual(css, []string{"a.css", "assets/css/style.css", "deep/a/b/c.css"}) {
		t.Fatalf("Glob repeated recursive css = %#v", css)
	}

	// R-40X5-6S7E
	scoped, err := Glob(root, "**/**", "assets")
	if err != nil {
		t.Fatalf("Glob repeated recursive scoped assets: %v", err)
	}
	if !reflect.DeepEqual(scoped, []string{"css/style.css", "js/app.js"}) {
		t.Fatalf("Glob repeated recursive scoped assets = %#v", scoped)
	}
}

func TestGlobAdjacentDoubleStarMatchesLiteralAtBaseDepth(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-3ZP8-T0GP
	html, err := Glob(root, "**/**/index.html", "")
	if err != nil {
		t.Fatalf("Glob adjacent recursive html: %v", err)
	}
	if !reflect.DeepEqual(html, []string{"index.html"}) {
		t.Fatalf("Glob adjacent recursive html = %#v", html)
	}

	// R-40X5-6S7E
	scoped, err := Glob(root, "**/**/style.css", "assets")
	if err != nil {
		t.Fatalf("Glob adjacent recursive scoped css: %v", err)
	}
	if !reflect.DeepEqual(scoped, []string{"css/style.css"}) {
		t.Fatalf("Glob adjacent recursive scoped css = %#v", scoped)
	}
}

func TestGlobDoubleStarMatchesZeroSegmentsWithinScopedBase(t *testing.T) {
	root := t.TempDir()
	for _, path := range []string{
		"index.html",
		"assets/index.html",
		"assets/nested/page.html",
		"assets/nested/style.css",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	// R-40X5-6S7E
	got, err := Glob(root, "**/*.html", "assets")
	if err != nil {
		t.Fatalf("Glob scoped recursive html: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"index.html", "nested/page.html"}) {
		t.Fatalf("Glob scoped recursive html = %#v", got)
	}
}

func TestGlobDoubleStarMatchesZeroSegmentsAfterPrefix(t *testing.T) {
	root := t.TempDir()
	for _, path := range []string{
		"assets/app.css",
		"assets/css/style.css",
		"assets/js/app.js",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	prefixed, err := Glob(root, "assets/**/*.css", "")
	if err != nil {
		t.Fatalf("Glob prefixed recursive css: %v", err)
	}
	if !reflect.DeepEqual(prefixed, []string{"assets/app.css", "assets/css/style.css"}) {
		t.Fatalf("Glob prefixed recursive css = %#v", prefixed)
	}

	// R-40X5-6S7E
	scoped, err := Glob(root, "**/*.css", "assets")
	if err != nil {
		t.Fatalf("Glob scoped recursive css: %v", err)
	}
	if !reflect.DeepEqual(scoped, []string{"app.css", "css/style.css"}) {
		t.Fatalf("Glob scoped recursive css = %#v", scoped)
	}
}

func TestGlobTrailingDoubleStarIncludesDirectFilesUnderPrefix(t *testing.T) {
	root := t.TempDir()
	for _, path := range []string{
		"assets/app.css",
		"assets/css/style.css",
		"index.css",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	recursive, err := Glob(root, "assets/**", "")
	if err != nil {
		t.Fatalf("Glob trailing recursive prefix: %v", err)
	}
	if !reflect.DeepEqual(recursive, []string{"assets/app.css", "assets/css/style.css"}) {
		t.Fatalf("Glob trailing recursive prefix = %#v", recursive)
	}

	// R-40X5-6S7E
	direct, err := Glob(root, "assets/*", "")
	if err != nil {
		t.Fatalf("Glob direct prefix: %v", err)
	}
	if !reflect.DeepEqual(direct, []string{"assets/app.css"}) {
		t.Fatalf("Glob direct prefix = %#v", direct)
	}
}

func TestGlobDoubleStarMatchesZeroSegmentsBetweenLiterals(t *testing.T) {
	root := t.TempDir()
	for _, path := range []string{
		"assets/style.css",
		"assets/nested/style.css",
		"assets/nested/app.css",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	got, err := Glob(root, "assets/**/style.css", "")
	if err != nil {
		t.Fatalf("Glob recursive literal css: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"assets/nested/style.css", "assets/style.css"}) {
		t.Fatalf("Glob recursive literal css = %#v", got)
	}
}

func TestGlobSegmentPatternsUseFilepathMatchSemantics(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-40X5-6S7E
	question, err := Glob(root, "assets/??/*.js", "")
	if err != nil {
		t.Fatalf("Glob question segment: %v", err)
	}
	if !reflect.DeepEqual(question, []string{"assets/js/app.js"}) {
		t.Fatalf("Glob question segment = %#v", question)
	}
	class, err := Glob(root, "[ai]*.html", "")
	if err != nil {
		t.Fatalf("Glob class segment: %v", err)
	}
	if !reflect.DeepEqual(class, []string{"index.html"}) {
		t.Fatalf("Glob class segment = %#v", class)
	}
	noCross, err := Glob(root, "assets/?/*.js", "")
	if err != nil {
		t.Fatalf("Glob non-crossing question segment: %v", err)
	}
	if noCross == nil || len(noCross) != 0 {
		t.Fatalf("Glob non-crossing question segment = %#v, want empty non-nil slice", noCross)
	}
}

func TestGlobRecursiveDoubleStarCombinesWithCharacterClasses(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-3ZP8-T0GP
	got, err := Glob(root, "**/[cs]*.css", "")
	if err != nil {
		t.Fatalf("Glob recursive character class css: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"assets/css/style.css", "deep/a/b/c.css"}) {
		t.Fatalf("Glob recursive character class css = %#v", got)
	}

	// R-40X5-6S7E
	direct, err := Glob(root, "[cs]*.css", "")
	if err != nil {
		t.Fatalf("Glob direct character class css: %v", err)
	}
	if direct == nil || len(direct) != 0 {
		t.Fatalf("Glob direct character class css = %#v, want empty non-nil slice", direct)
	}
}

func TestGlobRecursiveDoubleStarKeepsFilenameWildcardsWithinSegments(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-3ZP8-T0GP
	recursive, err := Glob(root, "**/*.c?s", "")
	if err != nil {
		t.Fatalf("Glob recursive css wildcard: %v", err)
	}
	if !reflect.DeepEqual(recursive, []string{"a.css", "assets/css/style.css", "deep/a/b/c.css"}) {
		t.Fatalf("Glob recursive css wildcard = %#v", recursive)
	}

	// R-40X5-6S7E
	direct, err := Glob(root, "*.c?s", "")
	if err != nil {
		t.Fatalf("Glob direct css wildcard: %v", err)
	}
	if !reflect.DeepEqual(direct, []string{"a.css"}) {
		t.Fatalf("Glob direct css wildcard = %#v", direct)
	}
}

func TestGlobPrefixedStarDoesNotCrossSlash(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-40X5-6S7E
	direct, err := Glob(root, "assets/*.js", "")
	if err != nil {
		t.Fatalf("Glob direct prefixed js: %v", err)
	}
	if direct == nil || len(direct) != 0 {
		t.Fatalf("Glob direct prefixed js = %#v, want empty non-nil slice", direct)
	}

	// R-3ZP8-T0GP
	recursive, err := Glob(root, "assets/**/*.js", "")
	if err != nil {
		t.Fatalf("Glob recursive prefixed js: %v", err)
	}
	if !reflect.DeepEqual(recursive, []string{"assets/js/app.js"}) {
		t.Fatalf("Glob recursive prefixed js = %#v", recursive)
	}
}

func TestGlobRecursiveDoubleStarCombinesWithFilenameWildcards(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-3ZP8-T0GP
	app, err := Glob(root, "**/app.*", "")
	if err != nil {
		t.Fatalf("Glob recursive app wildcard: %v", err)
	}
	if !reflect.DeepEqual(app, []string{"assets/js/app.js"}) {
		t.Fatalf("Glob recursive app wildcard = %#v", app)
	}

	// R-40X5-6S7E
	css, err := Glob(root, "**/c.?ss", "")
	if err != nil {
		t.Fatalf("Glob recursive question wildcard: %v", err)
	}
	if !reflect.DeepEqual(css, []string{"deep/a/b/c.css"}) {
		t.Fatalf("Glob recursive question wildcard = %#v", css)
	}
}

func TestGlobRecursiveDoubleStarDoesNotFollowSymlinks(t *testing.T) {
	root := globRecursiveFixture(t)
	outside := t.TempDir()
	if err := os.WriteFile(filepath.Join(outside, "outside.css"), []byte("outside"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(outside, "outside.css"), filepath.Join(root, "assets", "css", "linked.css")); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(outside, filepath.Join(root, "assets", "linked-dir")); err != nil {
		t.Fatal(err)
	}

	// R-3ZP8-T0GP
	got, err := Glob(root, "**/*.css", "")
	if err != nil {
		t.Fatalf("Glob recursive css: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"a.css", "assets/css/style.css", "deep/a/b/c.css"}) {
		t.Fatalf("Glob recursive css with symlinks = %#v", got)
	}

	scoped, err := Glob(root, "**/*.css", "assets")
	if err != nil {
		t.Fatalf("Glob scoped recursive css: %v", err)
	}
	if !reflect.DeepEqual(scoped, []string{"css/style.css"}) {
		t.Fatalf("Glob scoped recursive css with symlinks = %#v", scoped)
	}
}

func TestGlobRecursiveDoubleStarReturnsOnlyRegularFiles(t *testing.T) {
	root := t.TempDir()
	for _, dir := range []string{"assets/fake.css", "assets/css"} {
		if err := os.MkdirAll(filepath.Join(root, dir), 0o755); err != nil {
			t.Fatal(err)
		}
	}
	for _, path := range []string{"a.css", "assets/css/style.css"} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	recursive, err := Glob(root, "**/*.css", "")
	if err != nil {
		t.Fatalf("Glob recursive regular files: %v", err)
	}
	if !reflect.DeepEqual(recursive, []string{"a.css", "assets/css/style.css"}) {
		t.Fatalf("Glob recursive regular files = %#v", recursive)
	}

	// R-40X5-6S7E
	scopedDirect, err := Glob(root, "*.css", "assets")
	if err != nil {
		t.Fatalf("Glob scoped direct regular files: %v", err)
	}
	if scopedDirect == nil || len(scopedDirect) != 0 {
		t.Fatalf("Glob scoped direct regular files = %#v, want empty non-nil slice", scopedDirect)
	}
}

func TestGlobLiteralSymlinkPatternIsNotFollowed(t *testing.T) {
	root := t.TempDir()
	if err := os.MkdirAll(filepath.Join(root, "real"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "real", "style.css"), []byte("body{}"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(root, "real"), filepath.Join(root, "linked")); err != nil {
		t.Fatal(err)
	}

	// R-3ZP8-T0GP
	got, err := Glob(root, "linked/*.css", "")
	if err != nil {
		t.Fatalf("Glob literal symlink css: %v", err)
	}
	if got == nil || len(got) != 0 {
		t.Fatalf("Glob literal symlink css = %#v, want empty non-nil slice", got)
	}

	recursive, err := Glob(root, "**/*.css", "")
	if err != nil {
		t.Fatalf("Glob recursive css: %v", err)
	}
	if !reflect.DeepEqual(recursive, []string{"real/style.css"}) {
		t.Fatalf("Glob recursive css = %#v", recursive)
	}
}

func TestGlobRejectsEscapingPatterns(t *testing.T) {
	root := globRecursiveFixture(t)

	if _, err := Glob(root, "../*.css", "assets"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob escaping pattern error = %v, want ErrEscapes", err)
	}
	// R-40X5-6S7E
	if _, err := Glob(root, filepath.Join(root, "a.css"), "assets"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob absolute pattern outside search base error = %v, want ErrEscapes", err)
	}
	if _, err := Glob(root, "**/*.css", "../"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob escaping path error = %v, want ErrEscapes", err)
	}
}

func TestGlobRejectsEscapingSearchBasePath(t *testing.T) {
	root := globRecursiveFixture(t)
	outside := t.TempDir()
	if err := os.Symlink(outside, filepath.Join(root, "outside")); err != nil {
		t.Fatal(err)
	}

	// R-40X5-6S7E
	if _, err := Glob(root, "**/*.css", "../assets"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob parent-escaping search base error = %v, want ErrEscapes", err)
	}
	if _, err := Glob(root, "**/*.css", "outside"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob symlink-escaping search base error = %v, want ErrEscapes", err)
	}
}

func TestGlobAbsolutePatternInsideSearchBaseReturnsBaseRelativeMatches(t *testing.T) {
	root := globRecursiveFixture(t)

	// R-40X5-6S7E
	got, err := Glob(root, filepath.Join(root, "assets", "**", "*.css"), "assets")
	if err != nil {
		t.Fatalf("Glob absolute pattern inside search base: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"css/style.css"}) {
		t.Fatalf("Glob absolute pattern inside search base = %#v", got)
	}
}

func TestGlobAbsoluteRecursivePatternPreservesScopedBaseLevelMatches(t *testing.T) {
	root := t.TempDir()
	for _, path := range []string{
		"assets/app.css",
		"assets/css/style.css",
		"outside.css",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}

	// R-3ZP8-T0GP
	// R-40X5-6S7E
	got, err := Glob(root, filepath.Join(root, "assets", "**", "*.css"), "assets")
	if err != nil {
		t.Fatalf("Glob absolute scoped recursive css: %v", err)
	}
	if !reflect.DeepEqual(got, []string{"app.css", "css/style.css"}) {
		t.Fatalf("Glob absolute scoped recursive css = %#v", got)
	}
}

func TestGlobRejectsPatternsResolvingOutsideSearchBase(t *testing.T) {
	root := globRecursiveFixture(t)
	outside := t.TempDir()
	if err := os.WriteFile(filepath.Join(outside, "leak.css"), []byte("leak"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(outside, filepath.Join(root, "assets", "outside")); err != nil {
		t.Fatal(err)
	}

	// R-40X5-6S7E
	if _, err := Glob(root, "outside/*.css", "assets"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob symlinked escaping pattern error = %v, want ErrEscapes", err)
	}
	if _, err := Glob(root, "outside/**/*.css", "assets"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Glob recursive symlinked escaping pattern error = %v, want ErrEscapes", err)
	}
}

func TestGlobRejectsMalformedPatternWithoutWalkingMatches(t *testing.T) {
	root := t.TempDir()

	if _, err := Glob(root, "[", ""); err == nil {
		t.Fatal("Glob malformed pattern succeeded, want error")
	}
}

func TestGlobReturnsEmptyForNonDirectorySearchBase(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "file.txt"), []byte("text"), 0o644); err != nil {
		t.Fatal(err)
	}

	// R-40X5-6S7E
	missing, err := Glob(root, "**/*.css", "missing")
	if err != nil {
		t.Fatalf("Glob missing base: %v", err)
	}
	if missing == nil || len(missing) != 0 {
		t.Fatalf("Glob missing base = %#v, want empty non-nil slice", missing)
	}
	fileBase, err := Glob(root, "**", "file.txt")
	if err != nil {
		t.Fatalf("Glob file base: %v", err)
	}
	if fileBase == nil || len(fileBase) != 0 {
		t.Fatalf("Glob file base = %#v, want empty non-nil slice", fileBase)
	}
}

func TestGrepReturnsTypedRootRelativeMatches(t *testing.T) {
	root := t.TempDir()
	if err := os.MkdirAll(filepath.Join(root, "content"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "content", "page.txt"), []byte("first\nneedle here\nlast\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "content", "skip.md"), []byte("needle elsewhere\n"), 0o644); err != nil {
		t.Fatal(err)
	}

	// R-08BG-8KQZ
	got, err := Grep(root, "needle", "content", "*.txt")
	if err != nil {
		t.Fatalf("Grep: %v", err)
	}
	want := []Match{{Path: "content/page.txt", Line: 2, Text: "needle here"}}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("Grep matches = %#v, want %#v", got, want)
	}

	// R-0D71-RNPR
	var typed []Match = got
	if len(typed) != 1 || typed[0].Text == "" {
		t.Fatalf("Grep returned unusable typed matches: %#v", typed)
	}
}

func TestListReturnsRootRelativeRegularFilesWithHashesAndScope(t *testing.T) {
	root := t.TempDir()
	files := map[string]string{
		"index.html":     "<h1>hi</h1>",
		"assets/site.js": "console.log('hi')",
	}
	for path, content := range files {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(content), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.Mkdir(filepath.Join(root, "empty"), 0o755); err != nil {
		t.Fatal(err)
	}

	// R-09JC-MCHO
	got, err := List(root, "")
	if err != nil {
		t.Fatalf("List root: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("List root returned %d files: %#v", len(got), got)
	}
	byPath := map[string]FileInfo{}
	for _, file := range got {
		byPath[file.Path] = file
	}
	for path, content := range files {
		info, ok := byPath[path]
		if !ok {
			t.Fatalf("List missing %s in %#v", path, got)
		}
		if info.Size != int64(len(content)) {
			t.Fatalf("%s size = %d, want %d", path, info.Size, len(content))
		}
		if info.Md5 != fmt.Sprintf("%x", md5.Sum([]byte(content))) {
			t.Fatalf("%s md5 = %s", path, info.Md5)
		}
	}
	scoped, err := List(root, "assets")
	if err != nil {
		t.Fatalf("List scoped: %v", err)
	}
	if len(scoped) != 1 || scoped[0].Path != "assets/site.js" {
		t.Fatalf("scoped List = %#v, want assets/site.js only", scoped)
	}

	// R-0D71-RNPR
	var typed []FileInfo = scoped
	if len(typed) != 1 || typed[0].Md5 == "" {
		t.Fatalf("List returned unusable typed file info: %#v", typed)
	}
}

func TestMkdirCreatesNestedDirectoriesAndRejectsEscapes(t *testing.T) {
	root := t.TempDir()

	// R-0AR9-048D
	if err := Mkdir(root, "a/b/c"); err != nil {
		t.Fatalf("Mkdir nested: %v", err)
	}
	if info, err := os.Stat(filepath.Join(root, "a", "b", "c")); err != nil || !info.IsDir() {
		t.Fatalf("nested directory not created: info=%v err=%v", info, err)
	}
	if err := Mkdir(root, "../escape"); !errors.Is(err, ErrEscapes) {
		t.Fatalf("Mkdir escape error = %v, want ErrEscapes", err)
	}
}

func TestOperationsShareErrEscapesConfinement(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "page.txt"), []byte("needle\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	escape := t.TempDir()
	if err := os.Symlink(escape, filepath.Join(root, "escape")); err != nil {
		t.Fatal(err)
	}

	// R-0EEY-5FGG
	checks := []struct {
		name string
		err  error
	}{
		{"Read", errOnly(Read(root, "escape/read.txt", 0, 0))},
		{"Write", Write(root, "escape/write.txt", "x", false)},
		{"Edit", editErr(Edit(root, "escape/edit.txt", "x", "y", false))},
		{"Glob", errOnlyStrings(Glob(root, "*.txt", "escape"))},
		{"Grep", errOnlyMatches(Grep(root, "needle", "escape", ""))},
		{"List", errOnlyFileInfos(List(root, "escape"))},
		{"Mkdir", Mkdir(root, "escape/dir")},
	}
	for _, check := range checks {
		if !errors.Is(check.err, ErrEscapes) {
			t.Fatalf("%s escape error = %v, want ErrEscapes", check.name, check.err)
		}
	}
}

func TestGlobReturnsTypedStrings(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "a.txt"), []byte("a"), 0o644); err != nil {
		t.Fatal(err)
	}

	// R-0D71-RNPR
	got, err := Glob(root, "*.txt", "")
	if err != nil {
		t.Fatalf("Glob: %v", err)
	}
	var typed []string = got
	if !slices.Equal(typed, []string{"a.txt"}) {
		t.Fatalf("Glob typed strings = %#v, want a.txt", typed)
	}
}

func errOnly(_ string, err error) error {
	return err
}

func errOnlyStrings(_ []string, err error) error {
	return err
}

func errOnlyMatches(_ []Match, err error) error {
	return err
}

func errOnlyFileInfos(_ []FileInfo, err error) error {
	return err
}

func editErr(_ int, err error) error {
	return err
}

func globRecursiveFixture(t *testing.T) string {
	t.Helper()
	root := t.TempDir()
	for _, path := range []string{
		"a.css",
		"index.html",
		"assets/css/style.css",
		"assets/js/app.js",
		"deep/a/b/c.css",
	} {
		if err := os.MkdirAll(filepath.Dir(filepath.Join(root, path)), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, path), []byte(path), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	return root
}
