// Command wiki-eval-gen writes the per-site evaluation test-set bundles (plan P15,
// design docs/wiki-evaluation-design.md). It is the authoring half of Part II: the
// generators in wiki/internal/eval/gen produce gen-1 (dataset + prompt) bundles in
// the eval-design q3 storage layout under testsets/<site>/. The committed bundles
// are the deliverable; this command regenerates them deterministically (re-running
// overwrites with byte-identical content), so the committed test sets are
// reproducible from the generators.
//
// It makes NO provider calls — the goldens are synthetic-authored and verified
// offline by the adversarial pass in gen_test.go (eval design q1: synthetic-first,
// real data only validates). Usage:
//
//	wiki-eval-gen [-out testsets]
package main

import (
	"flag"
	"fmt"
	"os"

	"wiki/internal/eval/gen"
)

func main() {
	if err := run(os.Args[1:]); err != nil {
		fmt.Fprintln(os.Stderr, "wiki-eval-gen:", err)
		os.Exit(1)
	}
}

func run(args []string) error {
	fs := flag.NewFlagSet("wiki-eval-gen", flag.ContinueOnError)
	out := fs.String("out", "testsets", "the test-set bundle tree root to write under")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if err := gen.WriteAll(*out); err != nil {
		return err
	}
	for _, site := range gen.SiteNames() {
		fmt.Printf("wrote %s/%s/{datasets/gen-1.json, prompts/v1.txt, bundles/gen-1.json}\n", *out, site)
	}
	return nil
}
