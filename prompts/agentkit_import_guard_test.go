package prompts

import (
	"go/parser"
	"go/token"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"

	"golang.org/x/mod/modfile"
)

func TestAgentkitDependencyUsesPublishedModule(t *testing.T) {
	data, err := os.ReadFile("go.mod")
	if err != nil {
		t.Fatal(err)
	}
	mod, err := modfile.Parse("go.mod", data, nil)
	if err != nil {
		t.Fatal(err)
	}

	const published = "github.com/ikigenba/agentkit"
	const wantVersion = "v0.1.1"

	var found bool
	for _, req := range mod.Require {
		switch req.Mod.Path {
		case published:
			found = true
			if req.Mod.Version != wantVersion {
				t.Fatalf("%s version = %q, want %q", published, req.Mod.Version, wantVersion)
			}
		case "agentkit":
			t.Fatalf("go.mod still requires local module path %q", req.Mod.Path)
		}
	}
	if !found {
		t.Fatalf("go.mod does not require %s", published)
	}

	for _, rep := range mod.Replace {
		if rep.Old.Path == "agentkit" {
			t.Fatalf("go.mod still replaces local module path %q", rep.Old.Path)
		}
		if rep.New.Path == "../agentkit" || rep.New.Path == "./agentkit" {
			t.Fatalf("go.mod still points at local agentkit path %q", rep.New.Path)
		}
	}
}

func TestAgentkitImportsUsePublishedPath(t *testing.T) {
	err := filepath.WalkDir(".", func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			switch d.Name() {
			case ".git", "vendor":
				return filepath.SkipDir
			}
			return nil
		}
		if !strings.HasSuffix(path, ".go") {
			return nil
		}

		file, err := parser.ParseFile(token.NewFileSet(), path, nil, parser.ImportsOnly)
		if err != nil {
			return err
		}
		for _, spec := range file.Imports {
			importPath, err := strconv.Unquote(spec.Path.Value)
			if err != nil {
				return err
			}
			if importPath == "agentkit" || strings.HasPrefix(importPath, "agentkit/") {
				t.Fatalf("%s imports local agentkit path %q", path, importPath)
			}
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
}
