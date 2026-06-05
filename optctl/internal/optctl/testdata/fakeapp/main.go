// fakeapp is a stand-in for a real appkit-converted service binary, used by
// optctl's tests so no real service is needed (PLAN §C2: "tests provide a fake
// app binary … no real service needed"). It implements exactly the fixed verbs
// optctl drives — version, manifest, schema, migrate, backup, restore — over a
// trivial text-file "DB" so the schema-advance / backup / restore wiring is
// exercised end to end.
//
// Behavior is parameterised entirely by environment so one source builds every
// scenario the test needs:
//
//	FAKE_APP        the app name emitted in version/manifest (also the env prefix)
//	FAKE_VERSION    what `version` self-reports
//	FAKE_EMBEDDED   the binary's max-embedded schema version (an int)
//	FAKE_MANIFEST   the exact manifest.env body `manifest` prints
//	<APP>_DB_PATH   the "DB" file (a single integer = the applied schema version)
//
// The "DB" is a file whose contents is the applied schema version as decimal.
// migrate writes FAKE_EMBEDDED into it (advancing applied → embedded). schema
// prints `applied=<file or 0> embedded=<FAKE_EMBEDDED>`. backup/restore copy it.
package main

import (
	"fmt"
	"os"
	"strconv"
	"strings"
)

func main() {
	args := os.Args[1:]
	verb := ""
	if len(args) > 0 {
		verb = args[0]
		args = args[1:]
	}
	app := env("FAKE_APP", "fakeapp")
	switch verb {
	case "version":
		fmt.Println(env("FAKE_VERSION", "v0.0.0"))
	case "manifest":
		body := env("FAKE_MANIFEST", "APP="+app+"\nMOUNT=/srv/"+app+"/\nDEFAULT=false\nPORT=3999\nMCP=true\n")
		fmt.Print(body)
	case "schema":
		fmt.Printf("applied=%d embedded=%d\n", appliedVersion(app), embedded())
	case "migrate":
		// Idempotent like the real appkit runner: only write when there are pending
		// (higher-numbered) migrations. A no-op migrate must NOT touch the DB file,
		// so a no-schema-change deploy leaves data/<app>.db byte- and mtime-identical.
		if embedded() > appliedVersion(app) {
			writeDB(app, embedded())
		}
		fmt.Printf("migrated %s to version %d\n", app, embedded())
	case "backup":
		out := flagVal(args, "--out", dbPath(app)+".backup")
		copyFile(dbPath(app), out)
		fmt.Printf("backed up %s to %s\n", app, out)
	case "restore":
		from := flagVal(args, "--from", dbPath(app)+".backup")
		copyFile(from, dbPath(app))
		fmt.Printf("restored %s from %s\n", app, from)
	default:
		fmt.Fprintf(os.Stderr, "fakeapp: unknown verb %q\n", verb)
		os.Exit(2)
	}
}

func env(k, def string) string {
	if v := os.Getenv(k); v != "" {
		return v
	}
	return def
}

func embedded() int {
	n, _ := strconv.Atoi(env("FAKE_EMBEDDED", "0"))
	return n
}

func dbPath(app string) string {
	return env(strings.ToUpper(app)+"_DB_PATH", "")
}

func appliedVersion(app string) int {
	p := dbPath(app)
	if p == "" {
		return 0
	}
	b, err := os.ReadFile(p)
	if err != nil {
		return 0
	}
	n, _ := strconv.Atoi(strings.TrimSpace(string(b)))
	return n
}

func writeDB(app string, v int) {
	p := dbPath(app)
	if p == "" {
		fmt.Fprintln(os.Stderr, "fakeapp: no DB path set")
		os.Exit(1)
	}
	if err := os.WriteFile(p, []byte(strconv.Itoa(v)), 0o644); err != nil {
		fmt.Fprintln(os.Stderr, "fakeapp: write db:", err)
		os.Exit(1)
	}
}

func copyFile(src, dst string) {
	b, err := os.ReadFile(src)
	if err != nil {
		fmt.Fprintln(os.Stderr, "fakeapp: read", src, err)
		os.Exit(1)
	}
	if err := os.WriteFile(dst, b, 0o644); err != nil {
		fmt.Fprintln(os.Stderr, "fakeapp: write", dst, err)
		os.Exit(1)
	}
}

func flagVal(args []string, name, def string) string {
	for i, a := range args {
		if a == name && i+1 < len(args) {
			return args[i+1]
		}
		if strings.HasPrefix(a, name+"=") {
			return strings.TrimPrefix(a, name+"=")
		}
	}
	return def
}
