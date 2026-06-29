package backup

import (
	"bytes"
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"appkit"

	"dashboard/internal/db"
)

// freshDB stands up a migrated dashboard DB under a temp dir and returns its path.
func freshDB(t *testing.T) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "dashboard.db")
	conn, err := db.Open(path)
	if err != nil {
		t.Fatalf("db.Open: %v", err)
	}
	conn.Close()
	return path
}

// TestBackup_LocalSnapshotViaOut confirms the opsctl contract: `backup --out
// <path>` produces a local VACUUM-INTO snapshot (the pre-migration backup
// install/rollback rely on), NOT the S3 path. No bucket env is set, so reaching
// the S3 branch would error — its absence proves the --out dispatch.
func TestBackup_LocalSnapshotViaOut(t *testing.T) {
	dbPath := freshDB(t)
	out := filepath.Join(t.TempDir(), "pre-v1.db")
	var stdout, stderr bytes.Buffer

	err := Backup(context.Background(), appkit.BackupReq{
		App: "dashboard", DBPath: dbPath,
		Args: []string{"--out", out}, Stdout: &stdout, Stderr: &stderr,
	})
	if err != nil {
		t.Fatalf("Backup --out: %v", err)
	}
	if _, err := os.Stat(out); err != nil {
		t.Fatalf("snapshot not written at %s: %v", out, err)
	}
}

// TestRestore_LocalFromVia confirms `restore --from <snapshot>` replaces the DB
// file locally (the rollback contract), not via S3.
func TestRestore_LocalFromVia(t *testing.T) {
	src := freshDB(t)
	snap := filepath.Join(t.TempDir(), "snap.db")
	var b1, b2 bytes.Buffer
	if err := Backup(context.Background(), appkit.BackupReq{
		App: "dashboard", DBPath: src, Args: []string{"--out", snap}, Stdout: &b1, Stderr: &b2,
	}); err != nil {
		t.Fatalf("seed snapshot: %v", err)
	}

	dst := filepath.Join(t.TempDir(), "live.db")
	var stdout, stderr bytes.Buffer
	if err := Restore(context.Background(), appkit.RestoreReq{
		App: "dashboard", DBPath: dst, Args: []string{"--from", snap}, Stdout: &stdout, Stderr: &stderr,
	}); err != nil {
		t.Fatalf("Restore --from: %v", err)
	}
	if _, err := os.Stat(dst); err != nil {
		t.Fatalf("restored DB not at %s: %v", dst, err)
	}
}

// TestBackup_S3RequiresBucket confirms the operator (no --out) branch fails loudly
// when IKIGENBA_BACKUP_BUCKET is unset — it must never silently no-op, and it must
// never read a secret to get there.
func TestBackup_S3RequiresBucket(t *testing.T) {
	t.Setenv("IKIGENBA_BACKUP_BUCKET", "")
	t.Setenv("IKIGENBA_DOMAIN", "")
	dbPath := freshDB(t)
	var stdout, stderr bytes.Buffer
	err := Backup(context.Background(), appkit.BackupReq{
		App: "dashboard", DBPath: dbPath, Args: nil, Stdout: &stdout, Stderr: &stderr,
	})
	if err == nil {
		t.Fatal("S3 backup with no bucket: want error, got nil")
	}
}

func TestBackupRestore_S3BranchRetiredButLocalContractRemains(t *testing.T) {
	// R-4ALB-ZDDU
	t.Setenv("IKIGENBA_BACKUP_BUCKET", "")
	t.Setenv("IKIGENBA_DOMAIN", "")
	dbPath := freshDB(t)
	snap := filepath.Join(t.TempDir(), "snap.db")
	var stdout, stderr bytes.Buffer
	if err := Backup(context.Background(), appkit.BackupReq{
		App: "dashboard", DBPath: dbPath, Args: []string{"--out", snap}, Stdout: &stdout, Stderr: &stderr,
	}); err != nil {
		t.Fatalf("local Backup --out: %v", err)
	}
	if err := Backup(context.Background(), appkit.BackupReq{
		App: "dashboard", DBPath: dbPath, Args: nil, Stdout: &stdout, Stderr: &stderr,
	}); err == nil || !strings.Contains(err.Error(), "retired") {
		t.Fatalf("S3 Backup error = %v, want retired", err)
	}
	restorePath := filepath.Join(t.TempDir(), "restored.db")
	if err := Restore(context.Background(), appkit.RestoreReq{
		App: "dashboard", DBPath: restorePath, Args: []string{"--from", snap}, Stdout: &stdout, Stderr: &stderr,
	}); err != nil {
		t.Fatalf("local Restore --from: %v", err)
	}
	if err := Restore(context.Background(), appkit.RestoreReq{
		App: "dashboard", DBPath: restorePath, Args: nil, Stdout: &stdout, Stderr: &stderr,
	}); err == nil || !strings.Contains(err.Error(), "retired") {
		t.Fatalf("S3 Restore error = %v, want retired", err)
	}
}

// TestFlagValue covers the --name value / --name=value / -name forms.
func TestFlagValue(t *testing.T) {
	cases := []struct {
		args    []string
		want    string
		present bool
	}{
		{[]string{"--out", "/p"}, "/p", true},
		{[]string{"--out=/p"}, "/p", true},
		{[]string{"-out", "/p"}, "/p", true},
		{[]string{"--from", "/x"}, "", false}, // looking for "out"
		{nil, "", false},
	}
	for _, tc := range cases {
		got, ok := flagValue(tc.args, "out")
		if got != tc.want || ok != tc.present {
			t.Errorf("flagValue(%v,out) = (%q,%v), want (%q,%v)", tc.args, got, ok, tc.want, tc.present)
		}
	}
}
