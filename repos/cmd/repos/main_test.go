package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	appdb "appkit/db"
	"appkit/server"
	appweb "appkit/web"
	"eventplane/consumer"
	"eventplane/outbox"

	reposdb "repos/internal/db"
	"repos/internal/repos"
)

func TestManifestVerbMatchesCommittedServiceContract(t *testing.T) {
	// R-EISY-2LYZ
	binary := filepath.Join(t.TempDir(), "repos")
	command := exec.Command("go", "build", "-o", binary, ".")
	if output, err := command.CombinedOutput(); err != nil {
		t.Fatalf("build repos binary: %v: %s", err, output)
	}
	output, err := exec.Command(binary, "manifest").CombinedOutput()
	if err != nil {
		t.Fatalf("repos manifest: %v: %s", err, output)
	}
	want := "APP=repos\nMOUNT=/srv/repos/\nDEFAULT=false\nPORT=3007\nMCP=true\nFEED=/feed\nCONSUMES=webhooks\n"
	if string(output) != want {
		t.Fatalf("manifest output:\n%s\nwant:\n%s", output, want)
	}
	committed, err := os.ReadFile(filepath.Join("..", "..", "etc", "manifest.env"))
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(output, committed) {
		t.Fatalf("manifest verb differs from etc/manifest.env:\n%s", committed)
	}
}

func TestAssembledRoutesGateMCPAndServeLandingAndFeed(t *testing.T) {
	// R-EL8Q-U5GD
	root := t.TempDir()
	dbPath := filepath.Join(root, "repos.db")
	conn, err := appdb.Open(dbPath)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = conn.Close() })
	migrations, err := reposdb.Migrations()
	if err != nil {
		t.Fatal(err)
	}
	if err := appdb.Migrate(context.Background(), conn, migrations); err != nil {
		t.Fatal(err)
	}

	remoteRoot := filepath.Join(root, "remotes")
	if err := os.MkdirAll(remoteRoot, 0o755); err != nil {
		t.Fatal(err)
	}
	runGit(t, "", "init", "--bare", filepath.Join(remoteRoot, "fixture.git"))
	gitConfig := filepath.Join(root, "gitconfig")
	configText := fmt.Sprintf("[url \"file://%s/\"]\n\tinsteadOf = https://github.com/ikigenba/\n", filepath.ToSlash(remoteRoot))
	if err := os.WriteFile(gitConfig, []byte(configText), 0o600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("GIT_CONFIG_GLOBAL", gitConfig)
	t.Setenv("REPOS_STATE_DIR", filepath.Join(root, "state"))
	t.Setenv("ANTHROPIC_API_KEY", "fixture-key")

	var peerMu sync.Mutex
	var peerRequests []*http.Request
	peer := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, request *http.Request) {
		peerMu.Lock()
		peerRequests = append(peerRequests, request.Clone(context.Background()))
		peerMu.Unlock()
		w.Header().Set("Content-Type", "application/json")
		_, _ = io.WriteString(w, `{"jsonrpc":"2.0","id":1,"result":{}}`)
	}))
	defer peer.Close()
	originalClient := http.DefaultClient
	http.DefaultClient = &http.Client{Transport: rewriteTransport{target: peer.URL, base: http.DefaultTransport}}
	t.Cleanup(func() { http.DefaultClient = originalClient })

	site, err := appweb.Load(filepath.Join("..", "..", "share", "www"))
	if err != nil {
		t.Fatal(err)
	}
	producer, err := outbox.New(conn, outbox.Options{
		Source: "repos", DBPath: dbPath, GenerationPath: filepath.Join(root, "generation"), Registry: repos.Events,
	})
	if err != nil {
		t.Fatal(err)
	}
	spec := reposSpec()
	subscriptions := func() []consumer.Subscription {
		var all []consumer.Subscription
		for _, entry := range spec.Consumers {
			all = append(all, entry.Subscriptions...)
		}
		return all
	}
	srv, err := server.New(server.Options{
		Addr: "127.0.0.1:0", Logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
		ResourceID: "https://example.test/srv/repos/", AuthServer: "https://example.test/",
		Version: "test", Service: spec.App, Health: spec.Health, Events: spec.Events,
		Subscriptions: subscriptions, WWW: site, Feed: producer.FeedHandler(), FeedPath: spec.Feed,
		DB: conn, Register: spec.Handlers,
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := spec.Producer(producer); err != nil {
		t.Fatal(err)
	}

	unauthenticated := httptest.NewRecorder()
	srv.Handler.ServeHTTP(unauthenticated, rpcRequest(t, false))
	if unauthenticated.Code != http.StatusUnauthorized && unauthenticated.Code != http.StatusForbidden {
		t.Fatalf("unauthenticated POST /mcp status = %d, want 401 or 403", unauthenticated.Code)
	}
	authenticated := httptest.NewRecorder()
	srv.Handler.ServeHTTP(authenticated, rpcRequest(t, true))
	if authenticated.Code != http.StatusOK || !strings.Contains(authenticated.Body.String(), `"name":"clone"`) {
		t.Fatalf("authenticated tools/list status=%d body=%s", authenticated.Code, authenticated.Body.String())
	}

	landing := httptest.NewRecorder()
	srv.Handler.ServeHTTP(landing, httptest.NewRequest(http.MethodGet, "/", nil))
	if landing.Code != http.StatusOK || !strings.HasPrefix(landing.Header().Get("Content-Type"), "text/html") || !strings.Contains(landing.Body.String(), "repos") {
		t.Fatalf("landing status=%d content-type=%q body=%s", landing.Code, landing.Header().Get("Content-Type"), landing.Body.String())
	}

	ctx, cancel := context.WithCancel(context.Background())
	feedRequest := httptest.NewRequest(http.MethodGet, "/feed?from=tail", nil).WithContext(ctx)
	feedResponse := newSSERecorder()
	done := make(chan struct{})
	go func() {
		srv.Handler.ServeHTTP(feedResponse, feedRequest)
		close(done)
	}()
	select {
	case <-feedResponse.flushed:
		if got := feedResponse.Header().Get("Content-Type"); got != "text/event-stream" {
			t.Fatalf("feed Content-Type = %q", got)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("GET /feed did not establish an SSE response")
	}
	cancel()
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("GET /feed did not stop after request cancellation")
	}
	peerMu.Lock()
	defer peerMu.Unlock()
	if len(peerRequests) != 0 {
		t.Fatalf("tools/list unexpectedly called github peer %d times", len(peerRequests))
	}
}

func rpcRequest(t *testing.T, authenticated bool) *http.Request {
	t.Helper()
	body, err := json.Marshal(map[string]any{"jsonrpc": "2.0", "id": 1, "method": "tools/list"})
	if err != nil {
		t.Fatal(err)
	}
	request := httptest.NewRequest(http.MethodPost, "/mcp", bytes.NewReader(body))
	request.Header.Set("Content-Type", "application/json")
	if authenticated {
		request.Header.Set("X-Owner-Email", "owner@example.com")
		request.Header.Set("X-Client-Id", "fixture")
	}
	return request
}

func runGit(t *testing.T, dir string, args ...string) {
	t.Helper()
	command := exec.Command("git", args...)
	command.Dir = dir
	if output, err := command.CombinedOutput(); err != nil {
		t.Fatalf("git %v: %v: %s", args, err, output)
	}
}

type rewriteTransport struct {
	target string
	base   http.RoundTripper
}

func (r rewriteTransport) RoundTrip(request *http.Request) (*http.Response, error) {
	clone := request.Clone(request.Context())
	target, err := http.NewRequest(http.MethodGet, r.target, nil)
	if err != nil {
		return nil, err
	}
	clone.URL.Scheme = target.URL.Scheme
	clone.URL.Host = target.URL.Host
	return r.base.RoundTrip(clone)
}

type sseRecorder struct {
	*httptest.ResponseRecorder
	flushed chan struct{}
	once    sync.Once
}

func newSSERecorder() *sseRecorder {
	return &sseRecorder{ResponseRecorder: httptest.NewRecorder(), flushed: make(chan struct{})}
}

func (r *sseRecorder) Flush() {
	r.ResponseRecorder.Flush()
	r.once.Do(func() { close(r.flushed) })
}
