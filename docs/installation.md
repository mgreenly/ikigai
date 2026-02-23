# Installation

**Warning: ikigai is experimental software. It is not yet at tech demo stage and is 100% unsupported. Use at your own risk.**

## Requirements

- Debian Stable (only verified platform)
- PostgreSQL 15+ (for persistent storage)

## System Dependencies

Install build tools and required libraries:

```bash
sudo apt update
sudo apt install -y \
    gcc make \
    libtalloc-dev \
    uuid-dev \
    libb64-dev \
    libutf8proc-dev \
    libcurl4-openssl-dev \
    libpq-dev \
    libxkbcommon-dev \
    libxml2-dev \
    postgresql postgresql-contrib \
    direnv
```

## Download

```bash
git clone https://github.com/mgreenly/ikigai.git
cd ikigai
```

## Development Setup

For development, the project uses `direnv` to manage environment variables via `.envrc`. This file reads secrets from `~/.secrets/`.

### 1. Set Up Secrets Directory

Create the secrets directory and populate with your API keys:

```bash
mkdir -p ~/.secrets
chmod 700 ~/.secrets

# Database password (choose your own)
echo "your_db_password" > ~/.secrets/IKIGAI_DB_PASS

# AI Provider API Keys (at least one required)
echo "sk-..." > ~/.secrets/OPENAI_API_KEY
echo "sk-ant-..." > ~/.secrets/ANTHROPIC_API_KEY
echo "..." > ~/.secrets/GOOGLE_API_KEY

# Web Search API Keys (optional)
echo "..." > ~/.secrets/BRAVE_API_KEY
echo "..." > ~/.secrets/GOOGLE_SEARCH_API_KEY
echo "..." > ~/.secrets/GOOGLE_SEARCH_ENGINE_ID

# Push Notifications via ntfy.sh (optional)
echo "..." > ~/.secrets/NTFY_API_KEY
echo "..." > ~/.secrets/NTFY_TOPIC

# Secure the files
chmod 600 ~/.secrets/*
```

### 2. Set Up PostgreSQL

Create the database user and database:

```bash
sudo -u postgres createuser -P ikigai
# Enter the same password you put in ~/.secrets/IKIGAI_DB_PASS

sudo -u postgres createdb -O ikigai ikigai3
```

Note: The database name is derived from the project directory name (with hyphens removed). If your clone is in `ikigai-3/`, the database will be `ikigai3`.

### 3. Enable direnv

Add to your `~/.bashrc` or `~/.zshrc`:

```bash
eval "$(direnv hook bash)"  # or zsh
```

Then allow the project's `.envrc`:

```bash
cd /path/to/ikigai
direnv allow
```

### 4. Build

```bash
make
```

### 5. Run from Source

With direnv active, run directly from the build directory:

```bash
./bin/ikigai
```

## Build and Install

```bash
make && make install
```

This installs to `~/.local` (XDG conventions):
- Wrapper script: `~/.local/bin/ikigai`
- Actual binary: `~/.local/libexec/ikigai/ikigai`
- Tool binaries: `~/.local/libexec/ikigai/`
- Data files: `~/.local/share/ikigai/`
- Config template: `~/.config/ikigai/credentials.example.json`

Ensure `~/.local/bin` is in your `PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Add this line to your `~/.bashrc` or `~/.zshrc` to make it permanent.

## Uninstall

```bash
make uninstall
```

## Configuration

After installation, you must configure API credentials before ikigai can be useful.

See [Configuration](../docs/configuration.md) for:
- Setting up `~/.config/ikigai/credentials.json`
- Configuring AI providers (OpenAI, Anthropic, Google)
- Configuring web search providers (Brave, Google Custom Search)

## Building Your System Prompt

ikigai uses a **pinned document** system to build the effective system prompt sent to the AI. This allows you to customize the agent's behavior by assembling markdown files.

### How It Works

The system prompt is built from pinned documents in priority order:

1. **Pinned files** - Documents you pin during a session (highest priority)
2. **Default prompt** - `$IKIGAI_DATA_DIR/system/prompt.md` (fallback)
3. **Config fallback** - `openai_system_message` from `config.json`
4. **Hardcoded default** - Built-in minimal prompt

### Using /pin Commands

Pin documents to add them to your system prompt:

```
/pin path/to/document.md
```

List currently pinned documents:

```
/pin
```

Remove a pinned document:

```
/unpin path/to/document.md
```

### Creating Custom Prompts

You can create your own prompt documents and pin them to customize agent behavior. Documents are concatenated in the order they are pinned.

Example workflow:

```
/pin ik://system/prompt.md          # Start with default prompt
/pin ./my-project-context.md        # Add project-specific context
/pin ./coding-style.md              # Add coding style guidelines
```

The `ik://` URI scheme accesses ikigai's internal filesystem (`$IKIGAI_DATA_DIR/`), which contains the default system prompt and other shared resources.

### Pinned Documents Persist

Pin commands are persisted to the database and restored when you resume a session. This means your customized system prompt configuration survives across restarts.
