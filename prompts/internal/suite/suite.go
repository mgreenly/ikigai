// Package suite owns prompts' suite-specific discovery and identity policy: at
// run spawn it snapshots the box's other loopback MCP services and exposes their
// tools to the in-run agent, talking to each peer on behalf of the run's owner.
//
// Discovery is best-effort by design (decision: a down peer must not break a
// run). An unreadable inventory, an unreachable peer, or a garbled tools/list is
// logged loud and skipped; Discover never returns an error and never panics.
//
// Identity flows in as arguments (composition-root pattern): Discover takes
// manifestRoot/owner/promptID and reads no environment. The runner reads
// PROMPTS_MANIFEST_ROOT and threads it here.
package suite

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"sync"
	"time"

	"agentkit/mcpclient"
	"appkit/inventory"
	"github.com/ikigenba/agentkit"
)

// selfName is the service excluded from discovery: a run must not spawn another
// run, so the prompts service never lists its own tools to the in-run agent.
const selfName = "prompts"

// perCallTimeout bounds every outbound MCP call (tools/list during discovery,
// tools/call during dispatch). Generous but well under any run TTL, so a wedged
// peer can never hang discovery or a tool dispatch forever.
const perCallTimeout = 30 * time.Second

// clientIDPrefix is prepended to the run's prompt id to form the X-Client-Id the
// peers see, marking the call as originating from a prompts run.
const clientIDPrefix = "prompts:"

// Discover snapshots, at run spawn, the suite's loopback MCP tools available to
// the in-run agent on behalf of owner. It reads the box inventory under
// manifestRoot, excludes the prompts service itself, and concurrently lists each
// remaining peer's tools, attaching the run's identity headers. Best-effort:
// unreachable or garbled peers are logged and skipped; it never returns an error
// and never panics, always returning a non-nil slice (possibly empty).
func Discover(ctx context.Context, manifestRoot, owner, promptID string) []agentkit.Tool {
	headers := map[string]string{
		"X-Owner-Email": owner,
		"X-Client-Id":   clientIDPrefix + promptID,
	}

	services, err := inventory.Read(manifestRoot)
	if err != nil {
		// Treat an unreadable inventory as zero services: discovery degrades to an
		// empty slice rather than failing the run.
		slog.Error("suite discovery: inventory read failed, no suite tools",
			"manifest_root", manifestRoot, "err", err)
		return []agentkit.Tool{}
	}

	// Concurrently list tools from every non-self peer. Each peer's result lands
	// in its own slot; a guarding mutex serializes the index build.
	type listing struct {
		svc    inventory.Service
		client *mcpclient.Client
		tools  []mcpclient.Tool
	}

	var (
		wg sync.WaitGroup
		mu sync.Mutex
	)
	var listings []listing

	for _, svc := range services {
		if svc.Name == selfName {
			continue // self-exclusion: no run-spawns-run.
		}
		if svc.Port == "" {
			slog.Error("suite discovery: peer has no port, skipping", "service", svc.Name)
			continue
		}

		endpoint := fmt.Sprintf("http://127.0.0.1:%s/mcp", svc.Port)
		client := mcpclient.New(endpoint, headers, perCallTimeout)

		wg.Add(1)
		go func(svc inventory.Service, client *mcpclient.Client) {
			defer wg.Done()
			tools, lerr := client.ListTools(ctx)
			if lerr != nil {
				// A down or garbled peer must not break discovery: log loud, skip it.
				slog.Error("suite discovery: tools/list failed, skipping peer",
					"service", svc.Name, "endpoint", endpoint, "err", lerr)
				return
			}
			mu.Lock()
			listings = append(listings, listing{svc: svc, client: client, tools: tools})
			mu.Unlock()
		}(svc, client)
	}
	wg.Wait()

	// Build RawTool values. Peers now register BARE verbs, so the same verb
	// (health, reflection, ...) is exposed by every service and is not unique across
	// peers. The suite layer re-qualifies each bare verb back to ikigenba_<svc>_<verb>
	// using the owning service's name. The duplicate guard below therefore only
	// fires on a genuine within-service duplicate.
	tools := make([]agentkit.Tool, 0)
	seen := map[string]bool{}
	for _, l := range listings {
		for _, t := range l.tools {
			qualified := qualify(l.svc.Name, t.Name)
			if seen[qualified] {
				slog.Error("suite discovery: duplicate tool name within service, keeping first",
					"tool", qualified, "service", l.svc.Name)
				continue
			}
			seen[qualified] = true
			client := l.client
			bare := t.Name
			tools = append(tools, agentkit.RawTool(qualified, t.Description, t.InputSchema, func(ctx context.Context, input json.RawMessage) (string, error) {
				text, isError, err := client.CallTool(ctx, bare, input)
				if err != nil {
					return "", fmt.Errorf("suite: tool %q failed: %w", qualified, err)
				}
				if isError {
					return text, fmt.Errorf("suite: tool %q returned error: %s", qualified, text)
				}
				return text, nil
			}))
		}
	}

	return tools
}

// qualify reconstructs the service-qualified tool name ikigenba_<svc>_<verb> that
// peers used to register before the bare-verb rename (docs/adr-mcp-tool-bare-names.md).
// svc is the owning service's manifest APP name (the <svc> segment of the old
// prefix), verb is the bare verb the peer now registers.
func qualify(svc, verb string) string {
	return "ikigenba_" + svc + "_" + verb
}
