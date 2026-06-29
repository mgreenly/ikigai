package main

import "testing"

// R-4LKF-FB23
func TestDefaultMirrorPathTracksDurableStateDB(t *testing.T) {
	env := map[string]string{
		"DROPBOX_DB_PATH": "/opt/dropbox/state/dropbox.db",
	}
	got := defaultMirrorPath(func(key string) string { return env[key] })
	if want := "/opt/dropbox/state/mirror"; got != want {
		t.Fatalf("default mirror path = %q, want %q", got, want)
	}
}

func TestDefaultMirrorPathHonorsExplicitOverride(t *testing.T) {
	env := map[string]string{
		"DROPBOX_MIRROR_PATH": "/srv/private/dropbox-mirror",
		"DROPBOX_DB_PATH":     "/opt/dropbox/state/dropbox.db",
	}
	got := defaultMirrorPath(func(key string) string { return env[key] })
	if want := "/srv/private/dropbox-mirror"; got != want {
		t.Fatalf("default mirror path = %q, want explicit override %q", got, want)
	}
}
