package wiki_test

import (
	"context"
	"database/sql"
	"errors"
	"strings"
	"sync"
	"testing"
	"time"

	"wiki/internal/page"
	wikidomain "wiki/internal/wiki"
	"wiki/internal/worker"
)

func TestMergeSubjectWorkItemRejectsSelfMergeBeforeQueueing(t *testing.T) {
	// R-NEFH-U8IO
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()

	svc := wikidomain.NewService(conn, nil, &scriptedMergeCompiler{}, mergeClockAt(time.Date(2026, 6, 24, 0, 31, 0, 0, time.UTC)))
	if _, err := svc.MergeSubjects(ctx, "subject-same", "subject-same"); err == nil {
		t.Fatal("MergeSubjects self-merge returned nil error, want CHECK constraint failure")
	}
	assertTableCount(t, ctx, conn, "jobs", 0)
}

func TestMergeWorkerDispatchesMergeJobWithoutExtractor(t *testing.T) {
	// R-NFNE-809D
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()
	saveMergeFixture(t, ctx, conn)

	compiler := &scriptedMergeCompiler{}
	svc := wikidomain.NewService(conn, nil, compiler, mergeClockAt(time.Date(2026, 6, 24, 0, 32, 0, 0, time.UTC)))
	jobID, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("MergeSubjects: %v", err)
	}

	stop := startWorker(t, ctx, svc)
	defer stop()
	waitJobStatus(t, ctx, svc, jobID, wikidomain.JobDone)

	if got := compiler.CallCount(); got != 1 {
		t.Fatalf("compiler calls = %d, want merge dispatch to compile once without using extractor", got)
	}
}

func TestMergeWorkerFoldsLoserIntoWinnerAndCompilesCombinedClaims(t *testing.T) {
	// R-NGVA-LS02
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()
	saveMergeFixture(t, ctx, conn)

	compiler := &scriptedMergeCompiler{}
	svc := wikidomain.NewService(conn, nil, compiler, mergeClockAt(time.Date(2026, 6, 24, 0, 33, 0, 0, time.UTC)))
	jobID, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("MergeSubjects: %v", err)
	}

	stop := startWorker(t, ctx, svc)
	defer stop()
	waitJobStatus(t, ctx, svc, jobID, wikidomain.JobDone)

	if _, err := wikidomain.NewSubjectStore(conn).Get(ctx, "subject-loser"); !errors.Is(err, sql.ErrNoRows) {
		t.Fatalf("loser subject lookup error = %v, want sql.ErrNoRows", err)
	}
	claims, _, err := wikidomain.NewClaimStore(conn).ListBySubject(ctx, "subject-winner", page.Params{})
	if err != nil {
		t.Fatalf("ListBySubject winner: %v", err)
	}
	if got := claimBodies(claims); got != "Loser fact.\nWinner fact." {
		t.Fatalf("winner claim bodies = %q, want loser and winner claims folded", got)
	}
	page, err := wikidomain.NewPageStore(conn).GetBySubject(ctx, "subject-winner")
	if err != nil {
		t.Fatalf("GetBySubject winner: %v", err)
	}
	if page.Title != "Winner Subject" || page.Body != "Loser fact.\nWinner fact." {
		t.Fatalf("winner page = %+v, want compiled combined page", page)
	}

	call := compiler.Calls()[0]
	if call.Subject.ID != "subject-winner" || claimBodies(call.Claims) != "Loser fact.\nWinner fact." {
		t.Fatalf("compile call = %+v, want winner subject with combined claims", call)
	}
}

func TestMergeWorkerRollsBackWhenCompilerFails(t *testing.T) {
	// R-NI36-ZJQR
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()
	saveMergeFixture(t, ctx, conn)

	compiler := &scriptedMergeCompiler{err: errors.New("compile exploded")}
	svc := wikidomain.NewService(conn, nil, compiler, mergeClockAt(time.Date(2026, 6, 24, 0, 34, 0, 0, time.UTC)))
	jobID, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("MergeSubjects: %v", err)
	}

	stop := startWorker(t, ctx, svc)
	defer stop()
	status := waitJobStatus(t, ctx, svc, jobID, wikidomain.JobFailed)
	if !strings.Contains(status.Error, "compile exploded") {
		t.Fatalf("status error = %q, want compiler error", status.Error)
	}
	assertMergeFixtureStillSeparate(t, ctx, conn)
}

func TestMergeWorkerRepointsClaimsBeforeDeletingLoserSubject(t *testing.T) {
	// R-NJB3-DBHG
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()
	saveMergeFixture(t, ctx, conn)

	if err := wikidomain.NewSubjectStore(conn).Delete(ctx, "subject-loser"); err == nil {
		t.Fatal("Delete loser with claims returned nil, want delete-order constraint before repoint")
	}

	svc := wikidomain.NewService(conn, nil, &scriptedMergeCompiler{}, mergeClockAt(time.Date(2026, 6, 24, 0, 35, 0, 0, time.UTC)))
	jobID, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("MergeSubjects: %v", err)
	}
	stop := startWorker(t, ctx, svc)
	defer stop()
	waitJobStatus(t, ctx, svc, jobID, wikidomain.JobDone)

	loserClaims, _, err := wikidomain.NewClaimStore(conn).ListBySubject(ctx, "subject-loser", page.Params{})
	if err != nil {
		t.Fatalf("ListBySubject loser: %v", err)
	}
	if len(loserClaims) != 0 {
		t.Fatalf("loser claims = %+v, want all claims repointed before delete", loserClaims)
	}
}

func TestMergeWorkerAbortDuringCompilePreservesAbortedStatusAndRows(t *testing.T) {
	// R-NKIZ-R385
	ctx := context.Background()
	conns, cleanup := migratedConns(t, ctx)
	defer cleanup()
	saveMergeFixture(t, ctx, conns.Write)

	compiler := newBlockingCompiler()
	svc := wikidomain.NewService(conns, nil, compiler, mergeClockAt(time.Date(2026, 6, 24, 0, 36, 0, 0, time.UTC)))
	jobID, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("MergeSubjects: %v", err)
	}

	type processResult struct {
		processed bool
		err       error
	}
	processed := make(chan processResult, 1)
	go func() {
		ok, err := svc.ProcessNext(ctx)
		processed <- processResult{processed: ok, err: err}
	}()
	<-compiler.entered

	result, err := svc.Abort(ctx, jobID)
	if err != nil {
		t.Fatalf("Abort: %v", err)
	}
	if !result.Aborted || result.Status != wikidomain.JobAborted {
		t.Fatalf("Abort result = %+v, want aborted", result)
	}
	<-compiler.canceled
	select {
	case got := <-processed:
		if got.err != nil || !got.processed {
			t.Fatalf("ProcessNext = %v/%v, want true/nil", got.processed, got.err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("ProcessNext did not return after abort")
	}

	status := waitJobStatus(t, ctx, svc, jobID, wikidomain.JobAborted)
	if status.Error != "" {
		t.Fatalf("aborted status error = %q, want empty", status.Error)
	}
	assertMergeFixtureStillSeparate(t, ctx, conns.Read)
}

func TestMergeWorkerRequeuesWorkingMergeOnBootAndCompletesIt(t *testing.T) {
	// R-NLQW-4UYU
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()
	saveMergeFixture(t, ctx, conn)

	compiler := &scriptedMergeCompiler{}
	svc := wikidomain.NewService(conn, nil, compiler, mergeSequenceTimes(
		time.Date(2026, 6, 24, 0, 37, 0, 0, time.UTC),
		time.Date(2026, 6, 24, 0, 37, 1, 0, time.UTC),
		time.Date(2026, 6, 24, 0, 37, 2, 0, time.UTC),
	))
	jobID, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("MergeSubjects: %v", err)
	}
	if _, ok, err := wikidomain.NewJobStore(conn).ClaimPending(ctx, time.Date(2026, 6, 24, 0, 37, 30, 0, time.UTC)); err != nil || !ok {
		t.Fatalf("ClaimPending = %v/%v, want working orphan", ok, err)
	}

	stop := startWorker(t, ctx, svc)
	defer stop()
	waitJobStatus(t, ctx, svc, jobID, wikidomain.JobDone)
	if got := compiler.CallCount(); got != 1 {
		t.Fatalf("compiler calls = %d, want requeued merge processed once", got)
	}
}

func TestMergeWorkerTreatsStalePairAsDoneNoop(t *testing.T) {
	// R-NMYS-IMPJ
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()
	if err := wikidomain.NewSubjectStore(conn).Save(ctx, wikidomain.Subject{
		ID: "subject-winner", Name: "Winner Subject", Type: "entity",
	}); err != nil {
		t.Fatalf("Save winner: %v", err)
	}

	compiler := &scriptedMergeCompiler{}
	svc := wikidomain.NewService(conn, nil, compiler, mergeClockAt(time.Date(2026, 6, 24, 0, 38, 0, 0, time.UTC)))
	jobID, err := svc.MergeSubjects(ctx, "subject-missing", "subject-winner")
	if err != nil {
		t.Fatalf("MergeSubjects: %v", err)
	}

	stop := startWorker(t, ctx, svc)
	defer stop()
	waitJobStatus(t, ctx, svc, jobID, wikidomain.JobDone)
	if got := compiler.CallCount(); got != 0 {
		t.Fatalf("compiler calls = %d, want stale merge no-op", got)
	}
	if _, err := wikidomain.NewSubjectStore(conn).Get(ctx, "subject-winner"); err != nil {
		t.Fatalf("winner lookup after stale merge: %v", err)
	}
}

func TestMergeWorkerSerializesDuplicateMergeJobsIntoOneFold(t *testing.T) {
	// R-NPEL-A66X
	ctx := context.Background()
	conn := migratedWikiDB(t, ctx)
	defer conn.Close()
	saveMergeFixture(t, ctx, conn)

	compiler := &scriptedMergeCompiler{}
	svc := wikidomain.NewService(conn, nil, compiler, mergeSequenceTimes(
		time.Date(2026, 6, 24, 0, 39, 0, 0, time.UTC),
		time.Date(2026, 6, 24, 0, 39, 1, 0, time.UTC),
		time.Date(2026, 6, 24, 0, 39, 2, 0, time.UTC),
		time.Date(2026, 6, 24, 0, 39, 3, 0, time.UTC),
	))
	firstJob, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("first MergeSubjects: %v", err)
	}
	secondJob, err := svc.MergeSubjects(ctx, "subject-loser", "subject-winner")
	if err != nil {
		t.Fatalf("second MergeSubjects: %v", err)
	}

	runCtx, cancel := context.WithCancel(ctx)
	done := make(chan error, 2)
	go func() { done <- worker.Run(runCtx, svc) }()
	go func() { done <- worker.Run(runCtx, svc) }()
	waitJobStatus(t, ctx, svc, firstJob, wikidomain.JobDone)
	waitJobStatus(t, ctx, svc, secondJob, wikidomain.JobDone)
	cancel()
	for i := 0; i < 2; i++ {
		select {
		case err := <-done:
			if err != nil {
				t.Fatalf("worker.Run returned error: %v", err)
			}
		case <-time.After(2 * time.Second):
			t.Fatal("worker.Run did not stop after context cancellation")
		}
	}

	if got := compiler.CallCount(); got != 1 {
		t.Fatalf("compiler calls = %d, want duplicate serialized merge to fold once", got)
	}
	claims, _, err := wikidomain.NewClaimStore(conn).ListBySubject(ctx, "subject-winner", page.Params{})
	if err != nil {
		t.Fatalf("ListBySubject winner: %v", err)
	}
	if len(claims) != 2 {
		t.Fatalf("winner claims = %+v, want exactly the two original claims", claims)
	}
}

type mergeCompileCall struct {
	Subject wikidomain.Subject
	Claims  []wikidomain.Claim
}

type scriptedMergeCompiler struct {
	mu    sync.Mutex
	err   error
	calls []mergeCompileCall
}

func (c *scriptedMergeCompiler) Compile(_ context.Context, subject wikidomain.Subject, claims []wikidomain.Claim) (string, string, error) {
	c.mu.Lock()
	c.calls = append(c.calls, mergeCompileCall{
		Subject: subject,
		Claims:  append([]wikidomain.Claim(nil), claims...),
	})
	c.mu.Unlock()
	if c.err != nil {
		return "", "", c.err
	}
	return subject.Name, claimBodies(claims), nil
}

func (c *scriptedMergeCompiler) Calls() []mergeCompileCall {
	c.mu.Lock()
	defer c.mu.Unlock()
	return append([]mergeCompileCall(nil), c.calls...)
}

func (c *scriptedMergeCompiler) CallCount() int {
	c.mu.Lock()
	defer c.mu.Unlock()
	return len(c.calls)
}

func saveMergeFixture(t *testing.T, ctx context.Context, conn *sql.DB) {
	t.Helper()
	subjects := wikidomain.NewSubjectStore(conn)
	for _, subject := range []wikidomain.Subject{
		{ID: "subject-winner", Name: "Winner Subject", Type: "entity"},
		{ID: "subject-loser", Name: "Loser Subject", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save subject %s: %v", subject.ID, err)
		}
	}
	claims := wikidomain.NewClaimStore(conn)
	for _, claim := range []wikidomain.Claim{
		{ID: "claim-winner", SubjectID: "subject-winner", JobID: "job-existing", Body: "Winner fact."},
		{ID: "claim-loser", SubjectID: "subject-loser", JobID: "job-existing", Body: "Loser fact."},
	} {
		if err := claims.Save(ctx, claim); err != nil {
			t.Fatalf("Save claim %s: %v", claim.ID, err)
		}
	}
	pages := wikidomain.NewPageStore(conn)
	for _, page := range []wikidomain.Page{
		{ID: "subject-winner", SubjectID: "subject-winner", Title: "Winner Old", Body: "old winner body"},
		{ID: "subject-loser", SubjectID: "subject-loser", Title: "Loser Old", Body: "old loser body"},
	} {
		if err := pages.Upsert(ctx, page); err != nil {
			t.Fatalf("Upsert page %s: %v", page.ID, err)
		}
	}
}

func assertMergeFixtureStillSeparate(t *testing.T, ctx context.Context, conn *sql.DB) {
	t.Helper()
	if _, err := wikidomain.NewSubjectStore(conn).Get(ctx, "subject-loser"); err != nil {
		t.Fatalf("loser subject lookup: %v", err)
	}
	loserClaims, _, err := wikidomain.NewClaimStore(conn).ListBySubject(ctx, "subject-loser", page.Params{})
	if err != nil {
		t.Fatalf("ListBySubject loser: %v", err)
	}
	if got := claimBodies(loserClaims); got != "Loser fact." {
		t.Fatalf("loser claim bodies = %q, want original loser claim", got)
	}
	page, err := wikidomain.NewPageStore(conn).GetBySubject(ctx, "subject-loser")
	if err != nil {
		t.Fatalf("loser page lookup: %v", err)
	}
	if page.Body != "old loser body" {
		t.Fatalf("loser page body = %q, want old loser body", page.Body)
	}
}

func claimBodies(claims []wikidomain.Claim) string {
	var bodies []string
	for _, claim := range claims {
		bodies = append(bodies, claim.Body)
	}
	return strings.Join(bodies, "\n")
}

func mergeClockAt(t time.Time) func() time.Time {
	return func() time.Time { return t }
}

func mergeSequenceTimes(times ...time.Time) func() time.Time {
	var mu sync.Mutex
	i := 0
	return func() time.Time {
		mu.Lock()
		defer mu.Unlock()
		if i >= len(times) {
			return times[len(times)-1]
		}
		t := times[i]
		i++
		return t
	}
}
