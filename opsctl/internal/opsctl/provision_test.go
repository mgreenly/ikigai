package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// newProvisioner builds an Opsctl over temp OPSCTL_ROOT + OPSCTL_SYSROOT with the
// recording stub system, for the D1 init-box / setup verbs.
func newProvisioner(t *testing.T, root, sysRoot string, sys *stubSystem) *Opsctl {
	t.Helper()
	return &Opsctl{
		Root:    root,
		SysRoot: sysRoot,
		Keep:    3,
		System:  sys,
		Runner:  fakeRunner{},
		Out:     &strings.Builder{},
		Err:     &strings.Builder{},
	}
}

// expectedUnit reproduces the EXACT systemd unit today's */bin/setup emits: the
// heredoc body written via `printf '%s\n'` (one trailing newline). The D1 split
// must not change a byte of this.
func expectedUnit(app string) string {
	return "[Unit]\n" +
		"Description=" + app + "\n" +
		"After=network-online.target\n" +
		"Wants=network-online.target\n" +
		"\n" +
		"[Service]\n" +
		"Type=simple\n" +
		"User=" + app + "\n" +
		"WorkingDirectory=/opt/" + app + "\n" +
		"EnvironmentFile=/etc/ikigenba/env\n" +
		"ExecStart=/usr/local/bin/ikigenba-launch " + app + "\n" +
		"Restart=on-failure\n" +
		"\n" +
		"[Install]\n" +
		"WantedBy=multi-user.target\n"
}

// shellSubst reproduces the old bin/setup's `FRAGMENT="$(sed s/__X__/v/g f)"`
// then `printf '%s\n'`: substitute placeholders, drop trailing newlines, add
// exactly one back. This is the ground-truth the D1 emit must byte-match.
func shellSubst(src string, repl map[string]string) string {
	body := src
	for k, v := range repl {
		body = strings.ReplaceAll(body, k, v)
	}
	return strings.TrimRight(body, "\n") + "\n"
}

func readRepoFile(t *testing.T, rel string) string {
	t.Helper()
	b, err := os.ReadFile(rel)
	if err != nil {
		t.Fatalf("read %s: %v", rel, err)
	}
	return string(b)
}

// initBox prepares a SysRoot so a subsequent setup finds conf.d/locations/. It is
// the box-global precondition setup depends on.
func runInitBox(t *testing.T, o *Opsctl, apexSrc string) {
	t.Helper()
	if err := o.InitBox(context.Background(), InitBoxOptions{
		DefaultApp: "dashboard",
		Domain:     "int.ikigenba.com",
		Port:       3000,
		Email:      "ops@example.com",
		ApexBlock:  apexSrc,
		SkipCert:   false,
	}); err != nil {
		t.Fatalf("init-box: %v", err)
	}
}

// TestInitBox_WritesApexSubstrate is the D1 init-box acceptance: against a temp
// SysRoot it writes the apex server block (with /_authn + the locations include),
// creates conf.d/locations/, writes the renew timer+service, and requests the
// privileged box ops through the seam (never executing them).
func TestInitBox_WritesApexSubstrate(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)

	apexSrc := readRepoFile(t, "../../../dashboard/etc/nginx.conf")
	runInitBox(t, o, apexSrc)

	l := NewLayoutSys(root, sysRoot, "dashboard")

	// conf.d/locations/ exists (the include dir per-app fragments drop into).
	if fi, err := os.Stat(l.LocationsDir()); err != nil || !fi.IsDir() {
		t.Fatalf("conf.d/locations/ not created: %v", err)
	}

	// The apex block byte-matches the committed nginx.conf with __DOMAIN__/__PORT__
	// substituted (the old dashboard/bin/setup output, modulo the split).
	wantApex := shellSubst(apexSrc, map[string]string{
		"__DOMAIN__": "int.ikigenba.com",
		"__PORT__":   "3000",
	})
	gotApex := readRepoFile(t, l.ApexBlockPath())
	if gotApex != wantApex {
		t.Fatalf("apex block mismatch:\n--- got ---\n%q\n--- want ---\n%q", gotApex, wantApex)
	}
	// The apex block carries the /_authn hook and the locations include — the two
	// dashboard-owned pieces init-box is responsible for.
	if !strings.Contains(gotApex, "location = /_authn") {
		t.Errorf("apex block missing the /_authn introspection hook")
	}
	if !strings.Contains(gotApex, "include /etc/nginx/conf.d/locations/*.conf;") {
		t.Errorf("apex block missing the conf.d/locations include")
	}

	// The renewal timer + service are written with the expected content.
	if got := readRepoFile(t, l.RenewTimerPath()); got != renewTimer {
		t.Fatalf("renew timer mismatch:\n--- got ---\n%q\n--- want ---\n%q", got, renewTimer)
	}
	if got := readRepoFile(t, l.RenewServicePath()); got != renewService {
		t.Fatalf("renew service mismatch:\n--- got ---\n%q", got)
	}

	// Privileged box ops were REQUESTED through the seam, in order — and not run.
	// Greenfield box (cert absent): the first cert is bootstrapped via certbot
	// --standalone BEFORE nginx -t, since the apex :443 block references that cert
	// and nginx cannot validate without it.
	wantOps := []string{
		"install-packages:nginx,certbot",
		"obtain-cert-standalone:int.ikigenba.com",
		"nginx-test",
		"enable-now:nginx",
		"nginx-reload",
		"obtain-cert:int.ikigenba.com",
		"daemon-reload",
		"enable-now:ikigenba-certbot-renew.timer",
	}
	if got := sys.opSeq(); strings.Join(got, "|") != strings.Join(wantOps, "|") {
		t.Fatalf("init-box ops = %v, want %v", got, wantOps)
	}
}

// TestInitBox_SkipCert exercises the cert short-circuit: the apex block + timer
// are still written, but nginx is NOT validated/started and certbot is NOT
// invoked. The apex block's 443 server references the apex cert by path, so on a
// greenfield box `nginx -t` cannot pass until that cert exists — --skip-cert
// must therefore stage the block without touching nginx, deferring nginx
// validate/start (and cert issuance) to when the cert lands.
func TestInitBox_SkipCert(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)
	apexSrc := readRepoFile(t, "../../../dashboard/etc/nginx.conf")

	if err := o.InitBox(context.Background(), InitBoxOptions{
		DefaultApp: "dashboard", Domain: "int.ikigenba.com", Port: 3000,
		ApexBlock: apexSrc, SkipCert: true,
	}); err != nil {
		t.Fatalf("init-box --skip-cert: %v", err)
	}

	// The apex block + locations dir are staged and the renew timer enabled, but
	// no nginx validate/start and no cert — those wait for the cert to exist.
	wantOps := []string{
		"install-packages:nginx,certbot",
		"daemon-reload",
		"enable-now:ikigenba-certbot-renew.timer",
	}
	if got := sys.opSeq(); strings.Join(got, "|") != strings.Join(wantOps, "|") {
		t.Fatalf("init-box --skip-cert ops = %v, want %v", got, wantOps)
	}

	// The apex block is still written so the cert step can validate it later.
	if got := readRepoFile(t, o.layout("dashboard").ApexBlockPath()); !strings.Contains(got, "server_name int.ikigenba.com;") {
		t.Errorf("--skip-cert did not stage the apex block")
	}
}

// TestInitBox_CertExists covers a rerun on a box that already has the apex cert:
// init-box must NOT re-bootstrap via standalone (which would need :80 free); it
// just (re)validates + reloads nginx, leaving renewals to the timer. Idempotency.
func TestInitBox_CertExists(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{certExists: true}
	o := newProvisioner(t, root, sysRoot, sys)
	apexSrc := readRepoFile(t, "../../../dashboard/etc/nginx.conf")

	if err := o.InitBox(context.Background(), InitBoxOptions{
		DefaultApp: "dashboard", Domain: "int.ikigenba.com", Port: 3000,
		Email: "ops@example.com", ApexBlock: apexSrc, SkipCert: false,
	}); err != nil {
		t.Fatalf("init-box (cert exists): %v", err)
	}

	// No standalone bootstrap; nginx validate/enable/reload, then a webroot
	// obtain-cert that reconciles the renewal config (no re-issue, cert is live).
	wantOps := []string{
		"install-packages:nginx,certbot",
		"nginx-test",
		"enable-now:nginx",
		"nginx-reload",
		"obtain-cert:int.ikigenba.com",
		"daemon-reload",
		"enable-now:ikigenba-certbot-renew.timer",
	}
	if got := sys.opSeq(); strings.Join(got, "|") != strings.Join(wantOps, "|") {
		t.Fatalf("init-box (cert exists) ops = %v, want %v", got, wantOps)
	}
}

// TestSetup_PathRoutedService is the D1 setup acceptance for a plain path-routed
// service (ledger): the systemd unit + nginx fragment byte-match today's
// ledger/bin/setup output, the /opt/<app> tree is created, and the privileged ops
// went through the seam.
func TestSetup_PathRoutedService(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)

	// init-box first (setup depends on conf.d/locations/).
	runInitBox(t, o, readRepoFile(t, "../../../dashboard/etc/nginx.conf"))
	// Reset the op recorder so we assert only setup's ops.
	sys.ops = nil

	app := "ledger"
	fragSrc := readRepoFile(t, "../../../ledger/etc/nginx.conf")
	if err := o.Setup(context.Background(), SetupOptions{
		App: app, Port: 3002, Fragment: fragSrc,
	}); err != nil {
		t.Fatalf("setup ledger: %v", err)
	}

	l := NewLayoutSys(root, sysRoot, app)

	// systemd unit byte-matches today's bin/setup heredoc, enabled-not-started.
	if got := readRepoFile(t, l.UnitPath()); got != expectedUnit(app) {
		t.Fatalf("ledger unit mismatch:\n--- got ---\n%q\n--- want ---\n%q", got, expectedUnit(app))
	}

	// nginx fragment byte-matches the committed etc/nginx.conf with __PORT__ → 3002.
	wantFrag := shellSubst(fragSrc, map[string]string{"__PORT__": "3002"})
	if got := readRepoFile(t, l.FragmentPath()); got != wantFrag {
		t.Fatalf("ledger fragment mismatch:\n--- got ---\n%q\n--- want ---\n%q", got, wantFrag)
	}

	// The /opt/<app> tree exists (PLAN §1.4): releases/ bin/ etc/ data/ backups/.
	for _, dir := range []string{
		l.AppDir(), l.ReleasesDir(), l.BinDir(), l.EtcDir(), l.DataDir(), l.BackupsDir(),
	} {
		if fi, err := os.Stat(dir); err != nil || !fi.IsDir() {
			t.Fatalf("tree dir %s not created: %v", dir, err)
		}
	}
	// data/ must be 0750 (the DB's parent), the rest 0755 — matching install -d.
	if fi, _ := os.Stat(l.DataDir()); fi != nil && fi.Mode().Perm() != 0o750 {
		t.Errorf("data dir perm = %o, want 0750", fi.Mode().Perm())
	}
	// The DB itself is never created by setup (PLAN §2.7 — state on first start).
	if _, err := os.Stat(l.DBPath()); !os.IsNotExist(err) {
		t.Errorf("setup created the DB, want it untouched (err=%v)", err)
	}

	// Privileged ops requested through the seam, in order, enabled-NOT-started.
	// setup installs no packages (the launcher deps awscli-2/jq are present from
	// instance bootstrap, nginx/certbot are init-box's job).
	wantOps := []string{
		"ensure-user:ledger:" + l.AppDir(),
		"daemon-reload",
		"enable:ledger.service", // enable (not enable-now) — NOT started
		"nginx-test",
		"nginx-reload",
	}
	if got := sys.opSeq(); strings.Join(got, "|") != strings.Join(wantOps, "|") {
		t.Fatalf("setup ops = %v, want %v", got, wantOps)
	}
	// The unit was enabled but never started (no restart/start op recorded).
	if sys.restarts != 0 {
		t.Errorf("setup started the unit (%d restarts), want enabled-not-started", sys.restarts)
	}
}

// TestSetup_WWWTree provisions the sites service: in addition to the standard
// tree, setup must create the SEPARATE world-readable www/ tree (working/,
// served/, served/public/, served/private/, and the www/ root) at mode 0755 and
// `chown -R sites:sites` the www root, so nginx (www-data) can traverse+read it
// (the stock data/ is 0750 and untraversable). The WWWDirs are derived per-app
// via WWWDirsFor, so `opsctl setup sites` provisions them with no operator flag.
func TestSetup_WWWTree(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)
	runInitBox(t, o, readRepoFile(t, "../../../dashboard/etc/nginx.conf"))
	sys.ops = nil

	app := "sites"
	fragSrc := readRepoFile(t, "../../../ledger/etc/nginx.conf") // any path-routed fragment
	if err := o.Setup(context.Background(), SetupOptions{
		App: app, Port: 3010, Fragment: fragSrc,
		WWWDirs: WWWDirsFor(root, app),
	}); err != nil {
		t.Fatalf("setup sites: %v", err)
	}

	l := NewLayoutSys(root, sysRoot, app)

	// The four served/working dirs plus the www/ root all exist at exactly 0755 —
	// world-traversable so www-data can reach the served trees.
	for _, dir := range []string{
		l.WWWRoot(), l.WWWWorkingDir(), l.WWWServedDir(), l.WWWPublicDir(), l.WWWPrivateDir(),
	} {
		fi, err := os.Stat(dir)
		if err != nil || !fi.IsDir() {
			t.Fatalf("www dir %s not created: %v", dir, err)
		}
		if fi.Mode().Perm() != 0o755 {
			t.Errorf("www dir %s perm = %o, want 0755", dir, fi.Mode().Perm())
		}
	}

	// The www ROOT was handed to the sites user via a recursive chown through the
	// seam (so the whole subtree ends up sites:sites but 0755-traversable).
	wantOps := []string{
		"ensure-user:sites:" + l.AppDir(),
		"chown:sites:sites:" + l.WWWRoot(),
		"daemon-reload",
		"enable:sites.service",
		"nginx-test",
		"nginx-reload",
	}
	if got := sys.opSeq(); strings.Join(got, "|") != strings.Join(wantOps, "|") {
		t.Fatalf("setup sites ops = %v, want %v", got, wantOps)
	}
}

// TestWWWDirsFor_OnlySites asserts the www tree is derived per-app: sites gets
// the five-dir tree, every other app gets none — so non-sites setup creates no
// www dir (no regression). Pairs with the ledger setup test, which never sees a
// www dir or a chown op.
func TestWWWDirsFor_OnlySites(t *testing.T) {
	if got := WWWDirsFor("/opt", "ledger"); got != nil {
		t.Errorf("WWWDirsFor(ledger) = %v, want nil (non-sites apps get no www tree)", got)
	}
	got := WWWDirsFor("/opt", "sites")
	want := []string{
		"/opt/sites/www",
		"/opt/sites/www/working",
		"/opt/sites/www/served",
		"/opt/sites/www/served/public",
		"/opt/sites/www/served/private",
	}
	if strings.Join(got, "|") != strings.Join(want, "|") {
		t.Fatalf("WWWDirsFor(sites) = %v, want %v", got, want)
	}
}

// TestSetup_NoWWWTreeForOtherApps proves the non-sites path is unchanged: with no
// WWWDirs, setup creates no www dir and requests no chown. (The existing ledger
// setup test asserts the full op sequence has no chown; this guards the dir side.)
func TestSetup_NoWWWTreeForOtherApps(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)
	runInitBox(t, o, readRepoFile(t, "../../../dashboard/etc/nginx.conf"))
	sys.ops = nil

	app := "ledger"
	if err := o.Setup(context.Background(), SetupOptions{
		App: app, Port: 3002,
		Fragment: readRepoFile(t, "../../../ledger/etc/nginx.conf"),
		WWWDirs:  WWWDirsFor(root, app), // nil for ledger
	}); err != nil {
		t.Fatalf("setup ledger: %v", err)
	}
	l := NewLayoutSys(root, sysRoot, app)
	if _, err := os.Stat(l.WWWRoot()); !os.IsNotExist(err) {
		t.Errorf("setup created a www tree for ledger, want none (err=%v)", err)
	}
	for _, op := range sys.opSeq() {
		if strings.HasPrefix(op, "chown:") {
			t.Errorf("setup chowned for a non-sites app: %s", op)
		}
	}
}

// TestSetup_DeferNginx stages the fragment but skips nginx -t/reload — the
// greenfield path where nginx is not yet serviceable (no apex cert). The
// fragment is still written; only the validate/reload ops are absent.
func TestSetup_DeferNginx(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)
	runInitBox(t, o, readRepoFile(t, "../../../dashboard/etc/nginx.conf"))
	sys.ops = nil

	app := "ledger"
	fragSrc := readRepoFile(t, "../../../ledger/etc/nginx.conf")
	if err := o.Setup(context.Background(), SetupOptions{
		App: app, Port: 3002, Fragment: fragSrc, DeferNginx: true,
	}); err != nil {
		t.Fatalf("setup ledger --defer-nginx: %v", err)
	}

	l := NewLayoutSys(root, sysRoot, app)
	// The fragment is still staged.
	wantFrag := shellSubst(fragSrc, map[string]string{"__PORT__": "3002"})
	if got := readRepoFile(t, l.FragmentPath()); got != wantFrag {
		t.Fatalf("defer-nginx did not stage the fragment")
	}
	// But nginx -t/reload were NOT requested.
	wantOps := []string{
		"ensure-user:ledger:" + l.AppDir(),
		"daemon-reload",
		"enable:ledger.service",
	}
	if got := sys.opSeq(); strings.Join(got, "|") != strings.Join(wantOps, "|") {
		t.Fatalf("defer-nginx ops = %v, want %v", got, wantOps)
	}
}

// TestSetup_Apex provisions the apex app (dashboard) — DEFAULT, MOUNT=/, no
// location fragment of its own (the apex block is init-box's job). The unit still
// byte-matches; no fragment is dropped.
func TestSetup_Apex(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir()
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)
	runInitBox(t, o, readRepoFile(t, "../../../dashboard/etc/nginx.conf"))
	sys.ops = nil

	app := "dashboard"
	// The apex app has no /srv/<app>/ fragment of its own — its server block is
	// owned by init-box — so setup is called with an empty Fragment.
	if err := o.Setup(context.Background(), SetupOptions{App: app, Port: 3000, Fragment: ""}); err != nil {
		t.Fatalf("setup dashboard: %v", err)
	}

	l := NewLayoutSys(root, sysRoot, app)
	if got := readRepoFile(t, l.UnitPath()); got != expectedUnit(app) {
		t.Fatalf("dashboard unit mismatch:\n--- got ---\n%q\n--- want ---\n%q", got, expectedUnit(app))
	}
	// No location fragment is written for the apex app.
	if _, err := os.Stat(l.FragmentPath()); !os.IsNotExist(err) {
		t.Errorf("apex setup wrote a location fragment, want none (err=%v)", err)
	}
	// Tree created; ops requested through the seam (no fragment ⇒ no nginx ops).
	if fi, err := os.Stat(l.AppDir()); err != nil || !fi.IsDir() {
		t.Fatalf("/opt/dashboard not created: %v", err)
	}
	wantOps := []string{
		"ensure-user:dashboard:" + l.AppDir(),
		"daemon-reload",
		"enable:dashboard.service",
	}
	if got := sys.opSeq(); strings.Join(got, "|") != strings.Join(wantOps, "|") {
		t.Fatalf("apex setup ops = %v, want %v", got, wantOps)
	}
}

// TestSetup_RequiresInitBox asserts setup refuses when conf.d/locations/ is
// absent (the box-global precondition init-box owns) — proving the split: per-app
// provisioning never reaches for box-global state, it depends on it.
func TestSetup_RequiresInitBox(t *testing.T) {
	root := t.TempDir()
	sysRoot := t.TempDir() // no init-box run ⇒ no conf.d/locations/
	sys := &stubSystem{}
	o := newProvisioner(t, root, sysRoot, sys)

	err := o.Setup(context.Background(), SetupOptions{
		App: "ledger", Port: 3002,
		Fragment: readRepoFile(t, "../../../ledger/etc/nginx.conf"),
	})
	if err == nil || !strings.Contains(err.Error(), "init-box") {
		t.Fatalf("setup without init-box err = %v, want an init-box precondition error", err)
	}
	if len(sys.opSeq()) != 0 {
		t.Errorf("setup ran box ops before the precondition check: %v", sys.opSeq())
	}
}

// fragmentPathOnBox documents the on-box fragment path matches the old
// bin/setup target /etc/nginx/conf.d/locations/<app>.conf.
func TestFragmentPath_MatchesBoxConvention(t *testing.T) {
	l := NewLayout("/opt", "ledger") // SysRoot "" ⇒ "/"
	if got, want := l.FragmentPath(), "/etc/nginx/conf.d/locations/ledger.conf"; got != want {
		t.Fatalf("FragmentPath = %q, want %q", got, want)
	}
	if got, want := l.UnitPath(), "/etc/systemd/system/ledger.service"; got != want {
		t.Fatalf("UnitPath = %q, want %q", got, want)
	}
	if got, want := filepath.Clean(l.ApexBlockPath()), "/etc/nginx/conf.d/ledger.conf"; got != want {
		t.Fatalf("ApexBlockPath = %q, want %q", got, want)
	}
}
