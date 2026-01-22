# Debian Service Dependencies

**Status:** Design for future implementation.

## Problem

Ikigai depends on nginx, runit, and PostgreSQL binaries but needs to manage them directly rather than using system services. By default, Debian packages auto-start and enable their services on installation.

**Requirements:**
- Depend on official Debian packages (no vendoring/bundling)
- Binaries available for ikigai to invoke with custom configs
- Services don't auto-start during installation
- Respect existing user configurations (if user already runs nginx, leave it alone)
- Both system services and ikigai's instances can coexist

## Solution

Use Debian maintainer scripts with:
1. **preinst** - Record existing service state, block service starts during install
2. **postinst** - Remove block, disable only freshly-installed services

### How Coexistence Works

Same binary, different configs, no conflict:

```bash
# System nginx (user's choice, if enabled)
/usr/sbin/nginx                     # /etc/nginx/nginx.conf, port 80

# Ikigai's nginx (completely isolated)
/usr/sbin/nginx -p ~/.ikigai -c ~/.ikigai/nginx.conf -g "daemon off;"
                                    # User directory, port 8080 or socket
```

Each instance has separate:
- Config files
- Ports/sockets
- PID files
- Log directories

## Implementation

### debian/preinst

```bash
#!/bin/sh
set -e

IKIGAI_STATE_DIR="/var/lib/ikigai"
SERVICES="nginx postgresql runit"

if [ "$1" = "install" ] || [ "$1" = "upgrade" ]; then
    mkdir -p "$IKIGAI_STATE_DIR"

    # Record which services were already enabled before we install
    for svc in $SERVICES; do
        if systemctl is-enabled "$svc" >/dev/null 2>&1; then
            touch "$IKIGAI_STATE_DIR/.${svc}-was-enabled"
        fi
    done

    # Block all service starts during this dpkg transaction
    # Exit code 101 = "action not allowed"
    echo "exit 101" > /usr/sbin/policy-rc.d
    chmod +x /usr/sbin/policy-rc.d
fi
```

### debian/postinst

```bash
#!/bin/sh
set -e

IKIGAI_STATE_DIR="/var/lib/ikigai"
SERVICES="nginx postgresql runit"

# Remove the service-start block
rm -f /usr/sbin/policy-rc.d

# Disable services that weren't previously enabled by user
for svc in $SERVICES; do
    if [ ! -f "$IKIGAI_STATE_DIR/.${svc}-was-enabled" ]; then
        systemctl disable "$svc" 2>/dev/null || true
    fi
    rm -f "$IKIGAI_STATE_DIR/.${svc}-was-enabled"
done

#DEBHELPER#
```

### debian/postrm

```bash
#!/bin/sh
set -e

# Clean up state directory on purge
if [ "$1" = "purge" ]; then
    rm -rf /var/lib/ikigai
    rm -f /usr/sbin/policy-rc.d
fi

#DEBHELPER#
```

### debian/control

```
Package: ikigai
Depends: nginx, postgresql, runit, ${misc:Depends}, ${shlibs:Depends}
```

## How It Works

### Fresh Install (user has no nginx)

1. User runs `apt install ikigai`
2. **preinst**: No existing services enabled, creates policy-rc.d block
3. **apt installs nginx**: Binary arrives at `/usr/sbin/nginx`, service blocked from starting
4. **apt installs postgresql**: Binary arrives, service blocked
5. **apt installs runit**: Binary arrives, service blocked
6. **postinst**: Removes block, disables all three services
7. **Result**: Binaries available, services disabled, ikigai manages them

### Existing nginx User

1. User already has nginx running on port 80
2. User runs `apt install ikigai`
3. **preinst**: Detects nginx enabled, creates marker file, creates policy-rc.d
4. **apt**: nginx already installed, no change
5. **postinst**: Sees marker file, leaves nginx enabled, removes marker
6. **Result**: User's nginx untouched, ikigai can still run its own instance on different port

## Service-Specific Notes

### nginx

```bash
# Ikigai invocation
/usr/sbin/nginx \
    -p /home/user/.ikigai \
    -c /home/user/.ikigai/etc/nginx.conf \
    -g "daemon off; pid /home/user/.ikigai/run/nginx.pid;"
```

- `-p` sets prefix for relative paths in config
- `-g "daemon off;"` runs in foreground
- Custom pid file prevents conflict with system instance

### PostgreSQL

```bash
# Ikigai invocation
/usr/lib/postgresql/16/bin/postgres \
    -D /home/user/.ikigai/pgdata \
    -k /home/user/.ikigai/run \
    -p 5433
```

- `-D` specifies data directory (must be initialized with `initdb`)
- `-k` specifies socket directory
- `-p` specifies port (use non-default to avoid conflict)

Note: PostgreSQL binary path includes version number.

### runit

```bash
# Ikigai invocation
/usr/bin/runsvdir /home/user/.ikigai/services
```

- runsvdir watches a directory for service subdirectories
- Each service has `run` script that runsvdir supervises
- Runs in foreground by default

## policy-rc.d Explained

Debian uses `/usr/sbin/policy-rc.d` to control whether init scripts (including systemd units) can start services during package operations.

| Exit Code | Meaning |
|-----------|---------|
| 0 | Action allowed |
| 101 | Action forbidden |
| 104 | Action forbidden, no fallback |

By creating a policy-rc.d that returns 101, we prevent any service from starting during the dpkg transaction. This is a standard Debian mechanism, commonly used in containers and custom deployments.

**Important:** Remove policy-rc.d in postinst, otherwise no services can start on the system.

## Caveats

1. **Upgrade behavior**: On upgrade, services that were disabled stay disabled. The preinst/postinst logic handles this correctly.

2. **Manual intervention**: If user manually enables a service after ikigai install, we don't touch it on future upgrades.

3. **PostgreSQL data directory**: Ikigai must initialize its own pgdata directory before first use:
   ```bash
   /usr/lib/postgresql/16/bin/initdb -D ~/.ikigai/pgdata
   ```

4. **Port conflicts**: Ikigai must use non-standard ports if system services are also running.

5. **Permissions**: Ikigai runs as user, not root. PostgreSQL and nginx support this fine with user-owned directories.

## Related

- [docker-compose.md](docker-compose.md) - Alternative deployment via containers
- [external-tool-architecture.md](external-tool-architecture.md) - External tools architecture
