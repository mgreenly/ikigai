package opsctl

import (
	"context"
	"strings"
	"testing"
)

// newOpsForOps builds an Opsctl wired only to the recording stub system — the
// passthrough verbs (tail + start/stop/restart/enable/disable) need no real
// release tree, just the seam that records the systemctl/journalctl argv.
func newOpsForOps(sys *stubSystem) *Opsctl {
	return &Opsctl{
		Root:   "",
		System: sys,
		Out:    &strings.Builder{},
		Err:    &strings.Builder{},
	}
}

// lastOp returns the most recently recorded seam op (the argv string), or "".
func lastOp(sys *stubSystem) string {
	seq := sys.opSeq()
	if len(seq) == 0 {
		return ""
	}
	return seq[len(seq)-1]
}

// TestServiceControlPassthroughs asserts each control verb records the exact
// `systemctl <verb> <app> [extra…]` argv via the stub, with extra args forwarded
// verbatim (no reordering at this layer).
func TestServiceControlPassthroughs(t *testing.T) {
	ctx := context.Background()
	cases := []struct {
		verb string
		call func(o *Opsctl) error
		want string
	}{
		{"start", func(o *Opsctl) error { return o.Start(ctx, "crm", nil) }, "systemctl:start crm"},
		{"stop", func(o *Opsctl) error { return o.Stop(ctx, "crm", nil) }, "systemctl:stop crm"},
		{"restart", func(o *Opsctl) error { return o.Restartd(ctx, "crm", nil) }, "systemctl:restart crm"},
		{"enable", func(o *Opsctl) error { return o.Enable(ctx, "crm", nil) }, "systemctl:enable crm"},
		{"disable", func(o *Opsctl) error { return o.Disable(ctx, "crm", nil) }, "systemctl:disable crm"},
		{"restart+flags", func(o *Opsctl) error { return o.Restartd(ctx, "ledger", []string{"--no-block"}) }, "systemctl:restart ledger --no-block"},
	}
	for _, tc := range cases {
		sys := &stubSystem{}
		o := newOpsForOps(sys)
		if err := tc.call(o); err != nil {
			t.Fatalf("%s: %v", tc.verb, err)
		}
		if got := lastOp(sys); got != tc.want {
			t.Errorf("%s: recorded %q, want %q", tc.verb, got, tc.want)
		}
	}
}

// TestTailDefaultFollow asserts that with no extra args, tail injects the
// default `-f` (follow live).
func TestTailDefaultFollow(t *testing.T) {
	sys := &stubSystem{}
	o := newOpsForOps(sys)
	if err := o.Tail(context.Background(), "crm", nil); err != nil {
		t.Fatalf("tail: %v", err)
	}
	if got := lastOp(sys); got != "journalctl:-u crm -f" {
		t.Errorf("tail default: recorded %q, want %q", got, "journalctl:-u crm -f")
	}
}

// TestTailSuppressDefaultFollow asserts the default `-f` is suppressed when the
// args already carry any of the follow/range set (-f/-n/--since/--no-tail), and
// that `--no-tail` (an opsctl-only knob) is dropped before reaching journalctl.
func TestTailSuppressDefaultFollow(t *testing.T) {
	cases := []struct {
		name string
		args []string
		want string
	}{
		{"explicit -f", []string{"-f"}, "journalctl:-u crm -f"},
		{"-n window", []string{"-n", "100"}, "journalctl:-u crm -n 100"},
		{"--since range", []string{"--since", "1h"}, "journalctl:-u crm --since 1h"},
		{"--no-tail dropped", []string{"--no-tail"}, "journalctl:-u crm"},
		{"--no-tail with -n", []string{"--no-tail", "-n", "50"}, "journalctl:-u crm -n 50"},
	}
	for _, tc := range cases {
		sys := &stubSystem{}
		o := newOpsForOps(sys)
		if err := o.Tail(context.Background(), "crm", tc.args); err != nil {
			t.Fatalf("%s: %v", tc.name, err)
		}
		if got := lastOp(sys); got != tc.want {
			t.Errorf("%s: recorded %q, want %q", tc.name, got, tc.want)
		}
	}
}
