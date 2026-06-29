package opsctl

import (
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
