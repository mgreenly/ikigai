package wiki

import "testing"

func TestNormalizeReturnsLowercaseASCIIWordsJoinedByHyphens(t *testing.T) {
	// R-RU0J-77HX
	if got, want := Normalize("Acme Robotics Lab 42"), "acme-robotics-lab-42"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeCollapsesPunctuationAndWhitespaceToSingleHyphen(t *testing.T) {
	// R-RV8F-KZ8M
	if got, want := Normalize("Acme / Robotics\t\tLab"), "acme-robotics-lab"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeTrimsLeadingAndTrailingSeparators(t *testing.T) {
	// R-RXO8-CIQ0
	if got, want := Normalize(" -- Acme Robotics!! "), "acme-robotics"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeFoldsCompatibilityCharacters(t *testing.T) {
	// R-RYW4-QAGP
	if got, want := Normalize("ＡＬＰＨＡ Kelvin ①"), "alpha-kelvin-1"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeStripsDiacriticsBeforeApplyingCharacterSet(t *testing.T) {
	// R-S041-427E
	if got, want := Normalize("  Café\u0301 Déjà Vu  "), "cafe-deja-vu"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeTreatsNonASCIILettersAsSeparators(t *testing.T) {
	// R-S1BX-HTY3
	if got, want := Normalize("alpha 東京 beta"), "alpha-beta"; got != want {
		t.Fatalf("Normalize(...) = %q, want %q", got, want)
	}
}

func TestNormalizeOutputContainsOnlyLowercaseASCIIDigitsAndHyphens(t *testing.T) {
	// R-S2JT-VLOS
	got := Normalize(" Café/東京--Rocket № ９ ")
	if got != "cafe-rocket-no-9" {
		t.Fatalf("Normalize(...) = %q, want cafe-rocket-no-9", got)
	}
	for _, r := range got {
		if (r >= 'a' && r <= 'z') || (r >= '0' && r <= '9') || r == '-' {
			continue
		}
		t.Fatalf("Normalize(...) emitted %q outside [a-z0-9-] in %q", r, got)
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
