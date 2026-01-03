# Docker Compose Deployment

## Intent

Provide a way for users to try and use ikigai on systems where it doesn't natively build (macOS, Windows, non-Debian Linux distributions). Docker Compose creates a fully-configured Debian environment with all dependencies (PostgreSQL, build tools, etc.) that works identically across platforms.

## Use Cases

1. **Evaluation** - Try ikigai without installing dependencies or setting up PostgreSQL
2. **Cross-platform development** - Mac/Windows users contributing to the project
3. **Reproducible environments** - Consistent setup for testing, CI/CD, or demonstrations
4. **Isolated experimentation** - Test changes without affecting host system

## Architecture

### Services

**ikigai** (Debian bookworm):
- Builds from project Dockerfile
- Interactive shell access via `docker compose exec`
- Volume-mounted config directory for secrets
- Persistent data directory for user files
- Connected to PostgreSQL via Docker networking

**postgres** (PostgreSQL 16):
- Named volume for data persistence
- Pre-configured connection credentials
- Accessible from ikigai service via compose DNS

### Directory Structure

```
project-root/
├── compose.yaml           # Docker Compose configuration
├── Dockerfile            # Debian build environment
├── docker/
│   └── config/          # User config (gitignored)
│       └── config.json  # Contains API keys (user creates)
└── etc/ikigai/
    └── config.json      # Template for users to copy
```

## Key Decisions

### Secrets Management

**Decision:** Volume mount config directory from host

**Rationale:**
- Secrets never baked into Docker images
- Config lives on host filesystem, easy to edit
- Read-only mount prevents container from corrupting config
- Forward compatible with planned config/credentials split
- Users can mount their real `~/.config/ikigai` if desired

**Alternative considered:** Environment variables
- Rejected: Doesn't match ikigai's file-based config model
- Rejected: Would require dual config systems (files + env vars)

### Data Persistence

**Decision:** Named Docker volumes for PostgreSQL data and ikigai data

**Rationale:**
- Database persists across container restarts
- Users can build up history, test migrations, iterate on work
- Volume data survives `docker compose down` (only deleted with `-v` flag)
- Docker-managed volumes require no host filesystem knowledge

**Alternative considered:** Bind mount to host directory
- Rejected: Path handling varies across Mac/Windows/Linux
- Rejected: Permission issues with different host OSes
- Kept as option via `IKIGAI_DATA_DIR` environment variable override

### Database Configuration

**Decision:** Hardcoded credentials in compose.yaml, environment variable override for connection string

**Rationale:**
- Development/evaluation use case doesn't need secure DB credentials
- Simplified setup (no separate database config step)
- Connection string via environment variable overrides config.json
- Production deployments would use different approach anyway

## Example compose.yaml

```yaml
services:
  ikigai:
    image: ikigai:latest
    build:
      context: .
      dockerfile: Dockerfile
    stdin_open: true
    tty: true
    volumes:
      # Config with secrets (user creates on host)
      - ${IKIGAI_CONFIG_DIR:-./docker/config}:/root/.config/ikigai:ro
      # Persistent data directory
      - ikigai-data:/root/.local/share/ikigai
    depends_on:
      - postgres
    environment:
      # Override DB connection to use compose networking
      DATABASE_URL: postgres://ikigai:ikigai@postgres:5432/ikigai

  postgres:
    image: postgres:16
    environment:
      POSTGRES_DB: ikigai
      POSTGRES_USER: ikigai
      POSTGRES_PASSWORD: ikigai
    volumes:
      - postgres-data:/var/lib/postgresql/data
    ports:
      - "5432:5432"  # Optional: expose for external tools

volumes:
  postgres-data:
  ikigai-data:
```

## User Workflow

### Initial Setup

```bash
# 1. Clone repository
git clone https://github.com/user/ikigai
cd ikigai

# 2. Create config directory
mkdir -p docker/config

# 3. Copy template and add API key
cp etc/ikigai/config.json docker/config/config.json
# Edit docker/config/config.json - replace YOUR_API_KEY_HERE

# 4. Start services
docker compose up -d
```

### Daily Usage

```bash
# Enter interactive environment
docker compose exec ikigai /bin/bash

# Inside container:
make BUILD=release
./build/release/ikigai --help
./build/release/ikigai

# Exit container (Ctrl-D or exit)

# Stop services when done
docker compose down
```

### Data Management

```bash
# Stop services, keep data
docker compose down

# Stop services and DELETE all data (fresh start)
docker compose down -v

# View logs
docker compose logs ikigai
docker compose logs postgres

# Restart services
docker compose restart
```

## Benefits

1. **Works everywhere Docker runs** - Mac, Windows, Linux, cloud VMs
2. **No dependency installation** - Dockerfile handles all build requirements
3. **Isolated** - Won't conflict with host system packages or PostgreSQL
4. **Reproducible** - Same environment for all users
5. **Pre-configured** - Database connection works out of the box
6. **Persistent** - Data survives restarts
7. **Efficient** - Layers cached, volumes reused

## Tradeoffs

1. **Requires Docker** - Additional system dependency
2. **Slower than native** - Docker overhead (usually negligible for CLI tools)
3. **Extra step** - Users must create config directory and add API keys
4. **Volume inspection harder** - Data in Docker volumes vs plain directories

## Forward Compatibility

When config/credentials split is implemented (see `project/backlog/config-credentials-split.md`):

```yaml
volumes:
  - ./docker/config/config.json:/root/.config/ikigai/config.json:ro
  - ./docker/config/credentials.json:/root/.config/ikigai/credentials.json:ro
```

Ship `credentials.example.json` template, users copy and populate.

## Future Enhancements

- Pre-built images on Docker Hub/GHCR (skip build step)
- Multiple compose profiles (dev vs evaluation)
- Init script to seed example database
- Health checks for automatic readiness detection
- Multi-architecture images (ARM64 for Apple Silicon)
