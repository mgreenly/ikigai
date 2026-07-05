package opsctl

import (
	"context"
	"fmt"
)

func (o *Opsctl) ensureWWWPerms(ctx context.Context, app string, l Layout) error {
	if err := o.System.ChownTree(ctx, app, "web", l.WWWRoot()); err != nil {
		return fmt.Errorf("chown www tree: %w", err)
	}
	for _, dir := range []string{
		l.WWWRoot(),
		l.WWWWorkingDir(),
		l.WWWPublicDir(),
		l.WWWPrivateDir(),
	} {
		if err := o.System.Chmod(ctx, dir, 0o2750); err != nil {
			return fmt.Errorf("chmod www dir %s: %w", dir, err)
		}
	}
	return nil
}
