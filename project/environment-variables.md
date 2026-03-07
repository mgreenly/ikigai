# Environment Variables Reference

This document describes every environment variable that ikigai reads at runtime. Variables are organized by category. All `getenv()` calls in `apps/` and `shared/` are covered here.

---

## Runtime Paths

These seven path variables are **required** at startup. If any is missing or empty, ikigai exits with an error. The production wrapper script (`~/.local/bin/ikigai`) sets them automatically using XDG Base Directory conventions. The development `.envrc` sets them to source-tree subdirectories.

| Variable | File / Function | What it controls | Default (installed) | Example (dev) |
|---|---|---|---|---|
| `HOME` | `apps/ikigai/paths.c` · `ik_paths_expand_tilde()` | User home directory; used to expand `~` in tool paths and credentials path | Set by the OS | `/home/alice` |
| `IKIGAI_BIN_DIR` | `apps/ikigai/paths.c` · `ik_paths_init()` | Directory containing the `ikigai` wrapper script | `~/.local/bin` | `$PWD/bin` |
| `IKIGAI_CONFIG_DIR` | `apps/ikigai/paths.c` · `ik_paths_init()`, `shared/credentials.c` · `ik_credentials_load()` | Configuration directory; `credentials.json` is loaded from here | `~/.config/ikigai` | `$PWD/etc` |
| `IKIGAI_DATA_DIR` | `apps/ikigai/paths.c` · `ik_paths_init()`, `apps/ikigai/config.c` · `ik_config_load()` | Data directory; contains database migrations and the optional `prompts/system.md` override | `~/.local/share/ikigai` | `$PWD/share` |
| `IKIGAI_LIBEXEC_DIR` | `apps/ikigai/paths.c` · `ik_paths_init()` | Helper executables directory; system-installed tools are discovered here | `~/.local/libexec/ikigai` | `$PWD/libexec` |
| `IKIGAI_CACHE_DIR` | `apps/ikigai/paths.c` · `ik_paths_init()` | Cache directory; safe to delete | `~/.cache/ikigai` | `$PWD/cache` |
| `IKIGAI_STATE_DIR` | `apps/ikigai/paths.c` · `ik_paths_init()` | Persistent state directory | `~/.local/state/ikigai` | `$PWD/state` |
| `IKIGAI_RUNTIME_DIR` | `apps/ikigai/paths.c` · `ik_paths_init()` | Runtime directory; control sockets and PID files are placed here | `/run/user/<uid>/ikigai` | `$PWD/run` |

---

## Logging

| Variable | File / Function | What it controls | Default when unset | Example |
|---|---|---|---|---|
| `IKIGAI_LOG_DIR` | `apps/ikigai/debug_log.c` · `ik_debug_log_init()` (debug builds only), `shared/logger.c` · `ik_log_setup_directories()` | Directory where log files are written. In debug builds the variable is required and ikigai panics if unset. In release builds, logger falls back to a subdirectory of the working directory | None (panics in debug builds) | `$PWD/.ikigai/logs` |

---

## Database Configuration

These variables override the compiled-in defaults. All are optional; the defaults work for a standard local PostgreSQL installation.

| Variable | File / Function | What it controls | Default | Example |
|---|---|---|---|---|
| `IKIGAI_DB_HOST` | `apps/ikigai/config_env.c` · `ik_config_apply_env_overrides()` | PostgreSQL host | `localhost` | `db.example.com` |
| `IKIGAI_DB_PORT` | `apps/ikigai/config_env.c` · `ik_config_apply_env_overrides()` | PostgreSQL port (1–65535) | `5432` | `5433` |
| `IKIGAI_DB_NAME` | `apps/ikigai/config_env.c` · `ik_config_apply_env_overrides()` | Database name | `ikigai` | `ikigai_dev` |
| `IKIGAI_DB_USER` | `apps/ikigai/config_env.c` · `ik_config_apply_env_overrides()` | Database user | `ikigai` | `myuser` |
| `IKIGAI_DB_PASS` | `shared/credentials.c` · `ik_credentials_load()` | Database password. Also loadable from `credentials.json`; env takes priority | None | `s3cr3t` |

---

## Provider Configuration

| Variable | File / Function | What it controls | Default | Example |
|---|---|---|---|---|
| `IKIGAI_DEFAULT_PROVIDER` | `apps/ikigai/config.c` · `ik_config_get_default_provider()` | Which AI provider to use when no `--provider` flag is given. Checked at every call; overrides both `credentials.json` and compiled default. Valid values: `anthropic`, `openai`, `google` | `openai` | `anthropic` |
| `IKIGAI_SLIDING_CONTEXT_TOKENS` | `apps/ikigai/config_env.c` · `ik_config_apply_env_overrides()` | Token budget for the sliding context window. Must be a positive integer | `100000` | `200000` |

---

## API Keys (Credentials)

API keys can be supplied via environment variable **or** via `$IKIGAI_CONFIG_DIR/credentials.json`. Environment variables take priority over the file. Empty values are treated as unset.

| Variable | File / Function | What it controls | Default | Example |
|---|---|---|---|---|
| `ANTHROPIC_API_KEY` | `shared/credentials.c` · `ik_credentials_load()` | Anthropic Claude API key | None | `sk-ant-...` |
| `OPENAI_API_KEY` | `shared/credentials.c` · `ik_credentials_load()` | OpenAI API key | None | `sk-proj-...` |
| `GOOGLE_API_KEY` | `shared/credentials.c` · `ik_credentials_load()` | Google Gemini API key | None | `AIza...` |
| `BRAVE_API_KEY` | `shared/credentials.c` · `ik_credentials_load()` | Brave Search API key (used by Brave search tool) | None | `BSA...` |
| `GOOGLE_SEARCH_API_KEY` | `shared/credentials.c` · `ik_credentials_load()` | Google Custom Search API key (used by Google search tool) | None | `AIza...` |
| `GOOGLE_SEARCH_ENGINE_ID` | `shared/credentials.c` · `ik_credentials_load()` | Google Custom Search Engine ID | None | `abc123...` |
| `NTFY_API_KEY` | `shared/credentials.c` · `ik_credentials_load()` | ntfy.sh push notification API key | None | `tk_...` |
| `NTFY_TOPIC` | `shared/credentials.c` · `ik_credentials_load()` | ntfy.sh push notification topic | None | `my-alerts` |

---

## Provider Base URL Overrides

These variables redirect provider API calls to an alternative endpoint. They are intended for testing (e.g., pointing at a local mock server) and are not needed in production.

| Variable | File / Function | What it controls | Default | Example |
|---|---|---|---|---|
| `ANTHROPIC_BASE_URL` | `apps/ikigai/providers/anthropic/anthropic.c` · `ik_anthropic_create()` | Base URL for Anthropic API requests | `https://api.anthropic.com` | `http://localhost:8080` |
| `OPENAI_BASE_URL` | `apps/ikigai/providers/openai/openai.c` · `ik_openai_create_with_options()` | Base URL for OpenAI API requests | `https://api.openai.com` | `http://localhost:8080` |
| `GOOGLE_BASE_URL` | `apps/ikigai/providers/google/google.c` · `ik_google_create()` | Base URL for Google AI API requests | `https://generativelanguage.googleapis.com/v1beta` | `http://localhost:8080` |

---

## Terminal and Display

| Variable | File / Function | What it controls | Default | Example |
|---|---|---|---|---|
| `NO_COLOR` | `apps/ikigai/ansi.c` · `ik_ansi_init()` | Disables all ANSI color output when set to any non-null value, including empty string. Follows the [no-color.org](https://no-color.org/) convention | Unset (colors enabled) | `1` or `` (empty) |
| `TERM` | `apps/ikigai/ansi.c` · `ik_ansi_init()` | If set to `dumb`, disables all ANSI color output | Unset | `dumb` |

---

## Template Variables

Pinned documents (the stacked system-prompt blocks attached to an agent) support a template syntax. Any environment variable can be injected into a document using the `env.` namespace:

```
{{env.VAR_NAME}}
```

This is handled in `apps/ikigai/template.c` · `resolve_variable()`. The lookup calls `getenv(var + 4)` where `var` is the full `env.VAR_NAME` string, so any variable in the process environment is accessible. If the variable is unset the placeholder expands to nothing (the token is silently dropped).

**Example** — referencing the current provider in a pinned document:

```markdown
You are running on the {{env.IKIGAI_DEFAULT_PROVIDER}} provider.
```
