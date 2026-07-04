package main

import (
	"os"
	"path/filepath"
	"regexp"
	"testing"
)

func TestGoSourceDoesNotContainBareRegistryLoopbackLiterals(t *testing.T) {
	// R-RGNL-4E5P
	moduleRoot := filepath.Join("..", "..")
	self := filepath.Clean(filepath.Join(moduleRoot, "cmd", "notify", "loopback_guard_test.go"))
	needle := "127.0.0.1:" + "30"
	forbidden := regexp.MustCompile(regexp.QuoteMeta(needle) + `[0-9]{2}`)

	err := filepath.WalkDir(moduleRoot, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() || filepath.Ext(path) != ".go" || filepath.Clean(path) == self {
			return nil
		}

		src, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if match := forbidden.Find(src); match != nil {
			t.Errorf("%s contains bare loopback registry literal %q", path, match)
		}
		return nil
	})
	if err != nil {
		t.Fatalf("scan Go source under %s: %v", moduleRoot, err)
	}
}
