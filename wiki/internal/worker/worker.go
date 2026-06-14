// Package worker is wiki's dispatcher-free integration pool (design §3). There is
// NO central dispatcher: WIKI_INTEGRATION_WORKERS identical goroutines each loop
// over the SELECTION critical section under one in-flight-set mutex, claim a unit
// of work, run the matching integrator with NO lock held, commit, drop the claim,
// and loop. The mutex-guarded selection IS the lone dispatcher re-expressed as a
// lock — chosen on legibility.
//
// Selection order (design §3): (1) a pending cron row whose bound job name(s)
// aren't in flight → TryLock the job name; (2) else the oldest pending DOCUMENT
// row not in flight → TryLock the row id; (3) else block on the wake Cond. CRON
// BEFORE DOCUMENTS. Event rows are invisible to selection (they wait to be swept
// by a digest's selector).
//
// The nudge is an OPTIMIZATION, not the truth: it is contentless, and every wake
// (and boot) re-runs selection against the inbox, so a missed nudge loses nothing.
// Crash-resume rides one rule — stamp only at commit, never at claim (design §3):
// the in-flight set is RAM-only, wiped on crash, so a crash can only leave a row
// pending and restart re-selects it.
package worker

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"log/slog"
	"sync"
	"time"

	"wiki/internal/integrate"
	"wiki/internal/run"
)

// Runs is the runs-lifecycle surface the pool depends on (run.Store's Begin /
// Commit / Fail / StampCron / SweepOrphans), narrowed to an interface so the pool
// is unit-testable without a real DB-backed Store.
type Runs interface {
	Begin(ctx context.Context, job, causedBy string) (string, error)
	Commit(ctx context.Context, runID, causedBy string, m *integrate.Manifest, stampInbox bool) error
	Fail(ctx context.Context, runID, causedBy string, runErr error) error
	StampCron(ctx context.Context, runID, cronRowID string, boundJobs []string) (bool, error)
	SweepOrphans(ctx context.Context) (int64, error)
	// CountConflict bumps runs.conflicts when the conflict loop handles an
	// optimistic-commit collision (design §3, P7b).
	CountConflict(ctx context.Context, runID string) error
}

// ReMerger is the optional capability an integrator implements to support the
// optimistic-commit conflict loop (design §3, P7b): re-run the MERGE STAGE ONLY for
// the conflicting subject against the fresh page version, never re-extracting or
// re-resolving. The document pass (*integrate.Document) satisfies it; an integrator
// that does not (e.g. a stub or a no-op cron run) makes a commit conflict a clean,
// non-retryable failure — which is correct, since such a run has no merge to re-run.
type ReMerger interface {
	ReMerge(ctx context.Context, m *integrate.Manifest, subjectID string) error
}

// ReResolver is the optional capability an integrator implements to support the
// DUPLICATE-MINT conflict arm (design §3, P7b2): re-run the RESOLVE stage (and the
// match+merge that follow) for the colliding subject only, against the winner's
// freshly-committed aliases — never re-extracting. The document pass
// (*integrate.Document) satisfies it; an integrator that does not makes a
// duplicate-mint conflict a clean, non-retryable failure (the same stance as a
// non-ReMerger facing a lost update — such a run has no resolve stage to re-run).
type ReResolver interface {
	ReResolve(ctx context.Context, m *integrate.Manifest, subjectID string) error
}

// Pool is the worker pool. It owns the in-flight set + its mutex/Cond, the runs
// lifecycle, and the registered integrators. Constructed once at the composition
// root; Run launches the goroutines on a context and returns when they drain.
type Pool struct {
	db   *sql.DB
	runs Runs
	log  *slog.Logger
	now  func() time.Time

	workers int

	// document is the document-pass integrator (keyed by inbox row id). cron maps
	// a cron job name to its integrator; cronBindings maps a cron schedule name to
	// the job names a cron row of that schedule binds (the completion-time join).
	document integrate.Integrator
	cron     map[string]integrate.Integrator
	bindings map[string][]string // cron schedule name → bound job names

	mu       sync.Mutex
	cond     *sync.Cond
	inflight map[string]struct{} // claimed work keys (RAM-only; wiped on crash)
	shutdown bool
}

// Options configures a Pool. DB, Runs, and Document are required; Workers
// defaults to 4 (WIKI_INTEGRATION_WORKERS). Cron/Bindings may be empty (no digest
// yet — P4 ships only stubs).
type Options struct {
	DB       *sql.DB
	Runs     Runs
	Logger   *slog.Logger
	Workers  int
	Document integrate.Integrator
	Cron     map[string]integrate.Integrator
	Bindings map[string][]string
	// Now is the clock used for the pending predicate and the backoff timer wake
	// source (design §7). Defaults to time.Now; injectable for time-driven tests.
	Now func() time.Time
}

// New validates options and returns a ready Pool with its wake Cond initialized.
func New(opts Options) (*Pool, error) {
	if opts.DB == nil {
		return nil, fmt.Errorf("worker: DB is required")
	}
	if opts.Runs == nil {
		return nil, fmt.Errorf("worker: Runs is required")
	}
	if opts.Document == nil {
		return nil, fmt.Errorf("worker: Document integrator is required")
	}
	w := opts.Workers
	if w <= 0 {
		w = 4
	}
	p := &Pool{
		db:       opts.DB,
		runs:     opts.Runs,
		log:      opts.Logger,
		now:      opts.Now,
		workers:  w,
		document: opts.Document,
		cron:     opts.Cron,
		bindings: opts.Bindings,
		inflight: make(map[string]struct{}),
	}
	if p.now == nil {
		p.now = time.Now
	}
	if p.cron == nil {
		p.cron = map[string]integrate.Integrator{}
	}
	if p.bindings == nil {
		p.bindings = map[string][]string{}
	}
	p.cond = sync.NewCond(&p.mu)
	return p, nil
}

// Nudge is the contentless wake doorbell (design §3): a front door / consumer
// calls it after Accept so a worker re-runs selection. A missed nudge loses
// nothing (every wake re-scans). Safe to wire as inbox.Store's Nudge.
func (p *Pool) Nudge() {
	p.mu.Lock()
	p.cond.Broadcast()
	p.mu.Unlock()
}

// nowMillis is the pool clock in epoch ms — the basis of the pending predicate's
// ineligible_until comparison and the backoff timer.
func (p *Pool) nowMillis() int64 { return p.now().UTC().UnixMilli() }

// timerWake is the fourth wake source (design §7): a backed-off row's
// ineligible_until is in the future, so no nudge will fire when it becomes
// eligible. This goroutine arms a timer to the EARLIEST future ineligible_until
// among pending rows and broadcasts the wake when it arrives, so selection
// re-scans exactly when a row's backoff expires. It re-polls on every wake (the
// timer is an optimization, not the truth — like the nudge), so a newly-failed
// row's nearer deadline is always picked up.
func (p *Pool) timerWake(ctx context.Context) {
	for {
		next, ok := p.earliestBackoff(ctx)
		var timer *time.Timer
		var c <-chan time.Time
		if ok {
			d := time.Until(next)
			if d < 0 {
				d = 0
			}
			timer = time.NewTimer(d)
			c = timer.C
		}
		// With no future deadline, c is nil and we only wake on a re-poll tick so a
		// freshly-armed backoff (set after this pass) is eventually noticed.
		var poll <-chan time.Time
		if !ok {
			t := time.NewTimer(time.Minute)
			poll = t.C
			defer t.Stop()
		}
		select {
		case <-ctx.Done():
			if timer != nil {
				timer.Stop()
			}
			return
		case <-c:
			p.mu.Lock()
			p.cond.Broadcast()
			p.mu.Unlock()
		case <-poll:
		}
		if timer != nil {
			timer.Stop()
		}
	}
}

// earliestBackoff returns the soonest future ineligible_until among pending,
// not-dead rows (the rows the backoff timer must wake for). ok is false when no
// such row exists.
func (p *Pool) earliestBackoff(ctx context.Context) (time.Time, bool) {
	now := p.nowMillis()
	var ms sql.NullInt64
	err := p.db.QueryRowContext(ctx,
		`SELECT MIN(ineligible_until) FROM inbox
		  WHERE integrated_by = '' AND dead_at IS NULL
		    AND ineligible_until IS NOT NULL AND ineligible_until > ?`, now,
	).Scan(&ms)
	if err != nil || !ms.Valid {
		return time.Time{}, false
	}
	return time.UnixMilli(ms.Int64).UTC(), true
}

// Run launches the worker goroutines and blocks until ctx is cancelled and they
// drain. It first runs the boot sweep (orphaned `running` → `crashed`) and an
// initial selection pass (boot re-runs selection against the inbox — the nudge is
// not the truth). On ctx cancellation it broadcasts the wake so blocked workers
// exit, then waits for them.
func (p *Pool) Run(ctx context.Context) error {
	if n, err := p.runs.SweepOrphans(ctx); err != nil {
		return fmt.Errorf("worker: boot sweep: %w", err)
	} else if n > 0 && p.log != nil {
		p.log.Info("worker boot sweep", slog.Int64("crashed", n))
	}

	// A shutdown watcher flips the shutdown flag and wakes every blocked worker so
	// they observe cancellation while parked on the Cond.
	go func() {
		<-ctx.Done()
		p.mu.Lock()
		p.shutdown = true
		p.cond.Broadcast()
		p.mu.Unlock()
	}()

	// The backoff timer wake source (design §7): wakes selection when the earliest
	// future ineligible_until arrives, so a backed-off row is re-selected on time.
	go p.timerWake(ctx)

	var wg sync.WaitGroup
	wg.Add(p.workers)
	for i := 0; i < p.workers; i++ {
		go func() {
			defer wg.Done()
			p.loop(ctx)
		}()
	}
	wg.Wait()
	return nil
}

// loop is one worker's body: claim → run → commit → drop the claim → loop, parking
// on the wake Cond when selection finds nothing.
func (p *Pool) loop(ctx context.Context) {
	for {
		claim, ok := p.selectWork(ctx)
		if !ok {
			return // shutdown
		}
		p.runClaim(ctx, claim)
		p.drop(claim.key)
	}
}

// claim is one selected unit of work: the integrator to run, the unit it runs
// over, the in-flight key to drop afterward, and — for a cron entry — the cron
// row id + bound jobs for the completion-time stamp.
type claim struct {
	integ      integrate.Integrator
	unit       integrate.Unit
	key        string
	isCron     bool
	cronRowID  string
	boundJobs  []string
}

// selectWork is the SELECTION critical section (design §3), run under the
// in-flight mutex: cron before documents, oldest-first, skipping work already in
// flight. It blocks on the wake Cond when nothing is selectable, returning false
// only on shutdown. The claim's in-flight key is inserted BEFORE the lock is
// released, so two workers never grab the same unit.
func (p *Pool) selectWork(ctx context.Context) (claim, bool) {
	p.mu.Lock()
	defer p.mu.Unlock()
	for {
		if p.shutdown || ctx.Err() != nil {
			return claim{}, false
		}
		// (1) a pending cron row whose bound job name(s) aren't in flight.
		if c, ok := p.selectCron(ctx); ok {
			p.inflight[c.key] = struct{}{}
			return c, true
		}
		// (2) else the oldest pending document row not in flight.
		if c, ok := p.selectDocument(ctx); ok {
			p.inflight[c.key] = struct{}{}
			return c, true
		}
		// (3) else block on the wake Cond.
		p.cond.Wait()
	}
}

// selectCron finds the oldest pending cron row (kind=event, source LIKE 'cron:%')
// whose TryLock job-name key is free. The work key is the job name (digests are
// chosen by a shared selector, so two concurrent runs of one entry would read the
// same pending rows — design §3). Under the locked Framing 2 (P1) a cron row
// expands into (cron-row, entry) candidates; a cron row no job binds is stamped
// immediately as a no-op here.
//
// Called with p.mu held.
func (p *Pool) selectCron(ctx context.Context) (claim, bool) {
	now := p.nowMillis()
	rows, err := p.db.QueryContext(ctx,
		`SELECT id, source FROM inbox
		  WHERE kind = 'event' AND source LIKE 'cron:%'
		    AND integrated_by = '' AND dead_at IS NULL
		    AND (ineligible_until IS NULL OR ineligible_until <= ?)
		  ORDER BY id ASC`, now)
	if err != nil {
		if p.log != nil {
			p.log.Error("worker: cron scan", slog.String("err", err.Error()))
		}
		return claim{}, false
	}
	defer rows.Close()

	type cronRow struct{ id, schedule string }
	var pending []cronRow
	for rows.Next() {
		var id, source string
		if err := rows.Scan(&id, &source); err != nil {
			continue
		}
		schedule := source[len("cron:"):]
		pending = append(pending, cronRow{id: id, schedule: schedule})
	}

	for _, cr := range pending {
		jobs := p.bindings[cr.schedule]
		if len(jobs) == 0 {
			// No job binds this schedule → stamp immediately as a no-op (design §3).
			// Use the cron row id as the work key so two workers don't both stamp it.
			key := "cron-noop:" + cr.id
			if _, busy := p.inflight[key]; busy {
				continue
			}
			return claim{
				key:       key,
				isCron:    true,
				cronRowID: cr.id,
				boundJobs: nil, // StampCron treats empty bound set as immediate no-op
			}, true
		}
		// Framing 2 (P1): expand into (cron-row, entry) candidates; claim the first
		// bound entry whose job-name key is free.
		for _, job := range jobs {
			integ := p.cron[job]
			if integ == nil {
				continue // no integrator registered for this bound job
			}
			key := "cron:" + cr.id + ":" + job
			if _, busy := p.inflight[key]; busy {
				continue
			}
			return claim{
				integ:     integ,
				unit:      integrate.Unit{CausedBy: cr.id, Entry: job},
				key:       key,
				isCron:    true,
				cronRowID: cr.id,
				boundJobs: jobs,
			}, true
		}
	}
	return claim{}, false
}

// selectDocument finds the oldest pending document row not in flight. The work key
// is the inbox row id (so the pool isn't serialized — many document passes run at
// once; the guard only stops two workers grabbing the same row — design §3).
//
// Called with p.mu held.
func (p *Pool) selectDocument(ctx context.Context) (claim, bool) {
	now := p.nowMillis()
	rows, err := p.db.QueryContext(ctx,
		`SELECT id FROM inbox
		  WHERE kind = 'document'
		    AND integrated_by = '' AND dead_at IS NULL
		    AND (ineligible_until IS NULL OR ineligible_until <= ?)
		  ORDER BY id ASC`, now)
	if err != nil {
		if p.log != nil {
			p.log.Error("worker: document scan", slog.String("err", err.Error()))
		}
		return claim{}, false
	}
	defer rows.Close()

	for rows.Next() {
		var id string
		if err := rows.Scan(&id); err != nil {
			continue
		}
		if _, busy := p.inflight[id]; busy {
			continue // already claimed by another worker
		}
		return claim{
			integ: p.document,
			unit:  integrate.Unit{CausedBy: id},
			key:   id,
		}, true
	}
	return claim{}, false
}

// runClaim runs one claimed unit with NO lock held (design §3): insert the
// `running` run row, run the integrator, then either commit (succeeded + stamp)
// or fail cleanly (the causing row stays pending). For a cron entry the inbox
// stamp is deferred to the worker-local completion-time join (StampCron).
func (p *Pool) runClaim(ctx context.Context, c claim) {
	// A no-job cron row is a pure no-op stamp: no run, just the completion-time
	// stamp (design §3 "stamped immediately as a no-op").
	if c.isCron && c.integ == nil {
		if _, err := p.runs.StampCron(ctx, "", c.cronRowID, nil); err != nil && p.log != nil {
			p.log.Error("worker: cron no-op stamp", slog.String("err", err.Error()))
		}
		return
	}

	job := c.integ.Job()
	runID, err := p.runs.Begin(ctx, job, c.unit.CausedBy)
	if err != nil {
		if p.log != nil {
			p.log.Error("worker: begin run", slog.String("job", job), slog.String("err", err.Error()))
		}
		return
	}

	m, err := c.integ.Integrate(ctx, c.unit)
	if err != nil {
		if ferr := p.runs.Fail(ctx, runID, c.unit.CausedBy, err); ferr != nil && p.log != nil {
			p.log.Error("worker: mark failed", slog.String("err", ferr.Error()))
		}
		if p.log != nil {
			p.log.Warn("worker: integrate failed", slog.String("job", job),
				slog.String("caused_by", c.unit.CausedBy), slog.String("err", err.Error()))
		}
		return
	}

	// Document/event rows are stamped inside the commit; cron entries defer the
	// inbox stamp to StampCron (the commit still writes the terminal `succeeded`).
	stampInbox := !c.isCron
	if err := p.commitWithConflictLoop(ctx, runID, c, m, stampInbox); err != nil {
		// The conflict loop already marked the run failed (clean) on exhaustion or a
		// non-retryable commit error; just log. A successful commit returns nil.
		if p.log != nil {
			p.log.Error("worker: commit", slog.String("job", job), slog.String("err", err.Error()))
		}
		return
	}

	if c.isCron {
		if _, err := p.runs.StampCron(ctx, runID, c.cronRowID, c.boundJobs); err != nil && p.log != nil {
			p.log.Error("worker: cron completion stamp", slog.String("err", err.Error()))
		}
	}
}

// commitWithConflictLoop runs the end-of-run commit under the optimistic-commit
// conflict loop (design §3, P7b). On the happy path it is one Commit call. On a
// *run.ConflictError (the lost-update race: a concurrent run advanced a page past
// the base version merge read) it: counts the conflict on the run, re-runs MERGE
// ONLY for the conflicting subject against the fresh page version (re-reading the
// base version), and recommits — bounded to run.MaxCommitAttempts. On exhaustion it
// fails the run cleanly naming conflict-retry exhaustion (the row stays pending; its
// failure policy applies — §7). The in-run retries are UNSPACED (each re-merge is a
// minutes-long call, already jittered by natural latency — §3); post-exhaustion
// re-selection delay is the failure policy's ineligible_until, applied by Fail.
//
// An integrator that cannot re-merge (no ReMerger — a stub/no-op run) makes a
// conflict a clean, non-retryable failure: such a run has no merge to re-run, so
// retrying would loop forever on the same stale manifest.
func (p *Pool) commitWithConflictLoop(ctx context.Context, runID string, c claim, m *integrate.Manifest, stampInbox bool) error {
	remerger, canReMerge := c.integ.(ReMerger)
	reresolver, canReResolve := c.integ.(ReResolver)

	for attempt := 1; ; attempt++ {
		err := p.runs.Commit(ctx, runID, c.unit.CausedBy, m, stampInbox)
		if err == nil {
			return nil // committed
		}

		// Classify the failure into one of the two conflict arms (§3) or a real error.
		var lostUpdate *run.ConflictError
		var dupMint *run.DuplicateMintError
		isLostUpdate := errors.As(err, &lostUpdate)
		isDupMint := errors.As(err, &dupMint)
		if !isLostUpdate && !isDupMint {
			// A real (non-conflict) commit error: fail the run cleanly so the row's
			// policy applies, and surface it.
			if ferr := p.runs.Fail(ctx, runID, c.unit.CausedBy, err); ferr != nil && p.log != nil {
				p.log.Error("worker: mark failed after commit error", slog.String("err", ferr.Error()))
			}
			return err
		}

		// An optimistic-commit conflict (lost update or duplicate mint). Record it on
		// the run — the shared collision counter is the same for both arms (§3).
		if cerr := p.runs.CountConflict(ctx, runID); cerr != nil && p.log != nil {
			p.log.Error("worker: count conflict", slog.String("err", cerr.Error()))
		}

		// Whether this conflict type is recoverable by THIS integrator: a lost update
		// needs ReMerge, a duplicate mint needs ReResolve.
		recoverable := (isLostUpdate && canReMerge) || (isDupMint && canReResolve)

		// Bound: cap MaxCommitAttempts (shared by BOTH arms — §3), then fail naming
		// conflict-retry exhaustion.
		if attempt >= run.MaxCommitAttempts || !recoverable {
			failErr := run.ErrConflictRetryExhausted
			if !recoverable {
				failErr = fmt.Errorf("%w (integrator cannot recover this conflict type)", err)
			}
			if ferr := p.runs.Fail(ctx, runID, c.unit.CausedBy, failErr); ferr != nil && p.log != nil {
				p.log.Error("worker: mark failed after conflict exhaustion", slog.String("err", ferr.Error()))
			}
			return failErr
		}

		if isLostUpdate {
			// Re-run merge ONLY for the conflicting subject (re-reads the fresh base
			// version into the manifest; never re-extracts or re-resolves — §3). The
			// re-merge re-derives PageBody/Title/Superseded, so the §6.1 gate on the
			// next attempt runs against the re-run's FRESH superseded, not the stale one.
			if rmErr := remerger.ReMerge(ctx, m, lostUpdate.Subject); rmErr != nil {
				if ferr := p.runs.Fail(ctx, runID, c.unit.CausedBy, rmErr); ferr != nil && p.log != nil {
					p.log.Error("worker: mark failed after re-merge error", slog.String("err", ferr.Error()))
				}
				return rmErr
			}
			continue
		}

		// A duplicate-mint conflict: RESTART AT RESOLVE for the colliding subject only
		// (design §3, P7b2). The lookup now hits the winner's freshly-committed aliases,
		// so this run's subject resolves ONTO the winner instead of re-minting a
		// duplicate. Extract is never re-entered. Re-resolve re-folds the manifest, so
		// the next commit's version guard and §6.1 gate run against the right page.
		if rrErr := reresolver.ReResolve(ctx, m, dupMint.Subject); rrErr != nil {
			if ferr := p.runs.Fail(ctx, runID, c.unit.CausedBy, rrErr); ferr != nil && p.log != nil {
				p.log.Error("worker: mark failed after re-resolve error", slog.String("err", ferr.Error()))
			}
			return rrErr
		}
	}
}

// drop removes a finished claim's key from the in-flight set and wakes a worker —
// run completion is a wake source (design §3).
func (p *Pool) drop(key string) {
	p.mu.Lock()
	delete(p.inflight, key)
	p.cond.Broadcast()
	p.mu.Unlock()
}
