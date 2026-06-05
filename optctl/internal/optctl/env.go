package optctl

import "os"

// currentEnv returns the process environment as a KEY=VALUE slice. Wrapped so the
// RealRunner can layer per-verb overrides on top of the inherited env.
func currentEnv() []string { return os.Environ() }
