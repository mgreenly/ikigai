# Configuration

## Overview

ikigai runs with built-in defaults and requires no configuration files to start. However, it requires API credentials to be useful:

- **At least one AI provider** (OpenAI, Anthropic, or Google)

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

### .envrc

The `.envrc` file (used with [direnv](https://direnv.net/)) configures the development environment. Run `direnv allow` once after cloning to activate it.

**Development flag:**

| Variable | Value | Description |
|----------|-------|-------------|
| `IKIGAI_DEV` | `1` | Enables development mode |

**Directories:**

| Variable | Value | Description |
|----------|-------|-------------|
| `IKIGAI_BIN_DIR` | `$PWD/bin` | Compiled executables |
| `IKIGAI_CONFIG_DIR` | `$PWD/etc` | Configuration files |
| `IKIGAI_DATA_DIR` | `$PWD/share` | Shared data files |
| `IKIGAI_LIBEXEC_DIR` | `$PWD/libexec` | Helper executables |
| `IKIGAI_CACHE_DIR` | `$PWD/cache` | Cache files |
| `IKIGAI_STATE_DIR` | `$PWD/state` | Persistent state |
| `IKIGAI_RUNTIME_DIR` | `$PWD/run` | Runtime files (sockets, PIDs) |
| `IKIGAI_LOG_DIR` | `$PWD/.ikigai/logs` | Log files |

**Database:**

| Variable | Value | Description |
|----------|-------|-------------|
| `IKIGAI_DB_HOST` | `localhost` | PostgreSQL host |
| `IKIGAI_DB_PORT` | `5432` | PostgreSQL port |
| `IKIGAI_DB_NAME` | *(derived from project dir)* | Database name |
| `IKIGAI_DB_USER` | `ikigai` | Database user |
| `IKIGAI_DB_PASS` | *(from `~/.secrets/IKIGAI_DB_PASS`)* | Database password |

**AI provider API keys:**

| Variable | Secret file | Description |
|----------|-------------|-------------|
| `OPENAI_API_KEY` | `~/.secrets/OPENAI_API_KEY` | OpenAI API key |
| `ANTHROPIC_API_KEY` | `~/.secrets/ANTHROPIC_API_KEY` | Anthropic API key |
| `GOOGLE_API_KEY` | `~/.secrets/GOOGLE_API_KEY` | Google Gemini API key |

**Search:**

| Variable | Secret file | Description |
|----------|-------------|-------------|
| `BRAVE_API_KEY` | `~/.secrets/BRAVE_API_KEY` | Brave Search API key |
| `GOOGLE_SEARCH_API_KEY` | `~/.secrets/GOOGLE_SEARCH_API_KEY` | Google Custom Search API key |
| `GOOGLE_SEARCH_ENGINE_ID` | `~/.secrets/GOOGLE_SEARCH_ENGINE_ID` | Google Custom Search Engine ID |

**Notifications:**

| Variable | Secret file | Description |
|----------|-------------|-------------|
| `NTFY_API_KEY` | `~/.secrets/NTFY_API_KEY` | ntfy.sh API key |
| `NTFY_TOPIC` | `~/.secrets/NTFY_TOPIC` | ntfy.sh notification topic |

**Google Chat (optional):**

| Variable | Value | Description |
|----------|-------|-------------|
| `GOOGLE_CHAT_SPACE` | *(space ID)* | Google Chat space for notifications |
| `GOOGLE_CHAT_USER` | *(service account email)* | Google Chat sender identity |
| `GOOGLE_APPLICATION_CREDENTIALS` | `~/.secrets/ikigai-dev-*.iam.gserviceaccount.com` | Google service account credentials file |

**Ralph agent system:**

| Variable | Value | Description |
|----------|-------|-------------|
| `RALPH_PLANS_HOST` | `localhost` | Ralph plans service host |
| `RALPH_PLANS_PORT` | `5001` | Ralph plans service port |
| `RALPH_ORG` | *(GitHub org)* | GitHub organization for goal tracking |
| `RALPH_REPO` | *(repository name)* | GitHub repository for goal tracking |

**PATH additions:**

| Directory | Purpose |
|-----------|---------|
| `scripts/bin` | Goal management scripts |
| `.claude/bin` | Claude agent binaries |
| `.claude/scripts` | Quality harness scripts |
| `.ikigai/scripts` | ikigai helper scripts |
| `.ralph` | Ralph check scripts |

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
