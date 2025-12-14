# Pillar 4: Web Portal

> This is the fourth of [four pillars](02-architecture.md). The Web Portal provides browser-based access for users who interact through a GUI rather than the [Terminal](05-terminal.md).

---

## Purpose

The web portal is the standard user interface to the platform. Following the same philosophy as the rest of Ikigai, the web layer uses Linux infrastructure: Nginx serves static frontends, and a platform backend API provides access to Ikigai systems. Developers additionally have access to Ikigai Terminal for conversational, agentic interactions.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                         Nginx                                     │
│                    (reverse proxy + static files)                 │
└──────────────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
    ┌─────────┐         ┌─────────┐         ┌─────────┐
    │    /    │         │ /app1/  │         │ /app2/  │
    │ (root)  │         │         │         │         │
    └─────────┘         └─────────┘         └─────────┘
         │                    │                    │
         └────────────────────┼────────────────────┘
                              ▼
                    ┌─────────────────┐
                    │  Backend API    │
                    │    (Deno)       │
                    └─────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │   PostgreSQL    │
                    │ (sessions,      │
                    │  queues, etc.)  │
                    └─────────────────┘
```

**Nginx** serves as the entry point:
- Routes requests to static webapp files in `/opt/ikigai/webapps.d/`
- Proxies API requests to the backend service
- Handles TLS termination

**Webapps** are Fresh applications (Deno's native web framework):
- Built on Preact with islands architecture (minimal JS by default)
- File-based routing in `routes/`, interactive components in `islands/`
- No build step required - runs TypeScript directly like agents
- Deployed to `/opt/ikigai/webapps.d/<appname>/<version>/`
- Mounted at URL paths (`/`, `/appname/`) based on nginx configuration
- All communicate with the single backend API

**Backend API** is a platform service (Deno/TypeScript):
- Provides authenticated access to queues, mailboxes, telemetry, and other Ikigai systems
- Handles session management
- Runs as a systemd service

---

## Authentication

Web users authenticate with Linux credentials. The authentication flow:

1. User submits username/password via login form
2. Backend validates credentials against PAM
3. On success, backend creates a session record in PostgreSQL
4. Session cookie is issued to the browser
5. Subsequent requests include the cookie; backend validates against PostgreSQL

This maintains the "identity is Linux users" principle. Web users are Linux users authenticating through a browser instead of SSH.

---

## Webapp Structure

Each webapp is a Fresh application with versioned directories (shallow git clones of tags):

```
/opt/ikigai/webapps.d/
├── dashboard/
│   ├── v1.0.0/
│   │   ├── manifest.json      # Ikigai metadata
│   │   ├── deno.json          # Fresh config
│   │   ├── deno.lock
│   │   ├── fresh.gen.ts       # Auto-generated routes
│   │   ├── routes/
│   │   │   ├── index.tsx      # /
│   │   │   ├── _app.tsx       # Layout wrapper
│   │   │   └── api/           # API routes (optional)
│   │   ├── islands/           # Interactive components
│   │   └── static/            # Static assets
│   └── v1.1.0/
│       └── ...
└── monitoring/
    ├── v2.0.0/
    │   └── ...
    └── v2.1.0/
        └── ...
```

No `current` symlink. Nginx configuration directly controls which versions are served.

---

## Nginx Configuration

### Per-Webapp Configs

Each webapp gets its own nginx config fragment:

```
/etc/ikigai/nginx.d/
├── dashboard.conf
└── monitoring.conf
```

**dashboard.conf** - serves root path with traffic split:

```nginx
split_clients "${remote_addr}" $dashboard_version {
    10%  "v1.0.0";
    *    "v1.1.0";
}

location / {
    root /opt/ikigai/webapps.d/dashboard/$dashboard_version;
    try_files $uri $uri/ /index.html;
}
```

**monitoring.conf** - serves `/monitoring/` with its own independent split:

```nginx
split_clients "${remote_addr}" $monitoring_version {
    50%  "v2.0.0";
    *    "v2.1.0";
}

location /monitoring/ {
    alias /opt/ikigai/webapps.d/monitoring/$monitoring_version/;
    try_files $uri $uri/ /monitoring/index.html;
}
```

### Aggregator Config

The main nginx config includes all webapp fragments:

```nginx
# /etc/nginx/sites-enabled/ikigai.conf

server {
    listen 443 ssl;
    server_name example.com;

    ssl_certificate /etc/ikigai/ssl/cert.pem;
    ssl_certificate_key /etc/ikigai/ssl/key.pem;

    # Include all webapp configs
    include /etc/ikigai/nginx.d/*.conf;

    # Backend API proxy
    location /api/ {
        proxy_pass http://127.0.0.1:8000/;
    }
}
```

### Independent Traffic Splits

Each webapp's traffic split is completely independent:

| Webapp | Path | v1 | v2 | Notes |
|--------|------|-----|-----|-------|
| dashboard | `/` | 10% (v1.0.0) | 90% (v1.1.0) | Canary rollout |
| monitoring | `/monitoring/` | 50% (v2.0.0) | 50% (v2.1.0) | A/B test |

Adjusting dashboard weights has no effect on monitoring, and vice versa.

---

## Traffic Split Behavior

The `split_clients` directive hashes `$remote_addr` (client IP), so:

- Same client consistently sees the same version (sticky)
- Percentage is across all clients, not per-request
- Useful for canary deployments where you want consistent user experience

For per-request randomization, use `$request_id` instead:

```nginx
split_clients "${request_id}" $dashboard_version {
    10%  "v1.0.0";
    *    "v1.1.0";
}
```

---

## Deployment Workflow

```
> deploy dashboard v1.1.0
Cloning v1.1.0... done

> set traffic dashboard v1.0.0=10 v1.1.0=90
Updating nginx config... done
Reloading nginx... done

Dashboard traffic: v1.0.0 (10%), v1.1.0 (90%)

> set traffic dashboard v1.1.0=100
Updating nginx config... done
Reloading nginx... done

Dashboard traffic: v1.1.0 (100%)
```

Rollback is just changing the traffic split back.

---

## Webapp Routing Summary

| Path | Webapp | Source |
|------|--------|--------|
| `/` | `dashboard` | Configurable, default provided by platform |
| `/monitoring/` | `monitoring` | Derived from webapp name |
| `/admin/` | `admin` | Derived from webapp name |
| `/api/` | Backend API | Proxied to Deno service |

The root path (`/`) serves a platform-provided dashboard by default. This is replaceable. Any webapp can be configured as the root.

---

**Next**: The four pillars share common infrastructure. Continue to [Part V: Cross-Cutting Concerns](09-filesystem.md) for details on filesystem layout, security, deployment, and observability.
