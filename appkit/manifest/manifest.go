// Package manifest emits and parses a suite app's manifest.env — the
// deploy-time identity file at /opt/<app>/etc/current/manifest.env. The binary is the
// source of truth for its own identity (PLAN §1.1): `<app> manifest` writes this
// file, and the same KEY=value parser the on-box readers use (appkit/inventory +
// bin/registry) lives here so a self-read and the opsctl preflight share one
// implementation.
//
// Comment policy (PLAN §B1 map §6): the emit is COMMENT-FREE and deterministic,
// and the committed etc/manifest.env files are regenerated to match so
// `<app> manifest` byte-equals the committed oracle. Comments were always inert
// to both parsers; the documentary intent moves to each service's CLAUDE.md.
package manifest

import (
	"bufio"
	"io"
	"strconv"
	"strings"
)

// Fields is the declared identity of one app, the subset of appkit.Spec the
// manifest round-trips. It is a plain value type (no embed.FS / func fields) so
// the manifest package stays dependency-light and the emit is purely textual.
type Fields struct {
	App      string   // APP
	Mount    string   // MOUNT
	Default  bool     // DEFAULT
	Port     int      // PORT
	MCP      bool     // MCP (emitted only when true — the dashboard apex omits it)
	Feed     string   // FEED (producer; emitted only when non-empty)
	Consumes []string // CONSUMES (consumer; comma-joined; emitted only when non-empty)
	Extras   []KV     // ordered non-secret service config, emitted last in declared order
}

// KV is one ordered manifest extra. A slice (not a map) keeps the emit order
// deterministic for the byte-compare. Mirrors appkit.ManifestKV.
type KV struct {
	Key   string
	Value string
}

// Emit renders the manifest in a fixed, deterministic, comment-free order:
//
//	APP, MOUNT, DEFAULT, PORT, then MCP (iff true), FEED (iff producer),
//	CONSUMES (iff consumer), then every Extra in declared order.
//
// Every line is KEY=value\n; the result ends with a trailing newline and
// carries no blank or comment lines, so it byte-equals the regenerated
// committed etc/manifest.env.
func Emit(f Fields) string {
	var b strings.Builder
	write := func(k, v string) {
		b.WriteString(k)
		b.WriteByte('=')
		b.WriteString(v)
		b.WriteByte('\n')
	}
	write("APP", f.App)
	write("MOUNT", f.Mount)
	write("DEFAULT", strconv.FormatBool(f.Default))
	write("PORT", strconv.Itoa(f.Port))
	// MCP is emitted only when set: the apex dashboard manifest omits it entirely,
	// and the byte-compare oracle (dashboard/etc/manifest.env) has no MCP line.
	if f.MCP {
		write("MCP", "true")
	}
	if f.Feed != "" {
		write("FEED", f.Feed)
	}
	if len(f.Consumes) > 0 {
		write("CONSUMES", strings.Join(f.Consumes, ","))
	}
	for _, kv := range f.Extras {
		write(kv.Key, kv.Value)
	}
	return b.String()
}

// Parse reads a manifest.env into an ordered KEY=value map, matching the two
// on-box readers exactly: skip blank lines and lines beginning with '#', split
// on the first '=', trim surrounding whitespace from key and value, and strip a
// single matching surrounding quote pair from the value. Order of first
// appearance is preserved in Keys.
func Parse(r io.Reader) (map[string]string, []string, error) {
	out := map[string]string{}
	var keys []string
	sc := bufio.NewScanner(r)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		eq := strings.IndexByte(line, '=')
		if eq < 0 {
			continue
		}
		key := strings.TrimSpace(line[:eq])
		val := strings.TrimSpace(line[eq+1:])
		val = stripQuotes(val)
		if key == "" {
			continue
		}
		if _, seen := out[key]; !seen {
			keys = append(keys, key)
		}
		out[key] = val
	}
	if err := sc.Err(); err != nil {
		return nil, nil, err
	}
	return out, keys, nil
}

// stripQuotes removes one matching pair of surrounding single or double quotes,
// matching the on-box parsers.
func stripQuotes(v string) string {
	if len(v) >= 2 {
		if (v[0] == '"' && v[len(v)-1] == '"') || (v[0] == '\'' && v[len(v)-1] == '\'') {
			return v[1 : len(v)-1]
		}
	}
	return v
}
