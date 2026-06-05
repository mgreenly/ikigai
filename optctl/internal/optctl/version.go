package optctl

import (
	"strconv"
	"strings"
)

// lessVersion orders two release-dir names (e.g. "v1.2.0", "v1.10.0") by loose
// SemVer: split on '.', compare numerically component-by-component, with a stable
// string fallback for non-numeric or non-version names. v1.10.0 sorts after
// v1.2.0 (numeric, not lexicographic). Used to find the prior release for
// rollback and the newest N for prune.
func lessVersion(a, b string) bool {
	ai := strings.Split(strings.TrimPrefix(a, "v"), ".")
	bi := strings.Split(strings.TrimPrefix(b, "v"), ".")
	n := len(ai)
	if len(bi) < n {
		n = len(bi)
	}
	for i := 0; i < n; i++ {
		an, aerr := strconv.Atoi(ai[i])
		bn, berr := strconv.Atoi(bi[i])
		if aerr != nil || berr != nil {
			// Non-numeric component: fall back to a stable string compare overall.
			return a < b
		}
		if an != bn {
			return an < bn
		}
	}
	if len(ai) != len(bi) {
		return len(ai) < len(bi)
	}
	return a < b
}
