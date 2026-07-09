package opsctl

import (
	"context"
	"io"
	"testing"
)

func TestInitBoxDoesNotCreateServedTreeGroup(t *testing.T) {
	for _, tc := range []struct {
		name     string
		skipCert bool
	}{
		{name: "normal", skipCert: false},
		{name: "skip-cert", skipCert: true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			root := t.TempDir()
			sysRoot := t.TempDir()
			sys := &stubSystem{}
			o := &Opsctl{
				Root:    root,
				SysRoot: sysRoot,
				System:  sys,
				Out:     io.Discard,
				Err:     io.Discard,
			}

			opts := InitBoxOptions{
				DefaultApp: "dashboard",
				Domain:     "int.ikigenba.com",
				Email:      "ops@example.com",
				ApexBlock:  "server_name __DOMAIN__;\n",
				SkipCert:   tc.skipCert,
			}
			if err := o.InitBox(context.Background(), opts); err != nil {
				t.Fatalf("init-box: %v", err)
			}

			for _, op := range sys.opSeq() {
				if op == "ensure-group:web" || op == "add-user-to-group:nginx:web" {
					t.Fatalf("init-box requested retired served-tree group op %q; ops = %v", op, sys.opSeq())
				}
			}
		})
	}
}
