package opsctl

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestLayoutPathsUseAppRootScheme(t *testing.T) {
	root := t.TempDir()
	const app = "svc"

	l := NewLayout(root, app)

	tests := []struct {
		name string
		got  string
		want string
	}{
		{"LibexecDir", l.LibexecDir(), filepath.Join(root, app, "libexec")},
		{"LibexecBinary", l.LibexecBinary("v1.2.3"), filepath.Join(root, app, "libexec", app+"-v1.2.3")},
		{"RunLink", l.RunLink(), filepath.Join(root, app, "bin", "run")},
		{"StateDir", l.StateDir(), filepath.Join(root, app, "state")},
		{"DBPath", l.DBPath(), filepath.Join(root, app, "state", app+".db")},
		{"WWWDir", l.WWWDir(), filepath.Join(root, app, "state", "www")},
		{"WWWPublicDir", l.WWWPublicDir(), filepath.Join(root, app, "state", "www", "public")},
		{"WWWPrivateDir", l.WWWPrivateDir(), filepath.Join(root, app, "state", "www", "private")},
		{"CacheDir", l.CacheDir(), filepath.Join(root, app, "cache")},
		{"GenerationPath", l.GenerationPath(), filepath.Join(root, app, "cache", app+".db.generation")},
		{"BackupsDir", l.BackupsDir(), filepath.Join(root, app, "backups")},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if tt.got != tt.want {
				t.Fatalf("%s = %q, want %q", tt.name, tt.got, tt.want)
			}
		})
	}
}

func TestLayoutPerVersionAndActiveAccessors(t *testing.T) {
	const version = "v1.2.3"

	l := NewLayout("/opt", "crm")

	tests := []struct {
		name string
		got  string
		want string
	}{
		{"EtcVersionDir", l.EtcVersionDir(version), "/opt/crm/etc/v1.2.3"},
		{"NginxConfFile", l.NginxConfFile(version), "/opt/crm/etc/v1.2.3/nginx.conf"},
		{"ManifestFile", l.ManifestFile(version), "/opt/crm/etc/v1.2.3/manifest.env"},
		{"ShareVersionDir", l.ShareVersionDir(version), "/opt/crm/share/v1.2.3"},
		{"RunLink", l.RunLink(), "/opt/crm/bin/run"},
		{"EtcCurrentLink", l.EtcCurrentLink(), "/opt/crm/etc/current"},
		{"ActiveNginxConf", l.ActiveNginxConf(), "/opt/crm/etc/current/nginx.conf"},
		{"ActiveManifest", l.ActiveManifest(), "/opt/crm/etc/current/manifest.env"},
		{"ShareCurrentLink", l.ShareCurrentLink(), "/opt/crm/share/current"},
		{"LibexecBinary", l.LibexecBinary(version), "/opt/crm/libexec/crm-v1.2.3"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if tt.got != tt.want {
				t.Fatalf("%s = %q, want %q", tt.name, tt.got, tt.want)
			}
		})
	}
}

type stagePathRecordingRunner struct {
	fakeRunner
	binaries *[]string
}

func (r stagePathRecordingRunner) Run(ctx context.Context, binary, verb string, args []string, env []string) (string, error) {
	if verb == "version" || verb == "manifest" {
		*r.binaries = append(*r.binaries, binary)
	}
	return r.fakeRunner.Run(ctx, binary, verb, args, env)
}

func TestStageStagingParentUsesLayoutRoot(t *testing.T) {
	// R-65MT-7QEK
	root := t.TempDir()
	app := "ledger"
	version := "v1.2.3"
	l := NewLayout(root, app)
	artifact := stageBundleArtifact(t, app, version, "ledger-v1.2.3")
	var binaries []string

	o := newOpsctl(t, root, app, &stubSystem{}, fakeEnv(app, version, 1, ""))
	o.Runner = stagePathRecordingRunner{
		fakeRunner: fakeRunner{baseEnv: fakeEnv(app, version, 1, "")},
		binaries:   &binaries,
	}

	if err := o.Stage(context.Background(), app, version, artifact, false); err != nil {
		t.Fatalf("stage: %v", err)
	}
	if len(binaries) == 0 {
		t.Fatalf("stage did not preflight any bundle binary")
	}

	scratchDir := filepath.Dir(filepath.Dir(binaries[0]))
	parent := filepath.Clean(filepath.Dir(scratchDir))
	want := filepath.Clean(l.Root)
	if want != filepath.Clean(root) {
		t.Fatalf("layout root = %q, want test root %q", want, filepath.Clean(root))
	}
	if parent == "" {
		t.Fatalf("stage scratch parent is empty")
	}
	if parent == filepath.Clean(os.TempDir()) {
		t.Fatalf("stage scratch parent = %q, want layout root %q", parent, want)
	}
	if parent != want {
		t.Fatalf("stage scratch parent = %q, want layout root %q", parent, want)
	}
}
