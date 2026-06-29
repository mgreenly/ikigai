package opsctl

import (
	"os"
	"strings"

	"golang.org/x/mod/semver"
)

// validVersion accepts only canonical SemVer 2.0 release identities in Go module
// form: vMAJOR.MINOR.PATCH[-prerelease][+build]. Build metadata is valid identity
// text, but not part of SemVer precedence.
func validVersion(v string) bool {
	if !semver.IsValid(v) {
		return false
	}
	core := v
	if i := strings.IndexAny(core, "-+"); i >= 0 {
		core = core[:i]
	}
	return strings.Count(strings.TrimPrefix(core, "v"), ".") == 2
}

// compareVersion orders release identities by SemVer precedence. If precedence is
// equal, such as two builds that differ only by +metadata, the release binary
// mtimes decide which build is newer. Missing binaries or equal mtimes leave the
// versions precedence-equal.
func (l Layout) compareVersion(a, b string) int {
	if !validVersion(a) || !validVersion(b) {
		switch {
		case a < b:
			return -1
		case a > b:
			return 1
		default:
			return 0
		}
	}
	if c := semver.Compare(a, b); c != 0 {
		return c
	}

	ai, aerr := os.Stat(l.ReleaseBinary(a))
	bi, berr := os.Stat(l.ReleaseBinary(b))
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
