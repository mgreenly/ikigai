// Command opsctl is the ikigai on-box platform CLI (PLAN §1.3). It implements the
// deploy-critical verbs stage / deploy / rollback / prune over the versioned
// release-dir + atomic-symlink layout (PLAN §1.4), the operator read/control verbs
// status / releases / tail and the start/stop/restart/enable/disable systemctl
// passthroughs, plus the box-provisioning verbs init-box (box-global substrate)
// and setup (per-app provisioning) (PLAN §D1). It runs on the box only (operators
// SSH in and run `sudo opsctl …`); all filesystem ops are rooted at OPSCTL_ROOT
// (default /opt) — and the system-config tree at OPSCTL_SYSROOT (default /) — so
// the core is fully testable against temp dirs.
package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"strings"

	"opsctl/internal/opsctl"
)

// reorderArgs moves flag tokens ahead of positional tokens so the standard
// flag package — which stops scanning at the first non-flag token — accepts
// flags written AFTER positionals (e.g. `opsctl stage ledger v0.1.0
// --artifact X`, the form bin/ship emits, and `opsctl setup ledger --port N`).
// A bare `--` terminates flag scanning: everything after it is positional and is
// passed through verbatim. A flag that takes a separate value is detected by the
// known set of value-taking flags so the value token is not mistaken for a
// positional.
func reorderArgs(args []string, valueFlags map[string]bool) []string {
	var flags, pos []string
	for i := 0; i < len(args); i++ {
		a := args[i]
		if a == "--" {
			pos = append(pos, args[i+1:]...)
			break
		}
		if strings.HasPrefix(a, "-") && a != "-" {
			flags = append(flags, a)
			// If this flag takes a separate value (`--artifact X`, not `--artifact=X`)
			// pull the next token along as its value.
			name := strings.TrimLeft(a, "-")
			if !strings.Contains(a, "=") && valueFlags[name] && i+1 < len(args) {
				i++
				flags = append(flags, args[i])
			}
			continue
		}
		pos = append(pos, a)
	}
	return append(flags, pos...)
}

// verb is one documented command: its name and the one-line synopsis shown in
// the grouped --help and in `opsctl <verb> --help`. The verb's RUNNER lives in
// the separate `runners` map (keyed by the same name) — kept apart so the
// `groups` var initializer never references the runner funcs, which would form an
// init cycle (groups → runStage → newFlagSet → groups). The help-coverage test
// asserts the two stay in lockstep: every runner is documented, every documented
// verb has a runner.
type verb struct {
	name     string
	synopsis string // one-line `opsctl <verb> …` form, shown in --help and grouped usage
}

// group bundles verbs under a --help heading.
type group struct {
	title string
	verbs []verb
}

// groups is the grouped verb registry — the documentation source of truth for
// the grouped --help. Pure data (no funcs) so it stays init-cycle-free.
var groups = []group{
	{"Deploy lifecycle", []verb{
		{"stage", "opsctl stage <app> <version> --artifact <path> [--force]   place a release (preflight + collision guard); NOT live; deletes the /tmp artifact"},
		{"deploy", "opsctl deploy <app> <version>                              activate a staged release (atomic swap)"},
		{"rollback", "opsctl rollback <app> [version]                           repoint current to a prior release"},
		{"prune", "opsctl prune <app> [--keep N]                            bound on-box release history"},
	}},
	{"Inspect", []verb{
		{"status", "opsctl status [app]                                       report app · version · sha · active (all installed apps if omitted)"},
		{"releases", "opsctl releases <app>                                     list releases, marking current + predecessor"},
	}},
	{"Service control", []verb{
		{"start", "opsctl start <app> [systemctl args…]                      systemctl start (extra args forwarded verbatim)"},
		{"stop", "opsctl stop <app> [systemctl args…]                       systemctl stop (extra args forwarded verbatim)"},
		{"restart", "opsctl restart <app> [systemctl args…]                    systemctl restart (extra args forwarded verbatim)"},
		{"enable", "opsctl enable <app> [systemctl args…]                     systemctl enable (extra args forwarded verbatim)"},
		{"disable", "opsctl disable <app> [systemctl args…]                    systemctl disable (extra args forwarded verbatim)"},
		{"tail", "opsctl tail <app> [journalctl args…]                      stream the app's journal (journalctl -u <app> -f; extra args forwarded)"},
	}},
	{"Provisioning", []verb{
		{"setup", "opsctl setup <app> [--port <n>] [--fragment <path>]       per-app provisioning (user, /opt/<app> tree, systemd unit enabled-not-started, nginx fragment)"},
		{"init-box", "opsctl init-box --default-app <app> --domain <d> --port <n> --apex-block <path> [--email <e>] [--skip-cert]\n                                                            box-global substrate (apex block, /_authn, conf.d/locations/, cert, renew timer)"},
	}},
}

// runner is a verb's dispatch handler.
type runner func(ctx context.Context, root, name string, args []string) error

// runners is the dispatch table — keyed by the same names as `groups`. The
// help-coverage test asserts these two maps share an identical key set, so a verb
// can't be dispatchable but undocumented (or vice versa).
var runners = map[string]runner{
	"stage":    runStage,
	"deploy":   runDeploy,
	"rollback": runRollback,
	"prune":    runPrune,
	"status":   runStatus,
	"releases": runReleases,
	"start":    runServiceControl,
	"stop":     runServiceControl,
	"restart":  runServiceControl,
	"enable":   runServiceControl,
	"disable":  runServiceControl,
	"tail":     runTail,
	"setup":    runSetup,
	"init-box": runInitBox,
}

// synopsisOf returns a verb's one-line synopsis from the doc registry.
func synopsisOf(name string) (string, bool) {
	for _, g := range groups {
		for _, v := range g.verbs {
			if v.name == name {
				return v.synopsis, true
			}
		}
	}
	return "", false
}

// usage renders the grouped top-level help from the verb registry plus the env
// section. Building it from `groups` keeps help and dispatch from drifting.
func usage() string {
	var b strings.Builder
	b.WriteString("opsctl — ikigai on-box platform CLI\n\nusage:\n")
	for _, g := range groups {
		fmt.Fprintf(&b, "\n  %s\n", g.title)
		for _, v := range g.verbs {
			fmt.Fprintf(&b, "    %s\n", v.synopsis)
		}
	}
	b.WriteString(`
env:
  OPSCTL_ROOT     install base (default /opt) — the /opt/<app> tree
  OPSCTL_SYSROOT  system-config base (default /) — the /etc + /var tree

Run 'opsctl <verb> --help' for a verb's flags.
`)
	return b.String()
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprint(os.Stderr, usage())
		os.Exit(2)
	}
	root := os.Getenv("OPSCTL_ROOT")
	name := os.Args[1]
	args := os.Args[2:]
	ctx := context.Background()

	switch name {
	case "-h", "--help", "help":
		fmt.Fprint(os.Stdout, usage())
		return
	}

	run, ok := runners[name]
	if !ok {
		fmt.Fprintf(os.Stderr, "opsctl: unknown verb %q\n\n%s", name, usage())
		os.Exit(2)
	}
	if err := run(ctx, root, name, args); err != nil {
		fmt.Fprintf(os.Stderr, "opsctl: %v\n", err)
		os.Exit(1)
	}
}

// newFlagSet returns a FlagSet whose Usage prints the verb's one-line synopsis,
// then its flags — so `opsctl <verb> --help` (which flag.ContinueOnError surfaces
// as flag.ErrHelp) reads well. The synopsis is looked up from the registry.
func newFlagSet(name string) *flag.FlagSet {
	fs := flag.NewFlagSet(name, flag.ContinueOnError)
	fs.Usage = func() {
		out := fs.Output()
		if s, ok := synopsisOf(name); ok {
			fmt.Fprintf(out, "%s\n", s)
		} else {
			fmt.Fprintf(out, "usage: opsctl %s\n", name)
		}
		// Only print the flags block if the set has any defined.
		hasFlags := false
		fs.VisitAll(func(*flag.Flag) { hasFlags = true })
		if hasFlags {
			fmt.Fprintf(out, "\nflags:\n")
			fs.PrintDefaults()
		}
	}
	return fs
}

func runStage(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	artifact := fs.String("artifact", "", "path to the static linux/amd64 artifact (required)")
	force := fs.Bool("force", false, "replace an already-staged release at a different commit")
	if err := fs.Parse(reorderArgs(args, map[string]bool{"artifact": true})); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) != 2 {
		return fmt.Errorf("usage: opsctl stage <app> <version> --artifact <path> [--force]")
	}
	if *artifact == "" {
		return fmt.Errorf("stage: --artifact is required")
	}
	return opsctl.New(root).Stage(ctx, pos[0], pos[1], *artifact, *force)
}

func runDeploy(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	if err := fs.Parse(args); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) != 2 {
		return fmt.Errorf("usage: opsctl deploy <app> <version>")
	}
	return opsctl.New(root).Deploy(ctx, pos[0], pos[1])
}

func runRollback(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	if err := fs.Parse(args); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) < 1 || len(pos) > 2 {
		return fmt.Errorf("usage: opsctl rollback <app> [version]")
	}
	target := ""
	if len(pos) == 2 {
		target = pos[1]
	}
	return opsctl.New(root).Rollback(ctx, pos[0], target)
}

func runPrune(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	keep := fs.Int("keep", opsctl.DefaultKeep, "number of recent releases to retain")
	if err := fs.Parse(reorderArgs(args, map[string]bool{"keep": true})); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) != 1 {
		return fmt.Errorf("usage: opsctl prune <app> [--keep N]")
	}
	o := opsctl.New(root)
	o.Keep = *keep
	return o.Prune(ctx, pos[0])
}

func runStatus(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	if err := fs.Parse(args); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) > 1 {
		return fmt.Errorf("usage: opsctl status [app]")
	}
	app := ""
	if len(pos) == 1 {
		app = pos[0]
	}
	return opsctl.New(root).Status(ctx, app)
}

func runReleases(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	if err := fs.Parse(args); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) != 1 {
		return fmt.Errorf("usage: opsctl releases <app>")
	}
	return opsctl.New(root).Releases(ctx, pos[0])
}

// runTail and runServiceControl are PASSTHROUGH verbs: they BYPASS reorderArgs.
// The app is args[0]; everything after it (args[1:]) is forwarded VERBATIM to
// journalctl/systemctl, so `opsctl tail crm -n 100 --since 1h` reaches journalctl
// unscrambled. Structured verbs (stage/setup/prune/init-box) keep reorderArgs.
// A bare `--help`/`-h` (no app yet) still prints the verb synopsis.
func runTail(ctx context.Context, root, name string, args []string) error {
	if isHelp(args) {
		newFlagSet(name).Usage()
		return nil
	}
	if len(args) < 1 {
		return fmt.Errorf("usage: opsctl tail <app> [journalctl args…]")
	}
	return opsctl.New(root).Tail(ctx, args[0], args[1:])
}

func runServiceControl(ctx context.Context, root, name string, args []string) error {
	if isHelp(args) {
		newFlagSet(name).Usage()
		return nil
	}
	if len(args) < 1 {
		return fmt.Errorf("usage: opsctl %s <app> [systemctl args…]", name)
	}
	o := opsctl.New(root)
	app := args[0]
	extra := args[1:]
	switch name {
	case "start":
		return o.Start(ctx, app, extra)
	case "stop":
		return o.Stop(ctx, app, extra)
	case "restart":
		return o.Restartd(ctx, app, extra)
	case "enable":
		return o.Enable(ctx, app, extra)
	case "disable":
		return o.Disable(ctx, app, extra)
	}
	return fmt.Errorf("opsctl: unknown service-control verb %q", name)
}

func runInitBox(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	defaultApp := fs.String("default-app", "dashboard", "the apex/DEFAULT app name")
	domain := fs.String("domain", "", "apex domain, e.g. ai.metaspot.org (required)")
	port := fs.Int("port", 3000, "the apex app's loopback port")
	email := fs.String("email", "", "certbot email for HTTP-01 cert issuance")
	apexBlock := fs.String("apex-block", "", "path to the apex nginx server block source (required)")
	skipCert := fs.Bool("skip-cert", false, "do not issue a TLS cert (stage the block only)")
	if err := fs.Parse(reorderArgs(args, map[string]bool{
		"default-app": true, "domain": true, "port": true, "email": true, "apex-block": true,
	})); err != nil {
		return helpErr(err)
	}
	if *domain == "" {
		return fmt.Errorf("init-box: --domain is required")
	}
	block, err := opsctl.LoadApexBlockFile(*apexBlock)
	if err != nil {
		return err
	}
	return opsctl.New(root).InitBox(ctx, opsctl.InitBoxOptions{
		DefaultApp: *defaultApp,
		Domain:     *domain,
		Port:       *port,
		Email:      *email,
		ApexBlock:  block,
		SkipCert:   *skipCert,
	})
}

func runSetup(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	port := fs.Int("port", 0, "the service's loopback port (substituted for __PORT__ in the fragment)")
	fragment := fs.String("fragment", "", "path to the service's nginx location fragment source (omit for a worker)")
	if err := fs.Parse(reorderArgs(args, map[string]bool{"port": true, "fragment": true})); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) != 1 {
		return fmt.Errorf("usage: opsctl setup <app> [--port N] [--fragment <path>]")
	}
	frag, err := opsctl.LoadFragmentFile(*fragment)
	if err != nil {
		return err
	}
	return opsctl.New(root).Setup(ctx, opsctl.SetupOptions{
		App:      pos[0],
		Port:     *port,
		Fragment: frag,
	})
}

// helpErr swallows flag.ErrHelp (the FlagSet already printed its synopsis+flags
// via the Usage hook) so `opsctl <verb> --help` exits 0, not as an error.
func helpErr(err error) error {
	if err == flag.ErrHelp {
		return nil
	}
	return err
}

// isHelp reports whether the first token is a help flag — used by passthrough
// verbs that don't run their args through a FlagSet.
func isHelp(args []string) bool {
	return len(args) > 0 && (args[0] == "-h" || args[0] == "--help")
}
