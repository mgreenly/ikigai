package config

import (
	"path/filepath"
	"testing"
	"time"

	"registry"
)

// envFunc returns a getenv backed by a map.
func envFunc(m map[string]string) func(string) string {
	return func(k string) string { return m[k] }
}

func TestResolve_ComposesURLsFromDomain(t *testing.T) {
	cfg, err := Resolve("ledger", "/srv/ledger/", 3101, envFunc(map[string]string{
		"IKIGENBA_DOMAIN": "int.ikigenba.com",
		"IKIGENBA_ROOT":   "/opt",
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
	if cfg.Port != 3101 {
		t.Errorf("Port = %d, want default 3101", cfg.Port)
	}
	if cfg.IP != "127.0.0.1" {
		t.Errorf("IP = %q, want loopback default", cfg.IP)
	}
}

func TestResolve_LocalhostDefaults(t *testing.T) {
	cfg, err := Resolve("ledger", "/srv/ledger/", 3101, envFunc(map[string]string{}))
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
	dbPath := filepath.Join(t.TempDir(), "var", "data", "ledger.db")
	genPath := filepath.Join(t.TempDir(), "var", "cache", "ledger.db.generation")
	cfg, err := Resolve("ledger", "/srv/ledger/", 3101, envFunc(map[string]string{
		"IKIGENBA_DOMAIN":        "int.ikigenba.com",
		"IKIGENBA_ROOT":          "/opt",
		"LEDGER_RESOURCE_ID":     "https://override.example/srv/ledger/mcp",
		"LEDGER_AUTH_SERVER":     "https://override.example",
		"LEDGER_PORT":            "9999",
		"LEDGER_DB_PATH":         dbPath,
		"LEDGER_GENERATION_PATH": genPath,
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
	if cfg.DBPath != dbPath {
		t.Errorf("DBPath = %q, want explicit", cfg.DBPath)
	}
	if cfg.GenerationPath != genPath {
		t.Errorf("GenerationPath = %q, want explicit", cfg.GenerationPath)
	}
}

func TestComposeDataPaths_RootOverridesAndDevDefaults(t *testing.T) {
	// R-8FUU-NRQT
	t.Run("production root", func(t *testing.T) {
		dbPath, genPath := composeDataPaths(envFunc(map[string]string{
			"IKIGENBA_ROOT": "/opt",
		}), "LEDGER", "ledger")

		if want := filepath.Join("/opt", "ledger", "state", "ledger.db"); dbPath != want {
			t.Fatalf("dbPath = %q, want %q", dbPath, want)
		}
		if want := filepath.Join("/opt", "ledger", "cache", "ledger.db.generation"); genPath != want {
			t.Fatalf("genPath = %q, want %q", genPath, want)
		}
	})

	t.Run("explicit overrides", func(t *testing.T) {
		dbOverride := filepath.Join(t.TempDir(), "custom", "ledger.db")
		genOverride := filepath.Join(t.TempDir(), "custom", "ledger.db.generation")
		dbPath, genPath := composeDataPaths(envFunc(map[string]string{
			"IKIGENBA_ROOT":          "/opt",
			"LEDGER_DB_PATH":         dbOverride,
			"LEDGER_GENERATION_PATH": genOverride,
		}), "LEDGER", "ledger")

		if dbPath != dbOverride {
			t.Fatalf("dbPath = %q, want explicit %q", dbPath, dbOverride)
		}
		if genPath != genOverride {
			t.Fatalf("genPath = %q, want explicit %q", genPath, genOverride)
		}
	})

	t.Run("dev defaults", func(t *testing.T) {
		dbPath, genPath := composeDataPaths(envFunc(map[string]string{}), "LEDGER", "ledger")

		if want := "./tmp/ledger.db"; dbPath != want {
			t.Fatalf("dbPath = %q, want %q", dbPath, want)
		}
		if want := "./tmp/ledger.db.generation"; genPath != want {
			t.Fatalf("genPath = %q, want %q", genPath, want)
		}
	})
}

func TestResolve_ProductionStateCachePathsAreIndependent(t *testing.T) {
	// R-485J-7TWG
	root := filepath.Join(t.TempDir(), "opt")
	dbPath := filepath.Join(root, "ledger", "state", "ledger.db")
	genPath := filepath.Join(root, "ledger", "cache", "ledger.db.generation")

	cfg, err := Resolve("ledger", "/srv/ledger/", 3101, envFunc(map[string]string{
		"IKIGENBA_ROOT": root,
	}))
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if cfg.DBPath != dbPath {
		t.Fatalf("DBPath = %q, want %q", cfg.DBPath, dbPath)
	}
	if cfg.GenerationPath != genPath {
		t.Fatalf("GenerationPath = %q, want %q", cfg.GenerationPath, genPath)
	}
	if got := filepath.Base(filepath.Dir(cfg.DBPath)); got != "state" {
		t.Errorf("DBPath parent = %q, want state", got)
	}
	if got := filepath.Base(filepath.Dir(cfg.GenerationPath)); got != "cache" {
		t.Errorf("GenerationPath parent = %q, want cache", got)
	}
	if filepath.Dir(cfg.DBPath) == filepath.Dir(cfg.GenerationPath) {
		t.Fatalf("DBPath and GenerationPath share a directory: %s", filepath.Dir(cfg.DBPath))
	}
}

func TestResolve_DomainWithoutRootFailsLoudly(t *testing.T) {
	// R-8H2R-1JHI
	cfg, err := Resolve("ledger", "/srv/ledger/", 3101, envFunc(map[string]string{
		"IKIGENBA_DOMAIN": "int.ikigenba.com",
	}))
	if err == nil {
		t.Fatal("Resolve returned nil error when IKIGENBA_DOMAIN was set without IKIGENBA_ROOT")
	}
	if cfg.DBPath == "./tmp/ledger.db" {
		t.Fatalf("Resolve silently used dev DBPath %q", cfg.DBPath)
	}
}

func TestResolve_DomainWithoutRootAllowsExplicitDataPaths(t *testing.T) {
	dbPath := filepath.Join("state", "ledger.db")
	genPath := filepath.Join("cache", "ledger.db.generation")
	cfg, err := Resolve("ledger", "/srv/ledger/", 3101, envFunc(map[string]string{
		"IKIGENBA_DOMAIN":        "int.ikigenba.com",
		"LEDGER_DB_PATH":         dbPath,
		"LEDGER_GENERATION_PATH": genPath,
	}))
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if cfg.DBPath != dbPath {
		t.Fatalf("DBPath = %q, want %q", cfg.DBPath, dbPath)
	}
	if cfg.GenerationPath != genPath {
		t.Fatalf("GenerationPath = %q, want %q", cfg.GenerationPath, genPath)
	}
}

func TestResolve_ComposesWWWPath(t *testing.T) {
	dataDir := t.TempDir()
	tests := []struct {
		name string
		env  map[string]string
		want string
	}{
		{
			// R-LWOU-OWWQ
			name: "production root uses shipped share current tier",
			env: map[string]string{
				"IKIGENBA_DOMAIN":     "int.ikigenba.com",
				"IKIGENBA_ROOT":       "/opt",
				"CRM_DB_PATH":         filepath.Join(dataDir, "state", "crm.db"),
				"CRM_GENERATION_PATH": filepath.Join(dataDir, "cache", "crm.db.generation"),
			},
			want: filepath.Join("/opt", "crm", "share", "current", "www"),
		},
		{
			// R-LXWR-2ONF
			name: "dev default uses service share www",
			env:  map[string]string{},
			want: "./share/www",
		},
		{
			// R-LZ4N-GGE4
			name: "explicit override wins over production root",
			env: map[string]string{
				"IKIGENBA_ROOT": "/opt",
				"CRM_WWW_PATH":  "/somewhere/else",
			},
			want: "/somewhere/else",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg, err := Resolve("crm", "/srv/crm/", 3001, envFunc(tt.env))
			if err != nil {
				t.Fatalf("Resolve: %v", err)
			}
			if cfg.WWWPath != tt.want {
				t.Fatalf("WWWPath = %q, want %q", cfg.WWWPath, tt.want)
			}
		})
	}
}

func TestResolveConsumerFeed_DefaultsAndOverrides(t *testing.T) {
	tests := []struct {
		name        string
		env         map[string]string
		wantFeedURL string
		wantFrom    string
	}{
		{
			// R-464U-T3T1
			name:        "registry feed default and tail default",
			env:         map[string]string{},
			wantFeedURL: registry.BaseURL("crm") + "/feed",
			wantFrom:    "tail",
		},
		{
			// R-47CR-6VJQ
			name: "feed url override wins",
			env: map[string]string{
				"NOTIFY_CRM_FEED_URL": "http://sentinel.example/feed",
			},
			wantFeedURL: "http://sentinel.example/feed",
			wantFrom:    "tail",
		},
		{
			// R-48KN-KNAF
			name: "from override wins verbatim",
			env: map[string]string{
				"NOTIFY_CRM_FROM": "earliest",
			},
			wantFeedURL: registry.BaseURL("crm") + "/feed",
			wantFrom:    "earliest",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			feedURL, from := ResolveConsumerFeed("notify", "crm", envFunc(tt.env))
			if feedURL != tt.wantFeedURL {
				t.Fatalf("feedURL = %q, want %q", feedURL, tt.wantFeedURL)
			}
			if from != tt.wantFrom {
				t.Fatalf("from = %q, want %q", from, tt.wantFrom)
			}
		})
	}
}

func TestResolve_ApexMount(t *testing.T) {
	cfg, err := Resolve("dashboard", "/", 3000, envFunc(map[string]string{
		"IKIGENBA_DOMAIN": "int.ikigenba.com",
		"IKIGENBA_ROOT":   "/opt",
	}))
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if want := "https://int.ikigenba.com/mcp"; cfg.ResourceID != want {
		t.Errorf("ResourceID = %q, want %q", cfg.ResourceID, want)
	}
}

func TestResolve_BadPortErrors(t *testing.T) {
	_, err := Resolve("ledger", "/srv/ledger/", 3101, envFunc(map[string]string{
		"LEDGER_PORT": "not-a-number",
	}))
	if err == nil {
		t.Fatal("expected error for malformed LEDGER_PORT, got nil")
	}
}

func TestEnvOrInt(t *testing.T) {
	get := envFunc(map[string]string{"A": "42", "B": "oops"})
	if v, err := EnvOrInt(get, "A", 7); err != nil || v != 42 {
		t.Errorf("A: got (%d, %v), want (42, nil)", v, err)
	}
	if v, err := EnvOrInt(get, "MISSING", 7); err != nil || v != 7 {
		t.Errorf("MISSING: got (%d, %v), want (7, nil)", v, err)
	}
	if _, err := EnvOrInt(get, "B", 7); err == nil {
		t.Error("B: expected error for malformed int")
	}
}

func TestEnvOrDuration(t *testing.T) {
	get := envFunc(map[string]string{"D": "5s", "E": "nope"})
	if v, err := EnvOrDuration(get, "D", time.Second); err != nil || v != 5*time.Second {
		t.Errorf("D: got (%v, %v), want (5s, nil)", v, err)
	}
	if v, err := EnvOrDuration(get, "MISSING", time.Second); err != nil || v != time.Second {
		t.Errorf("MISSING: got (%v, %v), want (1s, nil)", v, err)
	}
	if _, err := EnvOrDuration(get, "E", time.Second); err == nil {
		t.Error("E: expected error for malformed duration")
	}
}
