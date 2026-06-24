package wiki

import "testing"

func TestNormalizeAppliesPathSafePipeline(t *testing.T) {
	// R-RU0J-77HX
	if got, want := Normalize("  Ｓalaì!!!Apollo  11?? "), "salai-apollo-11"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeLongTitleUsesHyphenSeparatedLowercaseWords(t *testing.T) {
	// R-RV8F-KZ8M
	if got, want := Normalize("Lives of the Most Excellent Painters, Sculptors, and Architects"), "lives-of-the-most-excellent-painters-sculptors-and-architects"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeStripsDiacriticsFromSalai(t *testing.T) {
	// R-RXO8-CIQ0
	if got, want := Normalize("Salaì"), "salai"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeMapsApostropheToSeparator(t *testing.T) {
	// R-RYW4-QAGP
	if got, want := Normalize("Lorenzo de' Medici"), "lorenzo-de-medici"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeTrimsAndCollapsesPunctuation(t *testing.T) {
	// R-S041-427E
	if got, want := Normalize("!!!Hello, World!!!"), "hello-world"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeKeepsDigitsInWords(t *testing.T) {
	// R-S1BX-HTY3
	if got, want := Normalize("Apollo 11"), "apollo-11"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeIsIdempotentAndReturnsEmptyForSeparatorOnlyInputs(t *testing.T) {
	// R-S2JT-VLOS
	inputs := []string{
		"Lives of the Most Excellent Painters, Sculptors, and Architects",
		"Salaì",
		"Lorenzo de' Medici",
		"!!!Hello, World!!!",
		"Apollo 11",
	}
	for _, input := range inputs {
		once := Normalize(input)
		if got := Normalize(once); got != once {
			t.Fatalf("Normalize(Normalize(%q)) = %q, want %q", input, got, once)
		}
	}

	for _, input := range []string{"???", "", "   "} {
		if got := Normalize(input); got != "" {
			t.Fatalf("Normalize(%q) = %q, want empty string", input, got)
		}
	}
}

func TestSpecDeclaresServedMCPService(t *testing.T) {
	spec := Spec()
	if spec.App != "wiki" {
		t.Fatalf("App = %q, want wiki", spec.App)
	}
	if spec.Mount != "/srv/wiki/" {
		t.Fatalf("Mount = %q, want /srv/wiki/", spec.Mount)
	}
	if spec.Port != 3006 {
		t.Fatalf("Port = %d, want 3006", spec.Port)
	}
	if !spec.MCP {
		t.Fatal("MCP = false, want true")
	}
	if spec.Handlers == nil {
		t.Fatal("Handlers is nil; service would not mount /mcp")
	}
	if spec.Config == nil {
		t.Fatal("Config is nil; service would not read LLM configuration")
	}
	if len(spec.Workers) != 1 {
		t.Fatalf("Workers len = %d, want 1", len(spec.Workers))
	}
}

func TestConfigBuildsSharedLLMClient(t *testing.T) {
	cfg, err := NewConfig(func(key string) string {
		if key == "ANTHROPIC_API_KEY" {
			return "test-key"
		}
		return ""
	})
	if err != nil {
		t.Fatalf("NewConfig: %v", err)
	}
	if cfg.Provider == nil {
		t.Fatal("Provider is nil")
	}
	if cfg.LLM == nil {
		t.Fatal("LLM is nil")
	}
	if cfg.LLM.Provider() != cfg.Provider {
		t.Fatal("LLM provider is not the shared provider")
	}
	if cfg.LLM.Model() != ModelID {
		t.Fatalf("LLM model = %q, want %q", cfg.LLM.Model(), ModelID)
	}
}
