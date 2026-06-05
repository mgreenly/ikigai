package optctl

import (
	"fmt"
	"strconv"
	"strings"
)

// parseManifest parses a manifest.env body into a key→value map, mirroring the
// parse rules of root bin/registry and appkit/manifest: skip blank and '#'
// comment lines, split on the first '=', trim whitespace from key and value, and
// strip one pair of matching surrounding quotes. It is used by install's
// preflight only to confirm the manifest is well-formed (it is written verbatim,
// not reserialized).
func parseManifest(body string) (map[string]string, error) {
	out := map[string]string{}
	for _, raw := range strings.Split(body, "\n") {
		line := strings.TrimSpace(raw)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		eq := strings.IndexByte(line, '=')
		if eq < 0 {
			return nil, fmt.Errorf("malformed manifest line (no '='): %q", raw)
		}
		key := strings.TrimSpace(line[:eq])
		val := strings.TrimSpace(line[eq+1:])
		if key == "" {
			return nil, fmt.Errorf("malformed manifest line (empty key): %q", raw)
		}
		val = stripQuotes(val)
		out[key] = val
	}
	if len(out) == 0 {
		return nil, fmt.Errorf("manifest is empty")
	}
	return out, nil
}

func stripQuotes(v string) string {
	if len(v) >= 2 {
		first, last := v[0], v[len(v)-1]
		if (first == '"' && last == '"') || (first == '\'' && last == '\'') {
			return v[1 : len(v)-1]
		}
	}
	return v
}

// schemaVersions parses the `applied=<N> embedded=<M>` line emitted by the
// appkit `schema` verb. It is install's seam for detecting whether a deploy
// advances the schema (embedded > applied ⇒ back up the DB before migrate).
func schemaVersions(out string) (applied, embedded int, err error) {
	fields := strings.Fields(strings.TrimSpace(out))
	for _, f := range fields {
		k, v, ok := strings.Cut(f, "=")
		if !ok {
			continue
		}
		n, perr := strconv.Atoi(v)
		if perr != nil {
			return 0, 0, fmt.Errorf("schema: bad %s value %q", k, v)
		}
		switch k {
		case "applied":
			applied = n
		case "embedded":
			embedded = n
		}
	}
	if !strings.Contains(out, "applied=") || !strings.Contains(out, "embedded=") {
		return 0, 0, fmt.Errorf("schema: unexpected output %q (want 'applied=N embedded=M')", strings.TrimSpace(out))
	}
	return applied, embedded, nil
}
