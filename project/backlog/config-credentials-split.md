# Config and Credentials File Handling

## Summary

Split configuration into two files (config vs credentials) and implement a standardized lookup chain with XDG compliance.

## Motivation

- Credentials (API keys, tokens) should be separate from general config
- Enables safe sharing of configs for support/collaboration without exposing secrets
- Aligns with XDG Base Directory specification
- Provides flexible override mechanisms for different deployment scenarios

## Design

### File Separation

**config.json** - Non-sensitive settings:
- Model preferences
- UI settings
- Feature flags
- Paths, defaults

**credentials.json** - Secrets only:
- API keys (Anthropic, etc.)
- Auth tokens

### Lookup Precedence (highest to lowest)

1. CLI argument (`--config`, `--credentials`)
2. Environment variable (`IKIGAI_CONFIG`, `IKIGAI_CREDENTIALS`)
3. XDG default path (`$XDG_CONFIG_HOME/ikigai/` or `~/.config/ikigai/`)

### Default Location Contents

```
~/.config/ikigai/
  config.json              # Actual working defaults (shipped with install)
  credentials.example.json # Template for user to copy and populate
```

### Missing File Behavior

**config.json:**
- Default exists and works out of the box (no user action required)
- If CLI/env override points to missing file → error with hint:
  "Config not found at [path]. Either create this file or remove the override to use default at ~/.config/ikigai/config.json"

**credentials.json:**
- No default credentials (security: user must explicitly provide secrets)
- Example template shipped at `credentials.example.json`
- If missing at default location → error with hint:
  "No credentials found. Copy ~/.config/ikigai/credentials.example.json to credentials.json and add your API key"
- If CLI/env override points to missing file → error with hint:
  "Credentials not found at [path]. Either create this file or remove the override to use default location ~/.config/ikigai/credentials.json"

### CLI Arguments

```
--config <path>       Override config.json location
--credentials <path>  Override credentials.json location
```

### Environment Variables

```
IKIGAI_CONFIG       Override config.json location
IKIGAI_CREDENTIALS  Override credentials.json location
```

## Rationale

- **No silent fallback on override failure**: If user explicitly specifies a path and it's missing, that's an error. Silent fallback hides misconfiguration.
- **Config has working default, credentials has example**: Config can have sensible defaults that work immediately. Credentials require user action (good security hygiene).
- **Separate env vars for each file**: Allows placing credentials in locked-down locations (e.g., `/run/secrets/` in containers) while config lives elsewhere.
- **XDG compliance**: Standard location users expect on Linux systems.
