package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

func TestModuleWiringPinsPublishedAgentkitWithoutModuleReplace(t *testing.T) {
	// R-MWBI-4JY7
	goMod, err := os.ReadFile(filepath.Join("..", "..", "go.mod"))
	if err != nil {
		t.Fatalf("read go.mod: %v", err)
	}
	modText := string(goMod)
	if !regexp.MustCompile(`(?m)^\s*github\.com/ikigenba/agentkit\s+v[0-9]`).MatchString(modText) {
		t.Fatalf("go.mod does not have a versioned github.com/ikigenba/agentkit require:\n%s", modText)
	}
	if strings.Contains(modText, "replace github.com/ikigenba/agentkit") {
		t.Fatalf("go.mod must not replace published agentkit:\n%s", modText)
	}
	replaces := regexp.MustCompile(`(?m)^\s*replace\s+([^\s]+)\s+=>\s+([^\s]+)`).FindAllStringSubmatch(modText, -1)
	got := map[string]string{}
	for _, match := range replaces {
		got[match[1]] = match[2]
	}
	want := map[string]string{"appkit": "../appkit", "eventplane": "../eventplane"}
	if len(got) != len(want) {
		t.Fatalf("go.mod replace count = %d, want %d: %#v", len(got), len(want), got)
	}
	for module, path := range want {
		if got[module] != path {
			t.Fatalf("replace %s = %q, want %q; all replaces: %#v", module, got[module], path, got)
		}
	}

	goWork, err := os.ReadFile(filepath.Join("..", "..", "..", "go.work"))
	if err != nil {
		t.Fatalf("read ../go.work: %v", err)
	}
	workText := string(goWork)
	if !strings.Contains(workText, "./wiki") {
		t.Fatalf("../go.work does not include ./wiki:\n%s", goWork)
	}
	if !strings.Contains(workText, "replace github.com/ikigenba/agentkit => /home/mgreenly/projects/agentkit") {
		t.Fatalf("../go.work does not keep the local agentkit replacement:\n%s", goWork)
	}
}

func TestProductionShapedBuildCompilesWithPublishedAgentkit(t *testing.T) {
	// R-MV3L-QS7I
	goMod, err := os.ReadFile(filepath.Join("..", "..", "go.mod"))
	if err != nil {
		t.Fatalf("read go.mod: %v", err)
	}
	modText := string(goMod)
	if !strings.Contains(modText, "github.com/ikigenba/agentkit v0.1.0") {
		t.Fatalf("go.mod does not pin published agentkit v0.1.0:\n%s", modText)
	}
	if strings.Contains(modText, "replace github.com/ikigenba/agentkit") {
		t.Fatalf("go.mod must not replace published agentkit:\n%s", modText)
	}

	cmd := exec.Command("go", "build", "-o", filepath.Join(t.TempDir(), "wiki"), "./cmd/wiki")
	cmd.Dir = filepath.Join("..", "..")
	cmd.Env = append(os.Environ(), "GOWORK=off")
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("GOWORK=off go build ./cmd/wiki failed: %v\n%s", err, out)
	}
}

func TestWorkspaceIncludesWikiModule(t *testing.T) {
	goWork, err := os.ReadFile(filepath.Join("..", "..", "..", "go.work"))
	if err != nil {
		t.Fatalf("read ../go.work: %v", err)
	}
	if !strings.Contains(string(goWork), "./wiki") {
		t.Fatalf("../go.work does not include ./wiki:\n%s", goWork)
	}
}
