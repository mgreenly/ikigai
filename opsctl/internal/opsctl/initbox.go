package opsctl

import (
	"context"
	"fmt"
	"os"
)

// InitBoxOptions parameterises `opsctl init-box` — the box-GLOBAL substrate (PLAN
// §D1, ADR "init-box vs setup"). It carries the apex routing the old
// dashboard/bin/setup sourced from etc/manifest.env + etc/deploy.env so init-box
// can emit a byte-identical apex server block.
type InitBoxOptions struct {
	// DefaultApp is the apex/DEFAULT app's name (today "dashboard"). The apex
	// server block lands at conf.d/<DefaultApp>.conf and the apex app's loopback
	// port is what /_authn proxies to.
	DefaultApp string
	Domain     string // apex domain (e.g. int.ikigenba.com) — __DOMAIN__ in the block
	Port       int    // the apex app's loopback PORT — __PORT__ in the block
	Email      string // CERTBOT_EMAIL — for HTTP-01 cert issuance
	ApexBlock  string // the apex nginx server{} SOURCE (committed dashboard
	// etc/nginx.conf, with __DOMAIN__/__PORT__ placeholders).
	// SkipCert short-circuits the certbot call (the box ops are still recorded);
	// init-box is otherwise idempotent and certbot reuses a live cert.
	SkipCert bool
}

// InitBox provisions the one-time, box-global substrate the apex used to
// bootstrap inside dashboard/bin/setup — split out so per-app setup never reaches
// for global state (PLAN §D1):
//
//  1. install nginx + certbot (seam),
//  2. create the conf.d/locations/ include dir + the letsencrypt webroot,
//  3. write the apex nginx server{} block (with /_authn + the locations include),
//  4. nginx -t + enable+reload nginx (seam),
//  5. obtain the apex TLS cert via certbot HTTP-01 webroot (seam),
//  6. write + enable-now the certbot renewal timer.
//
// Config artifacts (the apex block, the renew timer/service) are WRITTEN to
// SysRoot-rooted paths so tests byte-assert them; the imperative box ops (package
// install, nginx validate/reload, certbot, timer enable) go through the System
// seam. It is idempotent and per-box, not per-app.
func (o *Opsctl) InitBox(ctx context.Context, opts InitBoxOptions) error {
	if opts.DefaultApp == "" {
		return fmt.Errorf("init-box: default app name is required")
	}
	if opts.Domain == "" {
		return fmt.Errorf("init-box: domain is required")
	}
	if opts.ApexBlock == "" {
		return fmt.Errorf("init-box: apex nginx block source is required")
	}
	l := o.layout(opts.DefaultApp)

	// 1. Packages: nginx + certbot (the launcher's deps awscli-2/jq are already
	//    present from instance bootstrap, so init-box does not install them).
	o.logf("install nginx + certbot")
	if err := o.System.InstallPackages(ctx, "nginx", "certbot"); err != nil {
		return fmt.Errorf("init-box: install packages: %w", err)
	}

	// 2. The box-global include dir + the HTTP-01 webroot.
	o.logf("create %s + %s", l.LocationsDir(), l.LetsEncryptWebroot())
	if err := mkdirAll755(l.NginxConfDir(), l.LocationsDir(), l.LetsEncryptWebroot()); err != nil {
		return fmt.Errorf("init-box: create dirs: %w", err)
	}

	// 3. The apex server block (carries /_authn + the locations include).
	o.logf("write apex nginx block %s", l.ApexBlockPath())
	block := renderApexBlock(opts.ApexBlock, opts.Domain, opts.Port)
	if err := writeFileAtomic(l.ApexBlockPath(), []byte(block), 0o644); err != nil {
		return fmt.Errorf("init-box: write apex block: %w", err)
	}

	// 4 + 5. Bring nginx up and obtain the apex TLS cert — UNLESS --skip-cert.
	//
	// The apex block's 443 server references the apex cert by path, so `nginx -t`
	// (and therefore enable/reload) cannot succeed until that cert EXISTS. This is
	// a chicken-and-egg on a greenfield box: nginx can't validate without the
	// cert, but the usual HTTP-01 webroot issuance needs nginx already serving
	// :80. We break it by bootstrapping the FIRST cert via certbot --standalone
	// (certbot binds :80 itself, nginx not running) BEFORE `nginx -t`. Once the
	// cert is on disk, nginx -t passes and we enable+reload. On reruns the cert
	// already exists, so we skip the standalone bootstrap and just (re)validate +
	// reload nginx; the renewal timer owns ongoing renewals via webroot.
	//
	// --skip-cert stages the block WITHOUT validating/starting nginx or issuing a
	// cert (the block + locations dir are still written above); used to defer the
	// whole cert/nginx bring-up.
	if !opts.SkipCert {
		if opts.Email == "" {
			return fmt.Errorf("init-box: certbot email is required (set --email or --skip-cert)")
		}
		// 4. Bootstrap the FIRST apex cert via standalone (no nginx) if it does
		//    not exist yet — this lets the apex :443 block validate below.
		if !o.System.CertExists(opts.Domain) {
			o.logf("bootstrap first apex cert for %s (certbot --standalone, nginx not yet up)", opts.Domain)
			if err := o.System.ObtainCertStandalone(ctx, opts.Domain, opts.Email); err != nil {
				return fmt.Errorf("init-box: bootstrap apex cert: %w", err)
			}
		} else {
			o.logf("apex cert for %s already present — skipping standalone bootstrap", opts.Domain)
		}
		// 5. With the cert on disk, validate + bring nginx up.
		if err := o.System.NginxTest(ctx); err != nil {
			return fmt.Errorf("init-box: nginx -t: %w", err)
		}
		if err := o.System.EnableUnit(ctx, "nginx", true); err != nil {
			return fmt.Errorf("init-box: enable nginx: %w", err)
		}
		if err := o.System.NginxReload(ctx); err != nil {
			return fmt.Errorf("init-box: nginx reload: %w", err)
		}
		// 6. Reconcile the renewal method to HTTP-01 webroot now that nginx serves
		//    :80 (the ACME-challenge location). The first cert was bootstrapped via
		//    --standalone, which would otherwise pin renewals to :80-binding
		//    standalone and collide with the running nginx. certbot sees the live
		//    cert and does NOT re-issue; it just rewrites the renewal config to
		//    webroot. Idempotent on reruns.
		o.logf("reconcile apex cert renewal to webroot for %s", opts.Domain)
		if err := o.System.ObtainCert(ctx, opts.Domain, opts.Email, l.LetsEncryptWebroot()); err != nil {
			return fmt.Errorf("init-box: reconcile cert renewal: %w", err)
		}
	} else {
		o.logf("skip-cert: staging apex block only (nginx not validated/started; cert issued later)")
	}

	// 6. The suite-owned timers, enabled now: certbot renewal and the nightly
	//    backup sweep.
	o.logf("write + enable renewal timer %s", l.RenewTimerPath())
	if err := writeFileAtomic(l.RenewServicePath(), []byte(renewService), 0o644); err != nil {
		return fmt.Errorf("init-box: write renew service: %w", err)
	}
	if err := writeFileAtomic(l.RenewTimerPath(), []byte(renewTimer), 0o644); err != nil {
		return fmt.Errorf("init-box: write renew timer: %w", err)
	}
	o.logf("write + enable backup timer %s", l.BackupTimerPath())
	if err := writeFileAtomic(l.BackupServicePath(), []byte(backupService), 0o644); err != nil {
		return fmt.Errorf("init-box: write backup service: %w", err)
	}
	if err := writeFileAtomic(l.BackupTimerPath(), []byte(backupTimer), 0o644); err != nil {
		return fmt.Errorf("init-box: write backup timer: %w", err)
	}
	if err := o.System.DaemonReload(ctx); err != nil {
		return fmt.Errorf("init-box: daemon-reload: %w", err)
	}
	if err := o.System.EnableUnit(ctx, "ikigenba-certbot-renew.timer", true); err != nil {
		return fmt.Errorf("init-box: enable renew timer: %w", err)
	}
	if err := o.System.EnableUnit(ctx, "ikigenba-backup.timer", true); err != nil {
		return fmt.Errorf("init-box: enable backup timer: %w", err)
	}

	o.logf("init-box complete — next: opsctl setup <app> per service")
	return nil
}

// LoadApexBlockFile reads the apex nginx block source (the committed dashboard
// etc/nginx.conf) for the CLI path.
func LoadApexBlockFile(path string) (string, error) {
	if path == "" {
		return "", fmt.Errorf("init-box: --apex-block <path> is required")
	}
	b, err := os.ReadFile(path)
	if err != nil {
		return "", fmt.Errorf("init-box: read apex block %s: %w", path, err)
	}
	return string(b), nil
}
