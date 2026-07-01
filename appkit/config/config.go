// Package config is the env contract at a suite app's composition root. It
// parses the universal runtime knobs from the environment and composes the
// public RESOURCE_ID / AUTH_SERVER in-binary from IKIGENBA_DOMAIN + the app's
// Mount — work that used to live in the deleted bin/build run-wrapper and now
// moves into Go (PLAN §1.1).
//
// Secrets are NOT read here. Per PLAN §2.8 a service reads its own secrets at its
// own composition root (Spec.Config); appkit's config touches only non-secret
// env. The envOr* helpers are exported for the service-side Config hook to reuse.
package config

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

// Config is the resolved universal runtime configuration for one app.
type Config struct {
	IP             string // listen interface — loopback in prod (env <APP>_IP)
	Port           int    // loopback port (env <APP>_PORT, default Spec.Port)
	LogLevel       string // debug|info|warn|error (env <APP>_LOG_LEVEL)
	ResourceID     string // canonical resource id, composed from IKIGENBA_DOMAIN+Mount
	AuthServer     string // dashboard AS base URL, composed from IKIGENBA_DOMAIN
	DBPath         string // SQLite file (env <APP>_DB_PATH)
	GenerationPath string // event-plane epoch sidecar (env <APP>_GENERATION_PATH)
}

// Resolve reads the universal env contract for the named app. mount is the app's
// /srv/<app>/ prefix (or "/" for the apex); defaultPort is Spec.Port. getenv is
// the injected environment (os.Getenv in prod, a map in tests).
//
// RESOURCE_ID / AUTH_SERVER are composed from IKIGENBA_DOMAIN + mount when
// IKIGENBA_DOMAIN is set (the production path); otherwise they fall back to the
// localhost dev values an explicit <APP>_RESOURCE_ID / <APP>_AUTH_SERVER can
// still override. This is the in-binary move of the run-wrapper's composition.
func Resolve(app, mount string, defaultPort int, getenv func(string) string) (Config, error) {
	up := strings.ToUpper(app)
	dbPathKey := up + "_DB_PATH"
	genPathKey := up + "_GENERATION_PATH"

	hasExplicitDataPaths := strings.TrimSpace(getenv(dbPathKey)) != "" && strings.TrimSpace(getenv(genPathKey)) != ""
	if strings.TrimSpace(getenv("IKIGENBA_DOMAIN")) != "" && strings.TrimSpace(getenv("IKIGENBA_ROOT")) == "" && !hasExplicitDataPaths {
		return Config{}, fmt.Errorf("IKIGENBA_ROOT is required when IKIGENBA_DOMAIN is set")
	}

	port, err := EnvOrInt(getenv, up+"_PORT", defaultPort)
	if err != nil {
		return Config{}, err
	}

	resourceID, authServer := composeURLs(getenv, up, mount)

	dbPath, genPath := composeDataPaths(getenv, up, app)
	if getenv(dbPathKey) != "" {
		if err := ensureParentDir(dbPath, 0o750); err != nil {
			return Config{}, fmt.Errorf("%s: %w", dbPathKey, err)
		}
	}
	if getenv(genPathKey) != "" {
		if err := ensureParentDir(genPath, 0o755); err != nil {
			return Config{}, fmt.Errorf("%s: %w", genPathKey, err)
		}
	}

	return Config{
		IP:             EnvOr(getenv, up+"_IP", "127.0.0.1"),
		Port:           port,
		LogLevel:       EnvOr(getenv, up+"_LOG_LEVEL", "info"),
		ResourceID:     resourceID,
		AuthServer:     authServer,
		DBPath:         dbPath,
		GenerationPath: genPath,
	}, nil
}

// composeURLs derives RESOURCE_ID and AUTH_SERVER. Production composes them from
// IKIGENBA_DOMAIN + mount (RESOURCE_ID = https://<domain><mount>mcp,
// AUTH_SERVER = https://<domain>); an explicit <APP>_RESOURCE_ID /
// <APP>_AUTH_SERVER override wins (and is the local-dev default behind nginx on
// :8080).
func composeURLs(getenv func(string) string, up, mount string) (resourceID, authServer string) {
	domain := strings.TrimSpace(getenv("IKIGENBA_DOMAIN"))
	if domain != "" {
		resourceID = "https://" + domain + mount + "mcp"
		authServer = "https://" + domain
	} else {
		resourceID = "http://localhost:8080" + mount + "mcp"
		authServer = "http://localhost:8080"
	}
	resourceID = EnvOr(getenv, up+"_RESOURCE_ID", resourceID)
	authServer = EnvOr(getenv, up+"_AUTH_SERVER", authServer)
	return resourceID, authServer
}

// composeDataPaths composes DB and generation sidecar paths from IKIGENBA_ROOT,
// with explicit per-app env overrides winning over composed defaults.
func composeDataPaths(getenv func(string) string, up, app string) (db, gen string) {
	root := strings.TrimSpace(getenv("IKIGENBA_ROOT"))
	if root != "" {
		db = filepath.Join(root, app, "state", app+".db")
		gen = filepath.Join(root, app, "cache", app+".db.generation")
	} else {
		db = "./tmp/" + app + ".db"
		gen = "./tmp/" + app + ".db.generation"
	}
	if override := getenv(up + "_DB_PATH"); override != "" {
		db = override
		gen = db + ".generation"
	}
	gen = EnvOr(getenv, up+"_GENERATION_PATH", gen)
	return db, gen
}

// EnvOr returns getenv(key) when non-empty, else def.
func EnvOr(getenv func(string) string, key, def string) string {
	if v := getenv(key); v != "" {
		return v
	}
	return def
}

func ensureParentDir(path string, mode os.FileMode) error {
	dir := filepath.Dir(path)
	if dir == "." || dir == "" {
		return nil
	}
	if err := os.MkdirAll(dir, mode); err != nil {
		return fmt.Errorf("create parent dir %s: %w", dir, err)
	}
	return nil
}

// EnvOrInt returns def when key is unset/empty, the parsed value when it holds a
// valid integer, and an error naming the variable otherwise — a malformed
// override fails loudly rather than silently reverting to def.
func EnvOrInt(getenv func(string) string, key string, def int) (int, error) {
	v := getenv(key)
	if v == "" {
		return def, nil
	}
	n, err := strconv.Atoi(v)
	if err != nil {
		return 0, fmt.Errorf("%s: invalid integer %q", key, v)
	}
	return n, nil
}

// EnvOrDuration is the duration counterpart to EnvOrInt.
func EnvOrDuration(getenv func(string) string, key string, def time.Duration) (time.Duration, error) {
	v := getenv(key)
	if v == "" {
		return def, nil
	}
	d, err := time.ParseDuration(v)
	if err != nil {
		return 0, fmt.Errorf("%s: invalid duration %q", key, v)
	}
	return d, nil
}
