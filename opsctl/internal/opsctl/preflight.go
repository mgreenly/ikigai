package opsctl

import (
	"context"
	"debug/elf"
	"fmt"
	"strings"
)

// preflight runs the install gates that refuse a bad artifact BEFORE anything
// live is touched (ADR install step 1): the artifact is a static linux/amd64
// binary, its `version` self-report matches the <version> arg (the binary can't
// lie about which version it is), and its `manifest` parses into a well-formed
// manifest.env. Any failure aborts with the live release untouched.
func (o *Opsctl) preflight(ctx context.Context, artifact, app, version string) error {
	if err := checkStaticAMD64(artifact); err != nil {
		return err
	}

	// The binary self-reports its version; it must match the version arg. Trim the
	// "<version> (<sha>[-dirty])" stamp down to the leading version token.
	out, err := o.Runner.Run(ctx, artifact, "version", nil, nil)
	if err != nil {
		return fmt.Errorf("preflight: %q version: %w", app, err)
	}
	got := versionToken(out)
	if got != version {
		return fmt.Errorf("preflight: artifact self-reports version %q, want %q", got, version)
	}

	// The manifest must parse and carry at least the APP key naming this app.
	manOut, err := o.Runner.Run(ctx, artifact, "manifest", nil, nil)
	if err != nil {
		return fmt.Errorf("preflight: %q manifest: %w", app, err)
	}
	man, err := parseManifest(manOut)
	if err != nil {
		return fmt.Errorf("preflight: %q manifest: %w", app, err)
	}
	if man["APP"] != app {
		return fmt.Errorf("preflight: manifest APP=%q does not match app %q", man["APP"], app)
	}
	return nil
}

// checkStaticAMD64 parses the artifact as an ELF and asserts it is a static
// linux/amd64 binary (no PT_INTERP / no DT_NEEDED ⇒ no dynamic linkage). PLAN
// §2.1 / §1.1: the contract is one self-contained static linux/amd64 binary.
func checkStaticAMD64(path string) error {
	f, err := elf.Open(path)
	if err != nil {
		return fmt.Errorf("preflight: artifact %s is not a valid ELF binary: %w", path, err)
	}
	defer f.Close()

	if f.Machine != elf.EM_X86_64 {
		return fmt.Errorf("preflight: artifact arch is %s, want amd64 (linux/amd64)", f.Machine)
	}

	// A dynamic interpreter (PT_INTERP) means a dynamically linked binary.
	for _, p := range f.Progs {
		if p.Type == elf.PT_INTERP {
			return fmt.Errorf("preflight: artifact %s is dynamically linked (has PT_INTERP); want a static binary", path)
		}
	}
	// Belt-and-suspenders: any DT_NEEDED entry also means dynamic linkage.
	if libs, err := f.ImportedLibraries(); err == nil && len(libs) > 0 {
		return fmt.Errorf("preflight: artifact %s links shared libraries %v; want a static binary", path, libs)
	}
	return nil
}

// versionToken extracts the leading version token from `<app> version` output,
// which is "<version>" or "<version> (<sha>[-dirty])". Returns the first
// whitespace-delimited field of the first line.
func versionToken(out string) string {
	line := strings.TrimSpace(out)
	if i := strings.IndexByte(line, '\n'); i >= 0 {
		line = line[:i]
	}
	if i := strings.IndexByte(line, ' '); i >= 0 {
		line = line[:i]
	}
	return strings.TrimSpace(line)
}

// commitToken extracts the commit-SHA field from `<app> version` output, which is
// "<version>" or "<version> (<sha>[-dirty])". It returns the contents of the
// parenthesised "(<sha>[-dirty])" field (without the parens), or "" if the
// self-report carries no commit stamp. The stage collision guard compares these:
// versionToken strips the SHA, so it cannot tell two builds of the same version
// apart — only the commit token can.
func commitToken(out string) string {
	line := strings.TrimSpace(out)
	if i := strings.IndexByte(line, '\n'); i >= 0 {
		line = line[:i]
	}
	open := strings.IndexByte(line, '(')
	if open < 0 {
		return ""
	}
	close := strings.IndexByte(line[open:], ')')
	if close < 0 {
		return ""
	}
	return strings.TrimSpace(line[open+1 : open+close])
}
