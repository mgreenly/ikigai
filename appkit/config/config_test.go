package config_test

import (
	"testing"
	"time"

	"appkit/config"
)

// envFunc returns a getenv backed by a map.
func envFunc(m map[string]string) func(string) string {
	return func(k string) string { return m[k] }
}

func TestResolve_ComposesURLsFromDomain(t *testing.T) {
	cfg, err := config.Resolve("ledger", "/srv/ledger/", 3002, envFunc(map[string]string{
		"IKIGENBA_DOMAIN": "int.ikigenba.com",
	}))
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if want := "https://int.ikigenba.com/srv/ledger/mcp"; cfg.ResourceID != want {
		t.Errorf("ResourceID = %q, want %q", cfg.ResourceID, want)
	}
	if want := "https://int.ikigenba.com"; cfg.AuthServer != want {
		t.Errorf("AuthServer = %q, want %q", cfg.AuthServer, want)
	}
	if cfg.Port != 3002 {
		t.Errorf("Port = %d, want default 3002", cfg.Port)
	}
	if cfg.IP != "127.0.0.1" {
		t.Errorf("IP = %q, want loopback default", cfg.IP)
	}
}

func TestResolve_LocalhostDefaults(t *testing.T) {
	cfg, err := config.Resolve("ledger", "/srv/ledger/", 3002, envFunc(map[string]string{}))
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if want := "http://localhost:8080/srv/ledger/mcp"; cfg.ResourceID != want {
		t.Errorf("ResourceID = %q, want %q", cfg.ResourceID, want)
	}
	if want := "http://localhost:8080"; cfg.AuthServer != want {
		t.Errorf("AuthServer = %q, want %q", cfg.AuthServer, want)
	}
}

func TestResolve_ExplicitOverrideWins(t *testing.T) {
	cfg, err := config.Resolve("ledger", "/srv/ledger/", 3002, envFunc(map[string]string{
		"IKIGENBA_DOMAIN":    "int.ikigenba.com",
		"LEDGER_RESOURCE_ID": "https://override.example/srv/ledger/mcp",
		"LEDGER_AUTH_SERVER": "https://override.example",
		"LEDGER_PORT":        "9999",
		"LEDGER_DB_PATH":     "/var/data/ledger.db",
	}))
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if cfg.ResourceID != "https://override.example/srv/ledger/mcp" {
		t.Errorf("ResourceID override ignored: %q", cfg.ResourceID)
	}
	if cfg.AuthServer != "https://override.example" {
		t.Errorf("AuthServer override ignored: %q", cfg.AuthServer)
	}
	if cfg.Port != 9999 {
		t.Errorf("Port = %d, want 9999", cfg.Port)
	}
	if cfg.DBPath != "/var/data/ledger.db" {
		t.Errorf("DBPath = %q, want explicit", cfg.DBPath)
	}
	if want := "/var/data/ledger.db.generation"; cfg.GenerationPath != want {
		t.Errorf("GenerationPath = %q, want %q (derived from DBPath)", cfg.GenerationPath, want)
	}
}

func TestResolve_ApexMount(t *testing.T) {
	cfg, err := config.Resolve("dashboard", "/", 3000, envFunc(map[string]string{
		"IKIGENBA_DOMAIN": "int.ikigenba.com",
	}))
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if want := "https://int.ikigenba.com/mcp"; cfg.ResourceID != want {
		t.Errorf("ResourceID = %q, want %q", cfg.ResourceID, want)
	}
}

func TestResolve_BadPortErrors(t *testing.T) {
	_, err := config.Resolve("ledger", "/srv/ledger/", 3002, envFunc(map[string]string{
		"LEDGER_PORT": "not-a-number",
	}))
	if err == nil {
		t.Fatal("expected error for malformed LEDGER_PORT, got nil")
	}
}

func TestEnvOrInt(t *testing.T) {
	get := envFunc(map[string]string{"A": "42", "B": "oops"})
	if v, err := config.EnvOrInt(get, "A", 7); err != nil || v != 42 {
		t.Errorf("A: got (%d, %v), want (42, nil)", v, err)
	}
	if v, err := config.EnvOrInt(get, "MISSING", 7); err != nil || v != 7 {
		t.Errorf("MISSING: got (%d, %v), want (7, nil)", v, err)
	}
	if _, err := config.EnvOrInt(get, "B", 7); err == nil {
		t.Error("B: expected error for malformed int")
	}
}

func TestEnvOrDuration(t *testing.T) {
	get := envFunc(map[string]string{"D": "5s", "E": "nope"})
	if v, err := config.EnvOrDuration(get, "D", time.Second); err != nil || v != 5*time.Second {
		t.Errorf("D: got (%v, %v), want (5s, nil)", v, err)
	}
	if v, err := config.EnvOrDuration(get, "MISSING", time.Second); err != nil || v != time.Second {
		t.Errorf("MISSING: got (%v, %v), want (1s, nil)", v, err)
	}
	if _, err := config.EnvOrDuration(get, "E", time.Second); err == nil {
		t.Error("E: expected error for malformed duration")
	}
}
