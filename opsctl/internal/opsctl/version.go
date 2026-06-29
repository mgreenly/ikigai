package opsctl

import (
	"os"
	"strings"

	"golang.org/x/mod/semver"
)

const versionShape = "canonical SemVer 2.0 vMAJOR.MINOR.PATCH[-prerelease][+build]"

// validVersion accepts only canonical SemVer 2.0 release identities in Go module
// form: vMAJOR.MINOR.PATCH[-prerelease][+build]. Build metadata is valid identity
// text, but not part of SemVer precedence.
func validVersion(v string) bool {
	core := v
	if i := strings.IndexAny(core, "-+"); i >= 0 {
		core = core[:i]
	}
	if strings.Count(core, ".") != 2 {
		return false
	}
	if !semver.IsValid(v) {
		return false
	}
	withoutBuild := v[:len(v)-len(semver.Build(v))]
	return semver.Canonical(v) == withoutBuild
}

// compareVersion orders release identities by SemVer precedence. If precedence is
// equal, such as two builds that differ only by +metadata, the libexec file
// mtimes decide which build is newer. Missing files or equal mtimes leave the
// versions precedence-equal.
func (l Layout) compareVersion(a, b string) int {
	if !validVersion(a) || !validVersion(b) {
		panic("compareVersion requires canonical SemVer versions")
	}
	if c := semver.Compare(a, b); c != 0 {
		return c
	}

	ai, aerr := os.Stat(l.ReleaseLibexecFile(a))
	bi, berr := os.Stat(l.ReleaseLibexecFile(b))
	if aerr != nil || berr != nil {
		return 0
	}
	at := ai.ModTime()
	bt := bi.ModTime()
	switch {
	case at.Before(bt):
		return -1
	case at.After(bt):
		return 1
	default:
		return 0
	}
}
