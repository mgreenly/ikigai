// Package inventory reads the box's per-service deploy manifests and reports the
// services that expose an MCP endpoint, so the suite plugin's connect skill can
// wire up each one.
//
// Each reported Service carries its Name, Mount, loopback Port, and (for
// event-plane producers) its Feed path.
//
// A service is included only when its manifest sets MCP=true. The dashboard (the
// authorization server) is intentionally NOT special-cased out: its own manifest
// simply omits MCP=true, so it never appears. If someone mistakenly adds MCP=true
// to the dashboard manifest, it would self-list — the omission is the contract.
package inventory

import (
	"os"
	"path/filepath"
	"sort"

	"appkit/manifest"
)

// Service is one MCP-exposing service discovered on the box. The MCP resource URL
// is not computed here: it needs the request host, which only the HTTP layer has.
// Port is the loopback port the service binds; Feed is its event-plane feed path
// (empty unless the service is a producer).
type Service struct {
	Name  string
	Mount string
	Port  string
	Feed  string
}

// Read globs root/*/etc/manifest.env, parses each as simple shell KEY=value via
// appkit/manifest.Parse, and returns the services whose manifest sets MCP=true
// (sorted by Name). A single unreadable or garbled manifest is skipped, not
// fatal; the only returned error is a glob-level failure.
func Read(root string) ([]Service, error) {
	matches, err := filepath.Glob(filepath.Join(root, "*", "etc", "manifest.env"))
	if err != nil {
		return nil, err
	}
	var services []Service
	for _, path := range matches {
		env, perr := parseManifest(path)
		if perr != nil {
			continue
		}
		if env["MCP"] != "true" {
			continue
		}
		services = append(services, Service{
			Name:  env["APP"],
			Mount: env["MOUNT"],
			Port:  env["PORT"],
			Feed:  env["FEED"],
		})
	}
	sort.Slice(services, func(i, j int) bool { return services[i].Name < services[j].Name })
	return services, nil
}

// parseManifest opens a manifest.env and parses it into a key/value map using the
// shared appkit/manifest.Parse, so the inventory scan and the opsctl preflight
// share one parser. An unreadable file or a scanner-level parse error is returned
// so Read can skip that single manifest.
func parseManifest(path string) (map[string]string, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	env, _, err := manifest.Parse(f)
	if err != nil {
		return nil, err
	}
	return env, nil
}
