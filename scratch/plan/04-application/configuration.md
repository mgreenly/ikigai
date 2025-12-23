# Configuration Format

## Overview

ikigai uses two configuration files:
- **config.json** - User preferences, provider settings, defaults
- **credentials.json** - API keys (sensitive, should be mode 600)

Both are stored in `~/.config/ikigai/` (or `$XDG_CONFIG_HOME/ikigai/`).

## config.json

### Structure

```json
{
  "default_provider": "anthropic",
  "providers": {
    "anthropic": {
      "default_model": "claude-sonnet-4-5",
      "default_thinking": "med",
      "base_url": "https://api.anthropic.com"
    },
    "openai": {
      "default_model": "gpt-4o",
      "default_thinking": "none",
      "base_url": "https://api.openai.com",
      "use_responses_api": true
    },
    "google": {
      "default_model": "gemini-2.5-flash",
      "default_thinking": "med",
      "base_url": "https://generativelanguage.googleapis.com/v1beta"
    }
  },
  "ui": {
    "theme": "dark",
    "show_thinking": true
  }
}
```

### Fields

**Top-level:**
- `default_provider` (string): Provider to use for first root agent. Only applies once. Values: `"anthropic"`, `"openai"`, `"google"`, `"xai"`, `"meta"`

**Per-provider:**
- `default_model` (string): Model to use when provider is selected without explicit model
- `default_thinking` (string): Default thinking level. Values: `"none"`, `"low"`, `"med"`, `"high"`
- `base_url` (string, optional): Override API base URL (for testing, proxies, etc.)
- `use_responses_api` (boolean, OpenAI only): Use Responses API instead of Chat Completions API

**UI settings:**
- `theme` (string): UI theme. Values: `"dark"`, `"light"`
- `show_thinking` (boolean): Display thinking content in scrollback (default: true)

### Defaults

If config.json doesn't exist or is incomplete, use these defaults:
- `default_provider`: `"openai"` (most common)
- Anthropic: `"claude-sonnet-4-5"` with thinking level `"med"`
- OpenAI: `"gpt-4o"` with thinking level `"none"`
- Google: `"gemini-2.5-flash"` with thinking level `"med"`
- UI: Dark theme with thinking visible

### Loading Functions

**Naming Convention:** Uses `cfg` abbreviation per project naming standards (see .claude/library/naming/SKILL.md). Type is `ik_cfg_t`, functions are `ik_cfg_*()`.

```c
// Load configuration from XDG_CONFIG_HOME or ~/.config/ikigai/config.json
// Falls back to defaults if file doesn't exist
res_t ik_cfg_load(TALLOC_CTX *ctx, ik_cfg_t **out_config);
```

**Loading flow:**
1. Try `$XDG_CONFIG_HOME/ikigai/config.json`
2. Fall back to `~/.config/ikigai/config.json`
3. If file doesn't exist, use built-in defaults
4. Parse JSON and populate config structure
5. Return config object

## credentials.json

### Structure

```json
{
  "anthropic": {
    "api_key": "sk-ant-api03-..."
  },
  "openai": {
    "api_key": "sk-proj-..."
  },
  "google": {
    "api_key": "..."
  },
  "xai": {
    "api_key": "xai-..."
  },
  "meta": {
    "api_key": "..."
  }
}
```

### Fields

Per-provider:
- `api_key` (string): API key for the provider

### File Permissions

**Required:** Mode 600 (owner read/write only)

Function to check permissions:
```c
bool ik_credentials_insecure_permissions(const char *path);
```

Returns true if file has permissions beyond owner read/write (mode & 0077 != 0).

### Credentials API

**Two-function design** separates loading from lookup:

```c
// Load credentials from file and environment
// Precedence: environment variable > credentials.json file
res_t ik_credentials_load(TALLOC_CTX *ctx, const char *path,
                          ik_credentials_t **out_creds);

// Simple lookup in loaded credentials (returns NULL if not found)
const char *ik_credentials_get(const ik_credentials_t *creds,
                               const char *provider);
```

**Loading precedence:**
1. Environment variables (highest priority)
2. credentials.json file
3. NULL if neither exists

**Loading flow:**
1. Load from environment variables first
2. Determine credentials path (use provided path or default)
3. Check if file exists (skip if not)
4. Warn if file has insecure permissions
5. Parse JSON and load file values (only if env var not set)
6. Return credentials object

### Environment Variables

| Provider | Variable |
|----------|----------|
| Anthropic | `ANTHROPIC_API_KEY` |
| OpenAI | `OPENAI_API_KEY` |
| Google | `GOOGLE_API_KEY` |
| xAI | `XAI_API_KEY` |
| Meta | `LLAMA_API_KEY` |

## Configuration Loading Flow

```
Startup
  ↓
Load config.json (or use defaults)
  ↓
User runs /model command
  ↓
Lazy load credentials for provider
  ↓
Create provider instance
  ↓
Make API request
```

No credentials are loaded at startup - only when provider is first used.

## User Feedback

### No Credentials

```
❌ Cannot send message: no credentials configured for anthropic

To fix:
  • Set ANTHROPIC_API_KEY environment variable:
    export ANTHROPIC_API_KEY='sk-ant-api03-...'

  • Or add to ~/.config/ikigai/credentials.json:
    {
      "anthropic": {
        "api_key": "sk-ant-api03-..."
      }
    }

Get your API key at: https://console.anthropic.com/settings/keys
```

### Invalid JSON

```
❌ Failed to load config.json: invalid JSON at line 5

Check your config file:
  ~/.config/ikigai/config.json
```

### Missing Provider in Config

```
⚠️  No default model configured for xai, using first available model
```

## Configuration Validation

### Startup Validation

Function signature:
```c
res_t ik_cfg_validate(ik_cfg_t *config);
```

**Validation checks:**
- `default_provider` is one of: anthropic, openai, google, xai, meta
- `default_thinking` values are: none, low, med, high
- Warnings are printed to stderr, not errors

### Runtime Validation

No validation at startup. Errors surface when features are used.

## Configuration Updates

### Programmatic Updates

Agent settings are updated in database, not config.json:
- User runs `/model claude-sonnet-4-5/med`
- Update agent state in memory
- Save to database via `ik_db_update_agent()`

**config.json is not auto-updated.** It only provides initial defaults.

### Manual Editing

Users can edit config.json manually:

```bash
nano ~/.config/ikigai/config.json
```

Changes take effect on next startup (config is loaded once).

## Testing

### Mock Configuration

Tests use in-memory JSON strings to create mock configurations:
- Parse JSON strings directly
- No file I/O required
- Validate field values match expected defaults

### Mock Credentials

Tests validate environment variable precedence:
- Set environment variable
- Create credentials.json with different value
- Verify environment variable takes priority

## Migration

### From rel-06 (OpenAI only)

Old format (if it existed):
```json
{
  "openai_api_key": "sk-..."
}
```

New config.json format:
```json
{
  "default_provider": "openai",
  "providers": {
    "openai": {
      "default_model": "gpt-4o",
      "default_thinking": "none"
    }
  }
}
```

New credentials.json format:
```json
{
  "openai": {
    "api_key": "sk-..."
  }
}
```

**No automatic migration.** Developer (only user) will manually create new config/credentials files.

## Security Considerations

1. **Never log API keys** - Redact in logs/errors (log first 8 chars + "..." or "[redacted]")
2. **Warn on insecure permissions** - credentials.json should be mode 600
3. **No keys in config.json** - Only in credentials.json
4. **Environment variables cleared** - Don't leave in shell history
5. **No version control** - Add credentials.json to .gitignore

Function for safe logging:
```c
void ik_log_api_key(const char *key);
```
