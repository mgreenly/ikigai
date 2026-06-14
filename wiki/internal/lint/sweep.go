package lint

import (
	"context"
	"fmt"
	"strings"

	"wiki/internal/integrate"
	"wiki/internal/page"
)

// SweepJobName is lint-sweep's stable job name (runs.job; the cron/lint TryLock
// key — at most one in-flight lint-sweep run, design §6).
const SweepJobName = "lint-sweep"

// sweepStore is the slice of *page.Store the zero-LLM lint-sweep needs (design §6,
// P9b): enumerate every subject (the wide scan), run the two candidate FTS queries
// per subject (the same lane pair resolution uses), and flag any pair above the
// flag threshold in its own short transaction. Narrowed to an interface so the job
// is unit-testable with a fake (no live DB), even though it touches NO LLM.
type sweepStore interface {
	EnumerateSweepSubjects(ctx context.Context) ([]page.SweepSubject, error)
	Candidates(ctx context.Context, typ page.Type, nameQuery, claimQuery string, limit int) ([]page.Candidate, error)
	FlagDupAuto(ctx context.Context, a, b string) error
}

// SweepJob is the lint-sweep maintenance job (design §6, P9b): the proactive,
// FULLY MECHANICAL (zero-LLM) walker that finds duplicates built from disjoint
// streams (Bob-from-email vs Robert-from-CRM) that integration-time writers never
// co-examine. It is FLAG-ONLY — it inserts dup_flags via FlagDup and never judges
// or merges (that is lint-dups' job, P9a). It satisfies integrate.Integrator so
// the worker spine selects and runs it exactly like any other job; its Integrate
// does its own per-flag writes, so it returns an EMPTY manifest and the worker's
// end-of-run Commit is a harmless no-op stamp. It carries NO integration-tier
// slice — it has no live call site.
type SweepJob struct {
	store sweepStore
	// flagLimit is the config-injected per-lane flag threshold (WIKI_SWEEP_LIMIT,
	// eval-harness knob, obligation 2): the top-N cap on each of the two candidate
	// FTS queries. A non-positive value falls back to the default so a mis-set knob
	// never disables the sweep entirely.
	flagLimit int
}

// DefaultSweepLimit is the per-lane flag threshold used when the config knob is
// unset (mirrors the candidate-lane default of design §4.3).
const DefaultSweepLimit = 5

// NewSweepJob builds the lint-sweep job over the page store and the config-injected
// per-lane flag threshold. The threshold is injected — never a constant at the call
// site (design §10 / obligation 2).
func NewSweepJob(store sweepStore, flagLimit int) *SweepJob {
	if flagLimit <= 0 {
		flagLimit = DefaultSweepLimit
	}
	return &SweepJob{store: store, flagLimit: flagLimit}
}

// Job is the integrate.Integrator job name (runs.job). lint-sweep runs under it.
func (j *SweepJob) Job() string { return SweepJobName }

// Integrate runs one lint-sweep (design §6): enumerate every subject with a page,
// and for each run the same two candidate FTS queries the document pass uses
// (name/alias lane + claim/body lane), flagging every returned candidate pair via
// FlagDup. The pair UNIQUE makes it idempotent and polite — a re-run flags nothing
// new, and a settled (merged/dismissed) pair bounces off the conflict. Each flag is
// its own short transaction (FlagDupAuto), so a failure mid-sweep leaves the flags
// already written durable and the rest to a later run. The returned manifest is
// empty: lint-sweep owns its writes, so the worker's end-of-run Commit is a no-op.
func (j *SweepJob) Integrate(ctx context.Context, _ integrate.Unit) (*integrate.Manifest, error) {
	subjects, err := j.store.EnumerateSweepSubjects(ctx)
	if err != nil {
		return nil, fmt.Errorf("lint-sweep: enumerate: %w", err)
	}
	for _, s := range subjects {
		if err := j.sweepOne(ctx, s); err != nil {
			return nil, fmt.Errorf("lint-sweep: subject %q: %w", s.SubjectID, err)
		}
	}
	return &integrate.Manifest{}, nil
}

// sweepOne runs the two candidate queries for one subject and flags every distinct
// other-subject hit. A candidate equal to the subject itself is skipped (a subject
// is never its own duplicate); FlagDupAuto canonical-orders the pair and de-dups
// against any existing/settled row.
func (j *SweepJob) sweepOne(ctx context.Context, s page.SweepSubject) error {
	nameQuery := sweepNameQuery(s)
	claimQuery := strings.TrimSpace(s.Body)
	cands, err := j.store.Candidates(ctx, s.Type, nameQuery, claimQuery, j.flagLimit)
	if err != nil {
		return fmt.Errorf("candidates: %w", err)
	}
	for _, c := range cands {
		if c.SubjectID == s.SubjectID {
			continue // self-hit (the subject's own page matches its own name/body)
		}
		if err := j.store.FlagDupAuto(ctx, s.SubjectID, c.SubjectID); err != nil {
			return fmt.Errorf("flag (%s,%s): %w", s.SubjectID, c.SubjectID, err)
		}
	}
	return nil
}

// sweepNameQuery builds the name/alias FTS query text from an existing subject's
// surface forms (canonical name + normalized alias keys) — the registry analogue of
// the document pass's name query (design §4.3 lane 1).
func sweepNameQuery(s page.SweepSubject) string {
	parts := make([]string, 0, 1+len(s.Aliases))
	if n := strings.TrimSpace(s.CanonicalName); n != "" {
		parts = append(parts, n)
	}
	for _, a := range s.Aliases {
		if t := strings.TrimSpace(a); t != "" {
			parts = append(parts, t)
		}
	}
	return strings.Join(parts, " ")
}
