package main

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

func TestNoHardcodedReposLoopbackPortInGoSource(t *testing.T) {
	// R-EISY-2LYZ
	repoRoot := filepath.Clean(filepath.Join("..", ".."))
	barePort := regexp.MustCompile(`(^|[^0-9])3007([^0-9]|$)`)
	err := filepath.WalkDir(repoRoot, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(repoRoot, path)
		if err != nil {
			return err
		}
		if entry.IsDir() {
			if rel == "project" || strings.HasPrefix(rel, "project"+string(filepath.Separator)) {
				return filepath.SkipDir
			}
			return nil
		}
		if filepath.Ext(path) != ".go" || strings.HasSuffix(path, "_test.go") {
			return nil
		}
		source, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if strings.Contains(string(source), "127.0.0.1:30") {
			t.Errorf("%s contains hardcoded loopback service port prefix", rel)
		}
		if barePort.Match(source) {
			t.Errorf("%s contains bare repos service port literal", rel)
		}
		return nil
	})
	if err != nil {
		t.Fatalf("scan Go source: %v", err)
	}
}
