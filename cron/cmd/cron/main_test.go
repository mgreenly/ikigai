package main

import (
	"os/exec"
	"strings"
	"testing"
)

// R-4LKF-FB23
func TestManifestDeclaresCronPathRoutedProducer(t *testing.T) {
	cmd := exec.Command("go", "run", ".", "manifest")
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("cron manifest: %v\n%s", err, out)
	}
	got := string(out)
	for _, want := range []string{
		"APP=cron\n",
		"MOUNT=/srv/cron/\n",
		"PORT=3007\n",
		"MCP=true\n",
		"FEED=/feed\n",
		"OUTBOX_RETENTION_DAYS=7\n",
		"OUTBOX_RETENTION_MAX_ROWS=1000000\n",
	} {
		if !strings.Contains(got, want) {
			t.Fatalf("cron manifest missing %q\n--- manifest ---\n%s", want, got)
		}
	}
}
