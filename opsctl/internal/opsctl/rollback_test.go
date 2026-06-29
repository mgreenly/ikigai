package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestRollback_SwapsBinRunBackToPriorLibexecBinary(t *testing.T) {
	// R-3UQN-0CQT
	root := t.TempDir()
	app := "ledger"
	l := NewLayout(root, app)
	sys := &stubSystem{}

	o1 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.0.0", 1, ""))
	if err := stageAndDeploy(t, o1, app, "v1.0.0", stageArtifact(t, "ledger-v1.0.0")); err != nil {
		t.Fatalf("deploy v1.0.0: %v", err)
	}
	o2 := newOpsctl(t, root, app, sys, fakeEnv(app, "v1.1.0", 1, ""))
	if err := stageAndDeploy(t, o2, app, "v1.1.0", stageArtifact(t, "ledger-v1.1.0")); err != nil {
		t.Fatalf("deploy v1.1.0: %v", err)
	}

	if err := o2.Rollback(context.Background(), app, ""); err != nil {
		t.Fatalf("rollback: %v", err)
	}
	target, err := os.Readlink(l.RunLink())
	if err != nil {
		t.Fatalf("read bin/run: %v", err)
	}
	if want := l.runTarget("v1.0.0"); target != want {
		t.Fatalf("bin/run target after rollback = %q, want %q", target, want)
	}
	resolved, err := filepath.EvalSymlinks(l.RunLink())
	if err != nil {
		t.Fatalf("resolve bin/run after rollback: %v", err)
	}
	if resolved != l.LibexecBinary("v1.0.0") {
		t.Fatalf("bin/run resolves to %q, want %q", resolved, l.LibexecBinary("v1.0.0"))
	}
	if got := readRunVersion(t, l); got != "v1.0.0" {
		t.Fatalf("live version after rollback = %q, want v1.0.0", got)
	}
}
