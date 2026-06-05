// Command optctl is the ikigai on-box platform CLI (PLAN §1.3). This C2 skeleton
// implements the deploy-critical verbs install / rollback / prune over the
// versioned release-dir + atomic-symlink layout (PLAN §1.4). It runs on the box
// only (operators SSH in and run `sudo optctl …`); all filesystem ops are rooted
// at OPTCTL_ROOT (default /opt) so the core is fully testable against a temp dir.
//
// The remaining verbs (backup / restore / setup / init-box) land in later plan
// steps (D1 adds init-box / setup); this binary parses them as not-yet-implemented
// so the CLI surface is discoverable.
package main

import (
	"context"
	"flag"
	"fmt"
	"os"

	"optctl/internal/optctl"
)

const usage = `optctl — ikigai on-box platform CLI

usage:
  optctl install <app> <version> --artifact <path>   ship a version live (atomic swap)
  optctl rollback <app> [version]                     repoint current to a prior release
  optctl prune <app> [--keep N]                       bound on-box release history

env:
  OPTCTL_ROOT   install base (default /opt) — every path is rooted here
`

func main() {
	if len(os.Args) < 2 {
		fmt.Fprint(os.Stderr, usage)
		os.Exit(2)
	}
	root := os.Getenv("OPTCTL_ROOT")
	verb := os.Args[1]
	args := os.Args[2:]
	ctx := context.Background()

	var err error
	switch verb {
	case "install":
		err = cmdInstall(ctx, root, args)
	case "rollback":
		err = cmdRollback(ctx, root, args)
	case "prune":
		err = cmdPrune(ctx, root, args)
	case "-h", "--help", "help":
		fmt.Fprint(os.Stdout, usage)
		return
	default:
		fmt.Fprintf(os.Stderr, "optctl: unknown verb %q\n\n%s", verb, usage)
		os.Exit(2)
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "optctl: %v\n", err)
		os.Exit(1)
	}
}

func cmdInstall(ctx context.Context, root string, args []string) error {
	fs := flag.NewFlagSet("install", flag.ContinueOnError)
	artifact := fs.String("artifact", "", "path to the static linux/amd64 artifact (required)")
	if err := fs.Parse(args); err != nil {
		return err
	}
	pos := fs.Args()
	if len(pos) != 2 {
		return fmt.Errorf("usage: optctl install <app> <version> --artifact <path>")
	}
	if *artifact == "" {
		return fmt.Errorf("install: --artifact is required")
	}
	return optctl.New(root).Install(ctx, pos[0], pos[1], *artifact)
}

func cmdRollback(ctx context.Context, root string, args []string) error {
	fs := flag.NewFlagSet("rollback", flag.ContinueOnError)
	if err := fs.Parse(args); err != nil {
		return err
	}
	pos := fs.Args()
	if len(pos) < 1 || len(pos) > 2 {
		return fmt.Errorf("usage: optctl rollback <app> [version]")
	}
	target := ""
	if len(pos) == 2 {
		target = pos[1]
	}
	return optctl.New(root).Rollback(ctx, pos[0], target)
}

func cmdPrune(ctx context.Context, root string, args []string) error {
	fs := flag.NewFlagSet("prune", flag.ContinueOnError)
	keep := fs.Int("keep", optctl.DefaultKeep, "number of recent releases to retain")
	if err := fs.Parse(args); err != nil {
		return err
	}
	pos := fs.Args()
	if len(pos) != 1 {
		return fmt.Errorf("usage: optctl prune <app> [--keep N]")
	}
	o := optctl.New(root)
	o.Keep = *keep
	return o.Prune(ctx, pos[0])
}
