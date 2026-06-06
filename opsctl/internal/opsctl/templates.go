package opsctl

import (
	"fmt"
	"strings"
)

// unitFile renders the per-app systemd unit `setup` writes. It byte-matches what
// today's */bin/setup emits: the heredoc body below, written via `printf '%s\n'`
// (so a single trailing newline). ExecStart hands off to the stable launcher
// contract /usr/local/bin/ikigenba-launch <app> (PLAN §2.6); the unit is enabled
// but NOT started by setup.
//
// Verified against ledger/bin/setup: the produced ledger.service is 285 bytes
// with identical content.
func unitFile(app string) string {
	return fmt.Sprintf(`[Unit]
Description=%[1]s
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=%[1]s
WorkingDirectory=/opt/%[1]s
EnvironmentFile=/etc/ikigenba/env
ExecStart=/usr/local/bin/ikigenba-launch %[1]s
Restart=on-failure

[Install]
WantedBy=multi-user.target
`, app)
}

// renderApexBlock renders the apex nginx server{} block init-box writes. It takes
// the committed dashboard etc/nginx.conf SOURCE (with __DOMAIN__/__PORT__
// placeholders) and substitutes them, exactly as the old dashboard/bin/setup did
// (sed -e s/__DOMAIN__/…/g -e s/__PORT__/…/g then `printf '%s\n'`). The block
// carries the /_authn introspection hook and the
// `include /etc/nginx/conf.d/locations/*.conf;` of the per-app fragment dir.
func renderApexBlock(src, domain string, port int) string {
	body := strings.ReplaceAll(src, "__DOMAIN__", domain)
	body = strings.ReplaceAll(body, "__PORT__", fmt.Sprintf("%d", port))
	return strings.TrimRight(body, "\n") + "\n"
}

// renewTimer / renewService are the self-contained certbot renewal timer+service
// init-box writes (PLAN §D1: "the renew timer systemd unit"). The old
// dashboard/bin/setup only `enable --now`'d the package-provided
// certbot-renew.timer; init-box writes its OWN ikigenba-certbot-renew pair so the
// renewal cadence + deploy-hook are owned by the suite and the bytes are
// test-assertable. `certbot renew` is a no-op until a cert is near expiry.
const renewTimer = `[Unit]
Description=ikigenba certbot renewal timer

[Timer]
OnCalendar=*-*-* 00,12:00:00
RandomizedDelaySec=3600
Persistent=true

[Install]
WantedBy=timers.target
`

const renewService = `[Unit]
Description=ikigenba certbot renewal

[Service]
Type=oneshot
ExecStart=/usr/bin/certbot renew --quiet --deploy-hook "systemctl reload nginx"
`
