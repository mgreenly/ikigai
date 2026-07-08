package prompts

import (
	"os"
	"strings"
	"testing"

	"golang.org/x/mod/modfile"
)

func TestModuleAgentkitDependencyIsPublishedOnly(t *testing.T) {
	const (
		published = "github.com/ikigenba/agentkit"
		version   = "v0.2.0"
	)

	data, err := os.ReadFile("go.mod")
	if err != nil {
		t.Fatal(err)
	}
	mod, err := modfile.Parse("go.mod", data, nil)
	if err != nil {
		t.Fatal(err)
	}

	var found bool
	for _, req := range mod.Require {
		if req.Mod.Path == "agentkit" {
			t.Fatalf("go.mod requires deprecated local module path %q", req.Mod.Path)
		}
		if req.Mod.Path != published {
			continue
		}
		found = true
		if req.Mod.Version != version {
			t.Fatalf("%s version = %q, want %q", published, req.Mod.Version, version)
		}
	}
	if !found {
		t.Fatalf("go.mod does not require %s", published)
	}

	for _, rep := range mod.Replace {
		if rep.Old.Path == "agentkit" {
			t.Fatalf("go.mod replaces deprecated local module path %q", rep.Old.Path)
		}
		if rep.New.Path == "./agentkit" || rep.New.Path == "../agentkit" {
			t.Fatalf("go.mod points at local agentkit path %q", rep.New.Path)
		}
	}

	sum, err := os.ReadFile("go.sum")
	if err != nil {
		t.Fatal(err)
	}
	sumText := string(sum)
	for _, legacy := range []string{
		"agentkit v0.0.0",
		published + " v0.1.0 ",
		published + " v0.1.0/go.mod ",
		published + " v0.1.1 ",
		published + " v0.1.1/go.mod ",
	} {
		if strings.Contains(sumText, legacy) {
			t.Fatalf("go.sum contains legacy agentkit entry %q", legacy)
		}
	}
	if !strings.Contains(sumText, published+" ") {
		return
	}
	for _, want := range []string{
		published + " " + version + " ",
		published + " " + version + "/go.mod ",
	} {
		if !strings.Contains(sumText, want) {
			t.Fatalf("go.sum is missing %q", want)
		}
	}
}
