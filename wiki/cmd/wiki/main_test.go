package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"

	"appkit/server"
	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/compile"
	"wiki/internal/db"
	"wiki/internal/llm"
	"wiki/internal/wiki"
)

func TestServeFailsLoudWhenAnthropicKeyMissing(t *testing.T) {
	// R-6RVX-P1IG
	for _, value := range []string{"", "   "} {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()

		cmd := exec.CommandContext(ctx, "go", "run", ".", "serve")
		cmd.Env = withoutAnthropicKey(os.Environ())
		cmd.Env = append(cmd.Env, "ANTHROPIC_API_KEY="+value)
		out, err := cmd.CombinedOutput()
		if ctx.Err() == context.DeadlineExceeded {
			t.Fatal("serve did not fail before startup timeout")
		}
		if err == nil {
			t.Fatalf("serve with ANTHROPIC_API_KEY=%q exited 0; output:\n%s", value, out)
		}
		if !strings.Contains(string(out), "ANTHROPIC_API_KEY is required") {
			t.Fatalf("serve output = %q, want missing-key error", out)
		}
	}
}

func TestBuildSpecWiresEightMCPTools(t *testing.T) {
	// R-MUQ4-K1JS
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	spec := buildSpec(wiki.Config{
		ModelID:       "test-model",
		SearchDefault: 8,
		SearchCap:     32,
	})
	srv, err := server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		ResourceID: "https://int.ikigenba.com/srv/wiki/mcp",
		AuthServer: "https://int.ikigenba.com",
		Version:    "test-version",
		Service:    "wiki",
		Register:   spec.Handlers,
		DB:         conn,
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}

	req := httptest.NewRequest(http.MethodPost, "/mcp", strings.NewReader(`{"jsonrpc":"2.0","id":"list","method":"tools/list"}`))
	req.Header.Set("X-Owner-Email", "owner@example.com")
	req.Header.Set("X-Client-Id", "client-1")
	rec := httptest.NewRecorder()
	srv.Handler.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, want 200; body=%s", rec.Code, rec.Body.String())
	}
	var got struct {
		Result struct {
			Tools []struct {
				Name string `json:"name"`
			} `json:"tools"`
		} `json:"result"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &got); err != nil {
		t.Fatalf("decode tools/list response: %v", err)
	}
	names := make(map[string]bool, len(got.Result.Tools))
	for _, tool := range got.Result.Tools {
		names[tool.Name] = true
	}
	want := []string{"ingest", "status", "ask", "subjects", "claims", "page", "health", "reflection"}
	if len(names) != len(want) {
		t.Fatalf("tool names = %#v, want exact %v", names, want)
	}
	for _, name := range want {
		if !names[name] {
			t.Fatalf("tool names = %#v, missing %s", names, name)
		}
	}
}

func TestBuildCompilerUsesDefaultCompileCallSite(t *testing.T) {
	// R-4DS4-RXYX
	prov := &capturingProvider{responses: []string{`{"title":"Acme Robotics","body":"Acme Robotics runs a Tulsa lab."}`}}
	wantSite := compile.DefaultCallSite("root-model")
	compiler := buildCompiler(wiki.Config{
		ModelID: "root-model",
		LLM:     llm.New(prov, nil),
	})

	title, body, err := compiler.Compile(context.Background(), wiki.Subject{
		ID:       "subject-acme",
		Name:     "Acme Robotics",
		NormName: "acme robotics",
		Type:     "entity",
	}, []wiki.Claim{
		{ID: "claim-001", SubjectID: "subject-acme", Body: "Acme Robotics runs a Tulsa lab."},
	})
	if err != nil {
		t.Fatalf("Compile returned error: %v", err)
	}
	if title != "Acme Robotics" || body != "Acme Robotics runs a Tulsa lab." {
		t.Fatalf("Compile result = %q/%q, want mocked page", title, body)
	}
	if len(prov.requests) != 1 {
		t.Fatalf("requests len = %d, want 1", len(prov.requests))
	}
	req := prov.requests[0]
	if req.Model != wantSite.Model {
		t.Fatalf("request model = %q, want %q from compile.DefaultCallSite", req.Model, wantSite.Model)
	}
	if wantSite.Temperature == nil {
		t.Fatal("compile.DefaultCallSite temperature is nil, want deterministic temperature")
	}
	if req.Gen.Temperature == nil || *req.Gen.Temperature != *wantSite.Temperature || !req.Gen.Reasoning.Disabled() {
		t.Fatalf("gen settings = %#v, want compile.DefaultCallSite temperature %v and disabled reasoning", req.Gen, *wantSite.Temperature)
	}
}

func withoutAnthropicKey(env []string) []string {
	out := env[:0]
	for _, kv := range env {
		if strings.HasPrefix(kv, "ANTHROPIC_API_KEY=") {
			continue
		}
		out = append(out, kv)
	}
	return out
}

func migratedDB(t *testing.T, ctx context.Context) *sql.DB {
	t.Helper()
	conn, err := db.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if err := db.Migrate(ctx, conn); err != nil {
		conn.Close()
		t.Fatalf("Migrate: %v", err)
	}
	return conn
}

type capturingProvider struct {
	responses []string
	requests  []agentkit.Request
}

func (p *capturingProvider) RoundTrip(_ context.Context, req *agentkit.Request) *agentkit.RoundTrip {
	p.requests = append(p.requests, cloneRequest(req))
	text := `{"title":"Untitled","body":"Empty."}`
	if len(p.responses) > 0 {
		text = p.responses[0]
		p.responses = p.responses[1:]
	}
	return agentkit.NewRoundTrip(
		agentkit.Message{Role: agentkit.RoleAssistant, Blocks: []agentkit.Block{agentkit.TextBlock{Text: text}}},
		agentkit.FinishStop,
		agentkit.Usage{InputUncached: 1, Output: 1, Total: 2},
		nil,
		nil,
	)
}

func (p *capturingProvider) Name() string {
	return "capturing"
}

func (p *capturingProvider) Pricing(string) (agentkit.Pricing, bool) {
	return agentkit.Pricing{Tiers: []agentkit.RateTier{{MinInputTokens: 0}}}, true
}

func cloneRequest(req *agentkit.Request) agentkit.Request {
	if req == nil {
		return agentkit.Request{}
	}
	return agentkit.Request{
		Model:    req.Model,
		System:   req.System,
		Messages: append([]agentkit.Message(nil), req.Messages...),
		Tools:    append([]agentkit.Tool(nil), req.Tools...),
		Gen:      req.Gen,
	}
}
