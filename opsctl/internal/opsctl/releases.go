package opsctl

import (
	"context"
	"fmt"
	"os"
)

// Releases lists an app's on-box release history — releases/<version>/ ascending
// by semantic version — marking which one is live (`current` points at it) and
// which is its immediate predecessor (the default rollback target). It is
// read-only.
//
//	v0.1.0
//	v0.1.1  (predecessor)
//	v0.1.2  (current)
func (o *Opsctl) Releases(ctx context.Context, app string) error {
	if app == "" {
		return fmt.Errorf("releases: app is required")
	}
	l := o.layout(app)

	rels, err := o.listReleases(l)
	if err != nil {
		return fmt.Errorf("releases: list releases: %w", err)
	}

	current, err := o.currentVersion(l)
	if err != nil {
		return fmt.Errorf("releases: read current: %w", err)
	}

	predecessor := ""
	if current != "" {
		// priorRelease errors when there is no prior (the only release); that is not
		// fatal here — just leaves the predecessor unmarked.
		if prior, perr := o.priorRelease(l, current); perr == nil {
			predecessor = prior
		}
	}

	w := o.Out
	if w == nil {
		w = os.Stdout
	}
	if len(rels) == 0 {
		fmt.Fprintf(w, "%s: no releases\n", app)
		return nil
	}
	for _, v := range rels {
		mark := ""
		switch v {
		case current:
			mark = "  (current)"
		case predecessor:
			mark = "  (predecessor)"
		}
		fmt.Fprintf(w, "%s%s\n", v, mark)
	}
	return nil
}
