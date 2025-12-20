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

```c
static const ik_config_defaults_t DEFAULTS = {
    .default_provider = "openai",  // Most common
    .providers = {
        {"anthropic", "claude-sonnet-4-5", "med"},
        {"openai", "gpt-4o", "none"},
        {"google", "gemini-2.5-flash", "med"}
    },
    .ui = {
        .theme = "dark",
        .show_thinking = true
    }
};
```

### Loading

```c
res_t ik_config_load(TALLOC_CTX *ctx, ik_config_t **out_config)
{
    // Try XDG_CONFIG_HOME first, fall back to ~/.config
    const char *config_dir = getenv("XDG_CONFIG_HOME");
    if (config_dir == NULL) {
        config_dir = talloc_asprintf(ctx, "%s/.config", getenv("HOME"));
    }

    char *config_path = talloc_asprintf(ctx, "%s/ikigai/config.json",
                                       config_dir);

    // Read file
    FILE *f = fopen(config_path, "r");
    if (f == NULL) {
        // File doesn't exist - use defaults
        *out_config = create_default_config(ctx);
        return OK(NULL);
    }

    // Parse JSON
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json = talloc_array(ctx, char, len + 1);
    fread(json, 1, len, f);
    json[len] = '\0';
    fclose(f);

    yyjson_doc *doc = yyjson_read(json, len, 0);
    if (doc == NULL) {
        return ERR(ctx, ERR_INVALID_ARG,
                  "Failed to parse config.json: invalid JSON");
    }

    // Parse config
    ik_config_t *config = talloc_zero_(ctx, sizeof(*config));
    TRY(parse_config(config, doc));

    yyjson_doc_free(doc);
    *out_config = config;
    return OK(NULL);
}
```

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

```c
res_t ik_credentials_load(TALLOC_CTX *ctx, ik_credentials_t **out_creds)
{
    char *creds_path = talloc_asprintf(ctx, "%s/ikigai/credentials.json",
                                      config_dir);

    // Check permissions
    struct stat st;
    if (stat(creds_path, &st) == 0) {
        if ((st.st_mode & 0077) != 0) {
            fprintf(stderr, "⚠️  Warning: credentials.json has insecure permissions\n");
            fprintf(stderr, "   Run: chmod 600 %s\n", creds_path);
        }
    }

    // Load file...
}
```

### Environment Variable Precedence

Credentials are loaded in this order:
1. **Environment variable** (e.g., `ANTHROPIC_API_KEY`)
2. **credentials.json** file
3. **Error** (no credentials found)

```c
res_t ik_credentials_get_api_key(TALLOC_CTX *ctx,
                                 const char *provider,
                                 char **out_key)
{
    // Try environment variable first
    const char *env_var = ik_provider_env_var(provider);  // "ANTHROPIC_API_KEY"
    const char *env_value = getenv(env_var);

    if (env_value != NULL && strlen(env_value) > 0) {
        *out_key = talloc_strdup(ctx, env_value);
        return OK(NULL);
    }

    // Try credentials.json
    ik_credentials_t *creds = NULL;
    TRY(ik_credentials_load(ctx, &creds));

    const char *key = ik_credentials_get(creds, provider);
    if (key != NULL) {
        *out_key = talloc_strdup(ctx, key);
        return OK(NULL);
    }

    // Not found
    return ERR(ctx, ERR_AUTH,
              "No credentials for %s. Set %s or add to credentials.json",
              provider, env_var);
}
```

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

### Startup Validation (Optional)

```c
res_t ik_config_validate(ik_config_t *config)
{
    // Check default_provider exists
    bool provider_exists = false;
    const char *providers[] = {"anthropic", "openai", "google", "xai", "meta"};
    for (size_t i = 0; i < sizeof(providers) / sizeof(providers[0]); i++) {
        if (strcmp(config->default_provider, providers[i]) == 0) {
            provider_exists = true;
            break;
        }
    }

    if (!provider_exists) {
        fprintf(stderr, "⚠️  Warning: unknown default_provider '%s' in config.json\n",
               config->default_provider);
        fprintf(stderr, "   Valid providers: anthropic, openai, google, xai, meta\n");
    }

    // Check thinking levels
    const char *valid_levels[] = {"none", "low", "med", "high"};
    // ... validate each provider's default_thinking ...

    return OK(NULL);
}
```

### Runtime Validation

No validation at startup. Errors surface when features are used.

## Configuration Updates

### Programmatic Updates

```c
// User runs: /model claude-sonnet-4-5/med
// Update agent state (not config.json)
agent->provider = "anthropic";
agent->model = "claude-sonnet-4-5";
agent->thinking_level = IK_THINKING_MED;

// Save to database
ik_db_update_agent(db, agent);
```

**config.json is not auto-updated.** It only provides initial defaults.

### Manual Editing

Users can edit config.json manually:

```bash
nano ~/.config/ikigai/config.json
```

Changes take effect on next startup (config is loaded once).

## Testing

### Mock Configuration

```c
START_TEST(test_config_default_provider) {
    const char *config_json =
        "{\"default_provider\":\"anthropic\","
        "\"providers\":{\"anthropic\":{\"default_model\":\"claude-sonnet-4-5\"}}}";

    ik_config_t *config = NULL;
    TRY(parse_config_string(ctx, config_json, &config));

    ck_assert_str_eq(config->default_provider, "anthropic");
    ck_assert_str_eq(config->providers[0].default_model, "claude-sonnet-4-5");
}
END_TEST
```

### Mock Credentials

```c
START_TEST(test_credentials_env_precedence) {
    // Set environment variable
    setenv("ANTHROPIC_API_KEY", "env-key", 1);

    // Create credentials.json with different key
    write_credentials_file("{\"anthropic\":{\"api_key\":\"file-key\"}}");

    // Should prefer environment variable
    char *key = NULL;
    TRY(ik_credentials_get_api_key(ctx, "anthropic", &key));

    ck_assert_str_eq(key, "env-key");
}
END_TEST
```

## Migration

### From rel-06 (OpenAI only)

Old format (if it existed):
```json
{
  "openai_api_key": "sk-..."
}
```

New format:
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

Credentials:
```json
{
  "openai": {
    "api_key": "sk-..."
  }
}
```

**No automatic migration.** Developer (only user) will manually create new config/credentials files.

## Security Considerations

1. **Never log API keys** - Redact in logs/errors
2. **Warn on insecure permissions** - credentials.json should be mode 600
3. **No keys in config.json** - Only in credentials.json
4. **Environment variables cleared** - Don't leave in shell history
5. **No version control** - Add credentials.json to .gitignore

Example redaction:

```c
void ik_log_api_key(const char *key) {
    // Log first 8 chars + "..." for debugging
    if (strlen(key) > 8) {
        ik_log(LOG_DEBUG, "API key: %.8s...", key);
    } else {
        ik_log(LOG_DEBUG, "API key: [redacted]");
    }
}
```
