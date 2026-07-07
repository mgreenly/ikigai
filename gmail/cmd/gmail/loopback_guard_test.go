package main

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

func TestGoSourceDoesNotHardcodeLoopbackRegistryPorts(t *testing.T) {
	// R-9RMC-Y6QU
	moduleRoot := filepath.Clean(filepath.Join("..", ".."))
	needle := "127.0.0.1:" + "3"
	forbidden := regexp.MustCompile(regexp.QuoteMeta(needle) + `\d{3}`)

	var offenders []string
	err := filepath.WalkDir(moduleRoot, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() {
			if entry.Name() == "vendor" {
				return filepath.SkipDir
			}
			return nil
		}
		if entry.Name() == "loopback_guard_test.go" || !strings.HasSuffix(entry.Name(), ".go") {
			return nil
		}

		body, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if forbidden.Match(body) {
			offenders = append(offenders, filepath.ToSlash(path))
		}
		return nil
	})
	if err != nil {
		t.Fatalf("walk Go source under module root: %v", err)
	}
	if len(offenders) > 0 {
		t.Fatalf("Go source hardcoded loopback registry port(s):\n%s", strings.Join(offenders, "\n"))
	}
}
