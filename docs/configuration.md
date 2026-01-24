# Configuration

## Overview

ikigai runs with built-in defaults and requires no configuration files to start. However, it requires API credentials to be useful:

- **At least one AI provider** (OpenAI, Anthropic, or Google)
- **At least one web search provider** (Brave or Google Custom Search)

Credentials can be provided via environment variables or a `credentials.json` file.

## Quick Start

**Minimum configuration:**
1. Create `~/.config/ikigai/credentials.json` with at least one AI provider and one search provider
2. Set file permissions: `chmod 600 ~/.config/ikigai/credentials.json`
3. Run ikigai

## Configuration Files

### Location (XDG Base Directory)

ikigai uses environment variables for directory locations, falling back to XDG Base Directory defaults:

**Runtime directories:**
- `IKIGAI_CONFIG_DIR` - Configuration files (default: `$XDG_CONFIG_HOME/ikigai/` or `~/.config/ikigai/`)
- `IKIGAI_CACHE_DIR` - Cache files (default: `$XDG_CACHE_HOME/ikigai/` or `~/.cache/ikigai/`)
- `IKIGAI_STATE_DIR` - Persistent state (default: `$XDG_STATE_HOME/ikigai/` or `~/.local/state/ikigai/`)

**Development:**
The `.envrc` file (used with direnv) sets up development directories:
- `IKIGAI_CONFIG_DIR=$PWD/etc/ikigai`
- `IKIGAI_CACHE_DIR=$PWD/cache`
- `IKIGAI_STATE_DIR=$PWD/state`
- `IKIGAI_DATA_DIR=$PWD/share/ikigai`
- `IKIGAI_BIN_DIR=$PWD/bin`
- `IKIGAI_LIBEXEC_DIR=$PWD/libexec/ikigai`

### credentials.json

API keys for external services. Environment variables take precedence over this file.

**Location:** `IKIGAI_CONFIG_DIR/credentials.json` (defaults to `$XDG_CONFIG_HOME/ikigai/` or `~/.config/ikigai/`)

**Permissions:** Must be `0600` (owner read/write only). ikigai will warn if permissions are insecure.

**Example:**
```json
{
  "OPENAI_API_KEY": "sk-proj-...",
  "BRAVE_API_KEY": "BSA..."
}
```

**Complete example with all fields:**
```json
{
  "OPENAI_API_KEY": "sk-proj-YOUR_KEY_HERE",
  "ANTHROPIC_API_KEY": "sk-ant-api03-YOUR_KEY_HERE",
  "GOOGLE_API_KEY": "YOUR_KEY_HERE",
  "BRAVE_API_KEY": "YOUR_KEY_HERE",
  "GOOGLE_SEARCH_API_KEY": "YOUR_KEY_HERE",
  "GOOGLE_SEARCH_ENGINE_ID": "YOUR_ENGINE_ID_HERE",
  "NTFY_API_KEY": "YOUR_KEY_HERE",
  "NTFY_TOPIC": "YOUR_TOPIC_HERE"
}
```

### config.json

Application settings. All fields are optional; ikigai uses compiled defaults for missing fields.

**Location:** `IKIGAI_CONFIG_DIR/config.json` (defaults to `$XDG_CONFIG_HOME/ikigai/` or `~/.config/ikigai/`)

**Example:**
```json
{
  "openai_model": "gpt-4o",
  "openai_temperature": 1.0,
  "openai_system_message": "You are Ikigai, a helpful coding assistant.",
  "openai_max_completion_tokens": 4096,
  "db_connection_string": "postgresql://ikigai:ikigai@localhost/ikigai",
  "listen_address": "127.0.0.1",
  "listen_port": 1984,
  "max_tool_turns": 50,
  "max_output_size": 4096
}
```

**Fields:**
- `openai_model` - OpenAI model name (default: compiled-in)
- `openai_temperature` - Sampling temperature 0.0-2.0 (default: 1.0)
- `openai_system_message` - System prompt (default: compiled-in)
- `openai_max_completion_tokens` - Max tokens in completion (default: 4096)
- `db_connection_string` - PostgreSQL connection string
- `listen_address` - HTTP server bind address (default: 127.0.0.1)
- `listen_port` - HTTP server port (default: 1984)
- `max_tool_turns` - Maximum tool execution iterations (default: 50)
- `max_output_size` - Maximum output size in bytes (default: 4096)

## API Credentials

### Credential Precedence

1. **Environment variables** (highest priority)
2. **credentials.json file** (fallback)

If both are set, environment variables win. Missing credentials are NULL (providers using them will fail).

### AI Providers

Configure at least one:

#### OpenAI
- **Key:** `OPENAI_API_KEY`
- **Format:** `sk-proj-...`
- **Sign up:** https://platform.openai.com/api-keys
- **Models:** GPT-4, GPT-4o, GPT-3.5, etc.

#### Anthropic
- **Key:** `ANTHROPIC_API_KEY`
- **Format:** `sk-ant-api03-...`
- **Sign up:** https://console.anthropic.com/settings/keys
- **Models:** Claude 3.5 Sonnet, Claude 3 Opus, etc.

#### Google (Gemini)
- **Key:** `GOOGLE_API_KEY`
- **Sign up:** https://aistudio.google.com/app/apikey
- **Models:** Gemini 1.5 Pro, Gemini 1.5 Flash, etc.
- **Note:** This is separate from Google Custom Search API

### Web Search Providers

Configure at least one:

#### Brave Search
- **Key:** `BRAVE_API_KEY`
- **Format:** `BSA...`
- **Sign up:** https://brave.com/search/api/

#### Google Custom Search
- **Keys:** `GOOGLE_SEARCH_API_KEY` + `GOOGLE_SEARCH_ENGINE_ID`
- **Sign up:** https://developers.google.com/custom-search/v1/overview
- **Note:** This is separate from Google Gemini API
- **Setup:**
  1. Create API key in Google Cloud Console
  2. Create Custom Search Engine at https://programmablesearchengine.google.com/
  3. Use both values in credentials

### Optional Services

#### Ntfy (Push Notifications)
- **Keys:** `NTFY_API_KEY` + `NTFY_TOPIC`
- **Sign up:** https://ntfy.sh/
- **Purpose:** Push notifications for long-running tasks

## Advanced

### All Directory Variables

ikigai uses six directory variables:

**Install-time (PREFIX-dependent):**
- `IKIGAI_BIN_DIR` - Executables
- `IKIGAI_DATA_DIR` - Shared data files
- `IKIGAI_LIBEXEC_DIR` - Helper executables

**Runtime (XDG-aware):**
- `IKIGAI_CONFIG_DIR` - Configuration files
- `IKIGAI_CACHE_DIR` - Cache files
- `IKIGAI_STATE_DIR` - Persistent state

In production, runtime directories follow XDG Base Directory specification. In development, override with environment variables.

### Using Environment Variables

Instead of `credentials.json`, export environment variables:

```bash
export OPENAI_API_KEY="sk-proj-..."
export BRAVE_API_KEY="BSA..."
ikigai
```

Environment variables take precedence over `credentials.json`. Useful for:
- CI/CD pipelines
- Docker containers
- Development with direnv

### Security

**credentials.json permissions:**
- Must be `0600` (owner read/write only)
- ikigai warns on insecure permissions but continues
- Never commit credentials.json to version control

**Storing secrets:**
- Use `.gitignore` for credentials.json
- Consider password managers or secret vaults
- In production, prefer environment variables over files
