// Command opsctl is the suite on-box platform CLI (PLAN §1.3). It implements the
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

// verb is one documented command. `synopsis` is the bare `opsctl <verb> …` usage
// form printed both in the grouped --help and by `opsctl <verb> --help`; the verb
// names are self-explanatory, so no separate one-line description is carried. The
// verb's RUNNER lives in the separate `runners` map (keyed by the same name) —
// kept apart so the `groups` var initializer never references the runner funcs,
// which would form an init cycle (groups → runStage → newFlagSet → groups). The
// help-coverage test asserts the two stay in lockstep: every runner is
// documented, every documented verb has a runner.
type verb struct {
	name     string
	synopsis string // bare `opsctl <verb> …` usage form, shown in grouped + per-verb --help
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
		{"stage", "opsctl stage <app> <version> --artifact <path> [--force]"},
		{"deploy", "opsctl deploy <app> <version>"},
		{"rollback", "opsctl rollback <app> [version]"},
		{"prune", "opsctl prune <app> [--keep <n>]"},
		{"backup", "opsctl backup <app>|--all"},
		{"restore", "opsctl restore <app> [snapshot-key]"},
	}},
	{"Inspect", []verb{
		{"status", "opsctl status [app]"},
		{"releases", "opsctl releases <app>"},
	}},
	{"Service control", []verb{
		{"start", "opsctl start <app> [systemctl args…]"},
		{"stop", "opsctl stop <app> [systemctl args…]"},
		{"restart", "opsctl restart <app> [systemctl args…]"},
		{"enable", "opsctl enable <app> [systemctl args…]"},
		{"disable", "opsctl disable <app> [systemctl args…]"},
		{"tail", "opsctl tail <app> [journalctl args…]"},
	}},
	{"Provisioning", []verb{
		{"setup", "opsctl setup <app> [--port <n>] [--fragment <path>] [--defer-nginx] [--packages <p1,p2>]"},
		{"teardown", "opsctl teardown <app> --force [--keep-user]"},
		// One flag per line; line 1 is the verb + first flag, continuation lines
		// align under --domain (16 cols = len("opsctl init-box "); the renderer
		// prepends the 4-col group indent → column 20).
		{"init-box", "opsctl init-box --domain <d>\n" +
			"                --apex-block <path>\n" +
			"                [--default-app <app>]\n" +
			"                [--port <n>]\n" +
			"                [--email <e>]\n" +
			"                [--skip-cert]"},
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
	"backup":   runBackup,
	"restore":  runRestore,
	"status":   runStatus,
	"releases": runReleases,
	"start":    runServiceControl,
	"stop":     runServiceControl,
	"restart":  runServiceControl,
	"enable":   runServiceControl,
	"disable":  runServiceControl,
	"tail":     runTail,
	"setup":    runSetup,
	"teardown": runTeardown,
	"init-box": runInitBox,
}

// synopsisOf returns a verb's usage form from the doc registry.
func synopsisOf(name string) (form string, ok bool) {
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
//
// Layout: the verb names are self-explanatory, so each verb renders as just
// "<indent><form>" (no trailing description, no trailing whitespace). A form may
// span multiple physical lines (a long flag list wrapped onto continuation lines,
// each carrying its own leading alignment), so every line is indented and width is
// checked per physical line — every rendered line stays ≤100 columns (asserted by
// the usage-width test).
const helpIndent = 4 // leading spaces before each verb form

func usage() string {
	var b strings.Builder
	b.WriteString("opsctl — suite on-box platform CLI\n\nusage:\n")
	indent := strings.Repeat(" ", helpIndent)
	for _, g := range groups {
		fmt.Fprintf(&b, "\n  %s\n", g.title)
		for _, v := range g.verbs {
			// Just the form. A form may span multiple lines (long flag lists wrapped
			// onto continuation lines); indent each so wrapped args align under the verb.
			for _, line := range strings.Split(v.synopsis, "\n") {
				fmt.Fprintf(&b, "%s%s\n", indent, line)
			}
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
		if form, ok := synopsisOf(name); ok {
			fmt.Fprintf(out, "%s\n", form)
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

func runBackup(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	all := fs.Bool("all", false, "backup every deployed app and the apex cert")
	if err := fs.Parse(args); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if *all {
		if len(pos) != 0 {
			return fmt.Errorf("usage: opsctl backup <app>|--all")
		}
		return opsctl.New(root).BackupAll(ctx)
	}
	if len(pos) != 1 || strings.HasPrefix(pos[0], "-") {
		return fmt.Errorf("usage: opsctl backup <app>|--all")
	}
	return opsctl.New(root).Backup(ctx, pos[0])
}

func runRestore(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	if err := fs.Parse(args); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) < 1 || len(pos) > 2 {
		return fmt.Errorf("usage: opsctl restore <app> [snapshot-key]")
	}
	for _, p := range pos {
		if strings.HasPrefix(p, "-") {
			return fmt.Errorf("restore: unsupported flag %s; interactive confirmation is required and there is no --yes bypass", p)
		}
	}
	key := ""
	if len(pos) == 2 {
		key = pos[1]
	}
	return opsctl.New(root).Restore(ctx, pos[0], key, os.Stdin)
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
	domain := fs.String("domain", "", "apex domain, e.g. int.ikigenba.com (required)")
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
	deferNginx := fs.Bool("defer-nginx", false, "stage the fragment but skip nginx -t/reload (greenfield box with no apex cert yet)")
	packages := fs.String("packages", "", "comma-separated OS packages to install for the service (e.g. python3.11)")
	if err := fs.Parse(reorderArgs(args, map[string]bool{"port": true, "fragment": true, "packages": true})); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) != 1 {
		return fmt.Errorf("usage: opsctl setup <app> [--port N] [--fragment <path>] [--defer-nginx] [--packages p1,p2]")
	}
	frag, err := opsctl.LoadFragmentFile(*fragment)
	if err != nil {
		return err
	}
	return opsctl.New(root).Setup(ctx, opsctl.SetupOptions{
		App:        pos[0],
		Port:       *port,
		Fragment:   frag,
		DeferNginx: *deferNginx,
		// Derived per-app: sites gets its world-readable www/ tree automatically
		// (no operator flag); every other app gets none.
		WWWDirs:  opsctl.WWWDirsFor(root, pos[0]),
		Packages: splitList(*packages),
	})
}

func runTeardown(ctx context.Context, root, name string, args []string) error {
	fs := newFlagSet(name)
	force := fs.Bool("force", false, "confirm the destructive removal (required)")
	keepUser := fs.Bool("keep-user", false, "retain the dedicated --system app user")
	if err := fs.Parse(reorderArgs(args, nil)); err != nil {
		return helpErr(err)
	}
	pos := fs.Args()
	if len(pos) != 1 {
		return fmt.Errorf("usage: opsctl teardown <app> --force [--keep-user]")
	}
	return opsctl.New(root).Teardown(ctx, opsctl.TeardownOptions{
		App:      pos[0],
		Force:    *force,
		KeepUser: *keepUser,
	})
}

// splitList parses a comma-separated flag value into a trimmed, empty-dropped
// slice. "" yields nil so an absent --packages means "install nothing".
func splitList(s string) []string {
	if strings.TrimSpace(s) == "" {
		return nil
	}
	var out []string
	for _, p := range strings.Split(s, ",") {
		if p = strings.TrimSpace(p); p != "" {
			out = append(out, p)
		}
	}
	return out
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
