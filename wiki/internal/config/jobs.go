package config

import (
	"fmt"
	"sort"
	"strings"
)

// The jobs config (design §6) is config, NOT a table: it unifies digests and lint
// jobs into one list a worker can select and start. Each entry has a name, a
// trigger (the cron schedule that fires it — "cron.daily"), and — for DIGEST
// entries only — a select clause naming the event sources it sweeps. Lint entries
// bind name → trigger with NO selector (design §6: "select is legal only for
// digest entries"); selection treats both kinds identically (the worker spine).
//
// This phase (P8) wires the DIGEST half: the jobs config, the boot-time partition
// check, and the cron fan-out (a cron schedule binds the entries triggered by it).
// Lint entries (P9a–P9c) plug into the same config without reshaping it.

// JobKind distinguishes a digest entry (consumes event rows via a selector) from a
// lint job (maintains existing content, no selector). Selection treats them
// identically; the kind only gates whether a selector is legal (design §6).
type JobKind int

const (
	// JobDigest is an integrator entry: it sweeps the event rows its selector
	// matches and gives them one integration pass (design §5).
	JobDigest JobKind = iota
	// JobLint is a lint job: name → trigger, no selector (design §6).
	JobLint
)

// Job is one entry in the jobs config (design §6). Name is the stable job name
// (runs.job, the cron TryLock key). Trigger is the cron schedule that fires it
// ("cron.daily"). For a digest, SourcePrefixes names the event-source prefixes it
// sweeps (the deterministic, partition-checkable form of design §6's
// "source LIKE 'crm:%'" — a digest selects every event row whose source begins
// with one of these prefixes). A lint job leaves SourcePrefixes empty.
type Job struct {
	Name           string
	Kind           JobKind
	Trigger        string
	SourcePrefixes []string
}

// Jobs is the whole jobs config — the list of digest and lint entries.
type Jobs struct {
	Entries []Job
}

// Bindings maps a cron SCHEDULE NAME (the bare schedule, e.g. "daily", as the
// worker sees it on a cron:<name> inbox row) to the job names triggered by it —
// the cron fan-out (design §5: "a cron row is a tiny fan-out: the worker looks up
// the bound entries for the trigger"). The worker runs each bound entry as its own
// runs row with caused_by = the cron row id, and the completion-time join stamps
// the cron row only when all bound entries' runs have succeeded.
func (j Jobs) Bindings() map[string][]string {
	out := map[string][]string{}
	for _, e := range j.Entries {
		schedule := scheduleName(e.Trigger)
		if schedule == "" {
			continue
		}
		out[schedule] = append(out[schedule], e.Name)
	}
	// Deterministic order so the completion-time join probes the same set each run.
	for k := range out {
		sort.Strings(out[k])
	}
	return out
}

// DigestEntries returns just the digest entries (the integrators the worker's cron
// map binds to a Compiler-backed integrator). Lint entries are wired separately
// (P9a–P9c).
func (j Jobs) DigestEntries() []Job {
	var out []Job
	for _, e := range j.Entries {
		if e.Kind == JobDigest {
			out = append(out, e)
		}
	}
	return out
}

// LintEntries returns just the lint entries (the maintenance jobs the worker's
// cron map binds to a lint integrator; design §6). Symmetric with DigestEntries;
// lint entries carry no selector, so the cron fan-out simply runs each bound lint
// job. The lint plumbing (P9a) and the later lint jobs (P9b/P9c) register here.
func (j Jobs) LintEntries() []Job {
	var out []Job
	for _, e := range j.Entries {
		if e.Kind == JobLint {
			out = append(out, e)
		}
	}
	return out
}

// StandardLintEntries is the registered set of lint jobs (design §6's jobs yaml),
// the no-selector entries the composition root folds into the jobs config beside
// the digest entries. Each binds a stable job name (the runs.job / lint TryLock
// key the lint package's *JobName constants mirror) to its cron trigger. The names
// are kept as literals here (config has no dependency on internal/lint) and must
// stay in sync with the lint package's job-name constants. A lint job appears here
// only once its job code exists: lint-dups (P9a) and lint-sweep (P9b) are
// registered; lint-stale registers when P9c lands.
func StandardLintEntries() []Job {
	return []Job{
		{Name: "lint-dups", Kind: JobLint, Trigger: "cron.weekly"},
		{Name: "lint-sweep", Kind: JobLint, Trigger: "cron.monthly"},
	}
}

// scheduleName strips the "cron." trigger prefix to the bare schedule name the
// worker matches against a "cron:<name>" inbox row's source. A trigger that is not
// a cron trigger yields "" (it binds no cron schedule).
func scheduleName(trigger string) string {
	const p = "cron."
	if strings.HasPrefix(trigger, p) {
		return strings.TrimPrefix(trigger, p)
	}
	return ""
}

// PartitionCheck is the boot-time guard (design §5/§6): the digest selectors must
// PARTITION the consumed event sources. It refuses to boot on two faults:
//
//   - OVERLAP — two digest selectors both match the same consumed source. An event
//     row would be swept by two digests (double-counted knowledge); the worst
//     ambiguity, so it is a hard boot refusal.
//   - UNMATCHED — a consumed source no digest selector matches. Those events would
//     accumulate forever, never compiled (silently lost knowledge). Surfaced as a
//     boot refusal so the gap is loud, not silent.
//
// consumed is the list of event-source prefixes the service consumes (the upstream
// names from the Consumes list — "crm", "ledger" — each producing event rows whose
// source is "<upstream>:<type>"). A digest's SourcePrefixes match a consumed
// source when the consumed prefix begins with (or equals) a selector prefix.
//
// A selector prefix that matches NO consumed source is also surfaced — a digest
// configured against a source the service does not consume is a config error that
// would silently never fire.
func (j Jobs) PartitionCheck(consumed []string) error {
	// Each consumed source must be matched by EXACTLY ONE digest selector prefix.
	matchedBy := map[string][]string{} // consumed source → digest names matching it
	var unusedPrefix []string          // selector prefixes matching no consumed source

	for _, e := range j.DigestEntries() {
		if len(e.SourcePrefixes) == 0 {
			return fmt.Errorf("jobs: digest %q has no selector (a digest must select event sources — §6)", e.Name)
		}
		for _, pfx := range e.SourcePrefixes {
			hit := false
			for _, src := range consumed {
				if sourceMatches(src, pfx) {
					matchedBy[src] = append(matchedBy[src], e.Name)
					hit = true
				}
			}
			if !hit {
				unusedPrefix = append(unusedPrefix, fmt.Sprintf("%s (digest %q)", pfx, e.Name))
			}
		}
	}

	// OVERLAP: any consumed source matched by more than one digest.
	var overlaps []string
	for src, names := range matchedBy {
		if len(names) > 1 {
			sort.Strings(names)
			overlaps = append(overlaps, fmt.Sprintf("%s → %s", src, strings.Join(names, ", ")))
		}
	}
	if len(overlaps) > 0 {
		sort.Strings(overlaps)
		return fmt.Errorf("jobs: overlapping digest selectors (an event would be swept twice): %s", strings.Join(overlaps, "; "))
	}

	// UNMATCHED: any consumed source no digest selector matches.
	var unmatched []string
	for _, src := range consumed {
		if len(matchedBy[src]) == 0 {
			unmatched = append(unmatched, src)
		}
	}
	if len(unmatched) > 0 {
		sort.Strings(unmatched)
		return fmt.Errorf("jobs: consumed sources matched by no digest selector (events would never be compiled): %s", strings.Join(unmatched, ", "))
	}

	// A selector that matches nothing is a config error (surfaced, design §5/§6).
	if len(unusedPrefix) > 0 {
		sort.Strings(unusedPrefix)
		return fmt.Errorf("jobs: digest selector prefixes match no consumed source: %s", strings.Join(unusedPrefix, ", "))
	}

	return nil
}

// sourceMatches reports whether a consumed source-prefix is covered by a digest
// selector prefix. A consumed source "crm" is covered by a selector prefix "crm"
// (or "crm:"); the comparison is prefix-anchored on the colon boundary so "crm"
// never accidentally matches "crmx".
func sourceMatches(consumedSource, selectorPrefix string) bool {
	c := strings.TrimSuffix(consumedSource, ":")
	p := strings.TrimSuffix(selectorPrefix, ":")
	return c == p
}
