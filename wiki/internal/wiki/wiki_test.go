package wiki

import "testing"

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
