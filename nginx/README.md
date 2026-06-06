# nginx — ikigenba suite dev front door

An unprivileged, foreground nginx that fronts the suite locally the way the apex
nginx block fronts it in production — so `localhost:8080/...` behaves like
`int.ikigenba.com/...`, minus TLS.

Why this exists: the auth contract (Phase 2b) puts nginx *between* the MCP client
and the services — `auth_request` to the dashboard's `/internal/authn`, path
stripping, and authoritative identity-header injection. None of that is app code,
so faithful local testing needs a real nginx in the path. This is it.

## Run

    ./run

Foreground; Ctrl-C to stop. apache2 owns `:80` on this box, so this listens on
`:8080`. All state (pid, logs, temp) stays under this directory — no root, and
the system `/etc/nginx` is untouched.

## Layout

    nginx.conf     full config (main/events/http) — :8080, plain http
    locations/     per-service fragments (mirrors prod conf.d/locations/);
                   crm.conf etc. drop here in Phase 2b
    logs/          access.log, error.log
    tmp/           nginx scratch (temp paths)

## Map (Phase 2b — enforcement landed)

    localhost:8080/                                          -> dashboard 127.0.0.1:3000
    localhost:8080/_authn                                    -> dashboard 127.0.0.1:3000/internal/authn  (internal)
    localhost:8080/srv/crm/.well-known/oauth-protected-resource -> crm 127.0.0.1:3001  (unauthenticated PRM bootstrap)
    localhost:8080/srv/crm/...                               -> auth_request /_authn -> crm 127.0.0.1:3001
