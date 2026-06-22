package wiki

import (
	"context"
	"database/sql"
	"errors"
	"strings"
	"testing"
	"time"

	"wiki/internal/extract"
)

func TestIngestReturnsJobIDFromPendingInsertWithoutExtraction(t *testing.T) {
	// R-M8RN-87WV
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	fixed := time.Date(2026, 6, 20, 20, 30, 0, 0, time.UTC)
	extractor := &recordingExtractor{}
	svc := NewService(conn, extractor, &recordingCompiler{}, func() time.Time { return fixed })
	svc.newID = sequenceIDs("job-1")

	jobID, err := svc.Ingest(ctx, " owner@example.com ", "Acme Robotics opened a lab.", " Lab notes ", []string{"robotics"})
	if err != nil {
		t.Fatalf("Ingest returned error: %v", err)
	}
	if jobID != "job-1" {
		t.Fatalf("jobID = %q, want job-1", jobID)
	}
	if extractor.calls != 0 {
		t.Fatalf("extractor calls = %d, want 0 on request path", extractor.calls)
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != JobPending {
		t.Fatalf("status = %q, want pending", status.Status)
	}
	if !status.ReceivedAt.Equal(fixed) {
		t.Fatalf("received_at = %v, want %v", status.ReceivedAt, fixed)
	}
	if status.StartedAt != nil || status.FinishedAt != nil || len(status.Subjects) != 0 {
		t.Fatalf("status = %+v, want pending job without worker fields or subjects", status)
	}
}

func TestProcessNextMarksFailedJobStatusOnExtractError(t *testing.T) {
	// R-MG31-IUD1
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	times := sequenceTimes(
		time.Date(2026, 6, 20, 20, 31, 0, 0, time.UTC),
		time.Date(2026, 6, 20, 20, 31, 1, 0, time.UTC),
		time.Date(2026, 6, 20, 20, 31, 2, 0, time.UTC),
	)
	svc := NewService(conn, &recordingExtractor{err: errors.New("extract exploded")}, &recordingCompiler{}, times)
	svc.newID = sequenceIDs("job-1")

	jobID, err := svc.Ingest(ctx, "owner@example.com", "bad source", "Bad source", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	processed, err := svc.ProcessNext(ctx)
	if err != nil {
		t.Fatalf("ProcessNext returned error: %v", err)
	}
	if !processed {
		t.Fatal("ProcessNext processed = false, want true for pending job")
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != JobFailed {
		t.Fatalf("status = %q, want failed", status.Status)
	}
	if status.StartedAt == nil || status.FinishedAt == nil {
		t.Fatalf("status = %+v, want started and finished timestamps", status)
	}
	if !strings.Contains(status.Error, "extract exploded") {
		t.Fatalf("error = %q, want extract failure", status.Error)
	}
	if len(status.Subjects) != 0 {
		t.Fatalf("subjects = %#v, want none on failed extract", status.Subjects)
	}
}

func TestProcessNextReusesSubjectAndRecompilesFromCompleteClaims(t *testing.T) {
	// R-MDN8-RAVN
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	extractor := &recordingExtractor{batches: [][]extract.ExtractedSubject{
		{{
			Type:   "entity",
			Kind:   "company",
			Name:   "Acme Robotics",
			Claims: []string{"Acme Robotics opened a Tulsa lab."},
		}},
		{{
			Type:   "entity",
			Kind:   "company",
			Name:   " ACME   ROBOTICS ",
			Claims: []string{"Acme Robotics hired Mira Patel."},
		}},
	}}
	compiler := &recordingCompiler{}
	svc := NewService(conn, extractor, compiler, sequenceTimes(
		time.Date(2026, 6, 20, 20, 32, 0, 0, time.UTC),
		time.Date(2026, 6, 20, 20, 32, 1, 0, time.UTC),
		time.Date(2026, 6, 20, 20, 32, 2, 0, time.UTC),
		time.Date(2026, 6, 20, 20, 32, 3, 0, time.UTC),
		time.Date(2026, 6, 20, 20, 32, 4, 0, time.UTC),
		time.Date(2026, 6, 20, 20, 32, 5, 0, time.UTC),
	))
	svc.newID = sequenceIDs("job-1", "subject-1", "claim-1", "job-2", "claim-2")

	if _, err := svc.Ingest(ctx, "owner@example.com", "source one", "One", nil); err != nil {
		t.Fatalf("first Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("first ProcessNext = %v/%v, want true/nil", processed, err)
	}
	if _, err := svc.Ingest(ctx, "owner@example.com", "source two", "Two", nil); err != nil {
		t.Fatalf("second Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("second ProcessNext = %v/%v, want true/nil", processed, err)
	}

	if len(compiler.claimSets) != 2 {
		t.Fatalf("compile calls = %d, want 2", len(compiler.claimSets))
	}
	secondClaims := compiler.claimSets[1]
	if len(secondClaims) != 2 {
		t.Fatalf("second compile claims = %#v, want complete two-claim set", secondClaims)
	}
	if secondClaims[0].Body != "Acme Robotics opened a Tulsa lab." ||
		secondClaims[1].Body != "Acme Robotics hired Mira Patel." {
		t.Fatalf("second compile claims = %#v, want original plus new claim", secondClaims)
	}

	page, err := NewPageStore(conn).Get(ctx, "subject-1")
	if err != nil {
		t.Fatalf("Get page: %v", err)
	}
	if !strings.Contains(page.Body, "Acme Robotics hired Mira Patel.") {
		t.Fatalf("page body = %q, want recompiled body with latest claim", page.Body)
	}
}

func TestProcessNextCommitsPageWithoutPagesFTS(t *testing.T) {
	// R-PIGW-C9EM
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	assertNoPagesFTS(t, ctx, conn)

	extractor := &recordingExtractor{batches: [][]extract.ExtractedSubject{{
		{
			Type:   "entity",
			Kind:   "company",
			Name:   "Acme Robotics",
			Claims: []string{"Acme Robotics opened a Tulsa lab."},
		},
	}}}
	compiler := &recordingCompiler{}
	svc := NewService(conn, extractor, compiler, sequenceTimes(
		time.Date(2026, 6, 21, 10, 0, 0, 0, time.UTC),
		time.Date(2026, 6, 21, 10, 0, 1, 0, time.UTC),
		time.Date(2026, 6, 21, 10, 0, 2, 0, time.UTC),
	))
	svc.newID = sequenceIDs("job-1", "subject-1", "claim-1")

	jobID, err := svc.Ingest(ctx, "owner@example.com", "Acme Robotics opened a Tulsa lab.", "Tulsa lab", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	processed, err := svc.ProcessNext(ctx)
	if err != nil {
		t.Fatalf("ProcessNext: %v", err)
	}
	if !processed {
		t.Fatal("ProcessNext processed = false, want true")
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != JobDone || len(status.Subjects) != 1 || status.Subjects[0] != "subject-1" {
		t.Fatalf("status = %+v, want done with subject-1", status)
	}
	page, err := NewPageStore(conn).Get(ctx, "subject-1")
	if err != nil {
		t.Fatalf("Get page: %v", err)
	}
	if page.Title != "Acme Robotics" || !strings.Contains(page.Body, "Tulsa lab") {
		t.Fatalf("page = %+v, want compiled Tulsa page", page)
	}
	assertNoPagesFTS(t, ctx, conn)
}

func TestAbortPendingJobMarksAbortedAndPreventsProcessing(t *testing.T) {
	// R-0SCX-95OZ
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	now := time.Date(2026, 6, 22, 8, 0, 0, 0, time.UTC)
	extractor := &recordingExtractor{}
	svc := NewService(conn, extractor, &recordingCompiler{}, clockAt(now))
	svc.newID = sequenceIDs("job-1")

	jobID, err := svc.Ingest(ctx, "owner@example.com", "Acme Robotics opened a lab.", "Lab", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	result, err := svc.Abort(ctx, jobID)
	if err != nil {
		t.Fatalf("Abort: %v", err)
	}
	if !result.Aborted || result.Status != JobAborted {
		t.Fatalf("Abort result = %+v, want aborted status", result)
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != JobAborted || status.FinishedAt == nil {
		t.Fatalf("status = %+v, want aborted with finished_at", status)
	}
	processed, err := svc.ProcessNext(ctx)
	if err != nil {
		t.Fatalf("ProcessNext: %v", err)
	}
	if processed {
		t.Fatal("ProcessNext processed = true, want aborted pending job skipped")
	}
	if extractor.calls != 0 {
		t.Fatalf("extractor calls = %d, want 0 after abort", extractor.calls)
	}
}

func TestAbortTerminalJobLeavesStatusUnchanged(t *testing.T) {
	// R-0TKT-MXFO
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	svc := NewService(conn, &recordingExtractor{}, &recordingCompiler{}, clockAt(time.Date(2026, 6, 22, 8, 1, 0, 0, time.UTC)))
	svc.newID = sequenceIDs("job-1")
	jobID, err := svc.Ingest(ctx, "owner@example.com", "empty source", "Empty", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("ProcessNext = %v/%v, want true/nil", processed, err)
	}

	result, err := svc.Abort(ctx, jobID)
	if err != nil {
		t.Fatalf("Abort: %v", err)
	}
	if result.Aborted || result.Status != JobDone {
		t.Fatalf("Abort result = %+v, want unchanged done status", result)
	}
	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != JobDone {
		t.Fatalf("status = %q, want done", status.Status)
	}
}

func TestAbortWorkingJobIsNotOverwrittenByWorkerFinish(t *testing.T) {
	// R-0USQ-0P6D
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	extractor := &blockingExtractor{
		entered:  make(chan struct{}),
		release:  make(chan struct{}),
		canceled: make(chan struct{}),
	}
	svc := NewService(conn, extractor, &recordingCompiler{}, clockAt(time.Date(2026, 6, 22, 8, 2, 0, 0, time.UTC)))
	svc.newID = sequenceIDs("job-1")
	jobID, err := svc.Ingest(ctx, "owner@example.com", "Acme Robotics opened a lab.", "Lab", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}

	type processResult struct {
		processed bool
		err       error
	}
	done := make(chan processResult, 1)
	go func() {
		processed, err := svc.ProcessNext(ctx)
		done <- processResult{processed: processed, err: err}
	}()

	select {
	case <-extractor.entered:
	case <-time.After(2 * time.Second):
		t.Fatal("extractor was not entered")
	}
	result, err := svc.Abort(ctx, jobID)
	if err != nil {
		t.Fatalf("Abort: %v", err)
	}
	if !result.Aborted || result.Status != JobAborted {
		t.Fatalf("Abort result = %+v, want working job aborted", result)
	}
	select {
	case <-extractor.canceled:
	case <-time.After(2 * time.Second):
		t.Fatal("extractor context was not canceled by abort")
	}

	select {
	case got := <-done:
		if got.err != nil || !got.processed {
			t.Fatalf("ProcessNext = %v/%v, want true/nil", got.processed, got.err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("ProcessNext did not return")
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != JobAborted || status.StartedAt == nil || status.FinishedAt == nil {
		t.Fatalf("status = %+v, want aborted working job with lifecycle timestamps", status)
	}
	assertTableCount(t, ctx, conn, "subjects", 0)
	assertTableCount(t, ctx, conn, "claims", 0)
	assertTableCount(t, ctx, conn, "pages", 0)
}

func TestProcessNextRollsBackIntegratedRowsWhenCompileFails(t *testing.T) {
	// R-0W0M-EGX2
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	extractor := &recordingExtractor{batches: [][]extract.ExtractedSubject{{
		{
			Type:   "entity",
			Kind:   "company",
			Name:   "Acme Robotics",
			Claims: []string{"Acme Robotics opened a Tulsa lab."},
		},
	}}}
	compiler := &recordingCompiler{err: errors.New("compile exploded")}
	svc := NewService(conn, extractor, compiler, sequenceTimes(
		time.Date(2026, 6, 22, 8, 3, 0, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 3, 1, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 3, 2, 0, time.UTC),
	))
	svc.newID = sequenceIDs("job-1", "subject-1", "claim-1")

	jobID, err := svc.Ingest(ctx, "owner@example.com", "Acme Robotics opened a lab.", "Lab", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	processed, err := svc.ProcessNext(ctx)
	if err != nil {
		t.Fatalf("ProcessNext: %v", err)
	}
	if !processed {
		t.Fatal("ProcessNext processed = false, want true")
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus: %v", err)
	}
	if status.Status != JobFailed || !strings.Contains(status.Error, "compile exploded") {
		t.Fatalf("status = %+v, want failed with compile error", status)
	}
	assertTableCount(t, ctx, conn, "subjects", 0)
	assertTableCount(t, ctx, conn, "claims", 0)
	assertTableCount(t, ctx, conn, "pages", 0)
}

func TestRerunTerminalJobRequeuesAndUsesOriginalSourceText(t *testing.T) {
	// R-0X8I-S8NR
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	extractor := &recordingExtractor{}
	svc := NewService(conn, extractor, &recordingCompiler{}, sequenceTimes(
		time.Date(2026, 6, 22, 8, 4, 0, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 4, 1, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 4, 2, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 4, 3, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 4, 4, 0, time.UTC),
	))
	svc.newID = sequenceIDs("job-1")

	source := "Acme Robotics opened a lab from the original source."
	jobID, err := svc.Ingest(ctx, "owner@example.com", source, "Original title", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("first ProcessNext = %v/%v, want true/nil", processed, err)
	}

	result, err := svc.Rerun(ctx, jobID)
	if err != nil {
		t.Fatalf("Rerun: %v", err)
	}
	if !result.Requeued || result.Status != JobPending {
		t.Fatalf("Rerun result = %+v, want requeued pending", result)
	}
	pending, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus after Rerun: %v", err)
	}
	if pending.Status != JobPending || pending.StartedAt != nil || pending.FinishedAt != nil || pending.Error != "" {
		t.Fatalf("status after Rerun = %+v, want clean pending job", pending)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("second ProcessNext = %v/%v, want true/nil", processed, err)
	}
	if len(extractor.texts) != 2 || extractor.texts[0] != source || extractor.texts[1] != source {
		t.Fatalf("extractor texts = %#v, want original source used for both runs", extractor.texts)
	}
}

func TestRerunReplacesJobClaimsAndRecompilesPage(t *testing.T) {
	// R-0YGF-60EG
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	extractor := &recordingExtractor{batches: [][]extract.ExtractedSubject{
		{{
			Type:   "entity",
			Kind:   "company",
			Name:   "Acme Robotics",
			Claims: []string{"old claim from first run"},
		}},
		{{
			Type:   "entity",
			Kind:   "company",
			Name:   "Acme Robotics",
			Claims: []string{"new claim from rerun"},
		}},
	}}
	compiler := &recordingCompiler{}
	svc := NewService(conn, extractor, compiler, sequenceTimes(
		time.Date(2026, 6, 22, 8, 5, 0, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 5, 1, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 5, 2, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 5, 3, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 5, 4, 0, time.UTC),
	))
	svc.newID = sequenceIDs("job-1", "subject-1", "claim-1", "claim-2")

	jobID, err := svc.Ingest(ctx, "owner@example.com", "Acme source", "Acme", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("first ProcessNext = %v/%v, want true/nil", processed, err)
	}
	if _, err := svc.Rerun(ctx, jobID); err != nil {
		t.Fatalf("Rerun: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("second ProcessNext = %v/%v, want true/nil", processed, err)
	}

	claims, err := NewClaimStore(conn).ListBySubject(ctx, "subject-1")
	if err != nil {
		t.Fatalf("ListBySubject: %v", err)
	}
	if len(claims) != 1 || claims[0].JobID != jobID || claims[0].Body != "new claim from rerun" {
		t.Fatalf("claims after rerun = %+v, want exactly the new job claim", claims)
	}
	page, err := NewPageStore(conn).GetBySubject(ctx, "subject-1")
	if err != nil {
		t.Fatalf("GetBySubject: %v", err)
	}
	if strings.Contains(page.Body, "old claim") || !strings.Contains(page.Body, "new claim from rerun") {
		t.Fatalf("page body = %q, want recompiled page with new claim only", page.Body)
	}
}

func TestRerunRefreshesSubjectsDroppedByNewExtraction(t *testing.T) {
	// R-0ZOB-JS55
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	extractor := &recordingExtractor{batches: [][]extract.ExtractedSubject{
		{
			{Type: "entity", Kind: "company", Name: "Alpha Co", Claims: []string{"Alpha old claim"}},
			{Type: "entity", Kind: "company", Name: "Beta Co", Claims: []string{"Beta claim from first job"}},
			{Type: "concept", Kind: "concept", Name: "Dropped Concept", Claims: []string{"Dropped claim from first job"}},
		},
		{
			{Type: "entity", Kind: "company", Name: "Beta Co", Claims: []string{"Beta kept by another job"}},
		},
		{
			{Type: "entity", Kind: "company", Name: "Alpha Co", Claims: []string{"Alpha rerun claim"}},
		},
	}}
	svc := NewService(conn, extractor, &recordingCompiler{}, sequenceTimes(
		time.Date(2026, 6, 22, 8, 6, 0, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 6, 1, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 6, 2, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 6, 3, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 6, 4, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 6, 5, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 6, 6, 0, time.UTC),
		time.Date(2026, 6, 22, 8, 6, 7, 0, time.UTC),
	))
	svc.newID = sequenceIDs(
		"job-1", "subject-alpha", "claim-alpha-1", "subject-beta", "claim-beta-1", "subject-dropped", "claim-dropped-1",
		"job-2", "claim-beta-2",
		"claim-alpha-2",
	)

	jobID, err := svc.Ingest(ctx, "owner@example.com", "first source", "First", nil)
	if err != nil {
		t.Fatalf("first Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("first ProcessNext = %v/%v, want true/nil", processed, err)
	}
	if _, err := svc.Ingest(ctx, "owner@example.com", "second source", "Second", nil); err != nil {
		t.Fatalf("second Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("second ProcessNext = %v/%v, want true/nil", processed, err)
	}
	if _, err := svc.Rerun(ctx, jobID); err != nil {
		t.Fatalf("Rerun: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("rerun ProcessNext = %v/%v, want true/nil", processed, err)
	}

	betaClaims, err := NewClaimStore(conn).ListBySubject(ctx, "subject-beta")
	if err != nil {
		t.Fatalf("ListBySubject beta: %v", err)
	}
	if len(betaClaims) != 1 || betaClaims[0].JobID != "job-2" || betaClaims[0].Body != "Beta kept by another job" {
		t.Fatalf("beta claims = %+v, want only the other job's retained claim", betaClaims)
	}
	betaPage, err := NewPageStore(conn).GetBySubject(ctx, "subject-beta")
	if err != nil {
		t.Fatalf("GetBySubject beta: %v", err)
	}
	if strings.Contains(betaPage.Body, "first job") || !strings.Contains(betaPage.Body, "Beta kept by another job") {
		t.Fatalf("beta page body = %q, want reduced claim set", betaPage.Body)
	}
	if _, err := NewPageStore(conn).GetBySubject(ctx, "subject-dropped"); !errors.Is(err, sql.ErrNoRows) {
		t.Fatalf("dropped page lookup err = %v, want sql.ErrNoRows", err)
	}
	if _, err := NewSubjectStore(conn).Get(ctx, "subject-dropped"); err != nil {
		t.Fatalf("dropped subject row was not retained: %v", err)
	}
}

func TestRerunRefusesInProgressJobsWithoutChangingStatus(t *testing.T) {
	// R-10W7-XJVU
	t.Run("pending", func(t *testing.T) {
		ctx := context.Background()
		conn := migratedDB(t, ctx)
		defer conn.Close()

		svc := NewService(conn, &recordingExtractor{}, &recordingCompiler{}, clockAt(time.Date(2026, 6, 22, 8, 7, 0, 0, time.UTC)))
		svc.newID = sequenceIDs("job-1")
		jobID, err := svc.Ingest(ctx, "owner@example.com", "source", "Title", nil)
		if err != nil {
			t.Fatalf("Ingest: %v", err)
		}
		result, err := svc.Rerun(ctx, jobID)
		if !errors.Is(err, ErrJobNotTerminal) {
			t.Fatalf("Rerun err = %v, want ErrJobNotTerminal", err)
		}
		if result.Requeued || result.Status != JobPending {
			t.Fatalf("Rerun result = %+v, want unchanged pending", result)
		}
		status, err := svc.JobStatus(ctx, jobID)
		if err != nil {
			t.Fatalf("JobStatus: %v", err)
		}
		if status.Status != JobPending {
			t.Fatalf("status = %q, want pending", status.Status)
		}
	})

	t.Run("working", func(t *testing.T) {
		ctx := context.Background()
		conn := migratedDB(t, ctx)
		defer conn.Close()

		extractor := &blockingExtractor{
			entered: make(chan struct{}),
			release: make(chan struct{}),
		}
		svc := NewService(conn, extractor, &recordingCompiler{}, clockAt(time.Date(2026, 6, 22, 8, 8, 0, 0, time.UTC)))
		svc.newID = sequenceIDs("job-1")
		jobID, err := svc.Ingest(ctx, "owner@example.com", "source", "Title", nil)
		if err != nil {
			t.Fatalf("Ingest: %v", err)
		}

		done := make(chan error, 1)
		go func() {
			_, err := svc.ProcessNext(ctx)
			done <- err
		}()
		select {
		case <-extractor.entered:
		case <-time.After(2 * time.Second):
			t.Fatal("extractor was not entered")
		}

		result, err := svc.Rerun(ctx, jobID)
		if !errors.Is(err, ErrJobNotTerminal) {
			t.Fatalf("Rerun err = %v, want ErrJobNotTerminal", err)
		}
		if result.Requeued || result.Status != JobWorking {
			t.Fatalf("Rerun result = %+v, want unchanged working", result)
		}
		status, err := svc.JobStatus(ctx, jobID)
		if err != nil {
			t.Fatalf("JobStatus: %v", err)
		}
		if status.Status != JobWorking {
			t.Fatalf("status = %q, want working", status.Status)
		}

		close(extractor.release)
		select {
		case err := <-done:
			if err != nil {
				t.Fatalf("ProcessNext returned error: %v", err)
			}
		case <-time.After(2 * time.Second):
			t.Fatal("ProcessNext did not return")
		}
	})
}

func TestServiceListsSubjectsAndReadsClaimsAndPagesBySubject(t *testing.T) {
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	claims := NewClaimStore(conn)
	pages := NewPageStore(conn)
	if err := subjects.Save(ctx, Subject{ID: "subject-1", Name: "Acme Robotics", Type: "entity"}); err != nil {
		t.Fatalf("Save subject-1: %v", err)
	}
	if err := subjects.Save(ctx, Subject{ID: "subject-2", Name: "Acme Launch", Type: "event"}); err != nil {
		t.Fatalf("Save subject-2: %v", err)
	}
	if err := claims.Save(ctx, Claim{ID: "claim-1", SubjectID: "subject-1", JobID: "job-1", Body: "Acme Robotics opened a lab."}); err != nil {
		t.Fatalf("Save claim: %v", err)
	}
	if err := pages.Upsert(ctx, Page{ID: "page-1", SubjectID: "subject-1", Title: "Acme Robotics", Body: "Acme Robotics opened a lab."}); err != nil {
		t.Fatalf("Upsert page: %v", err)
	}

	gotSubjects, err := svc.Subjects(ctx, "entity", "robot")
	if err != nil {
		t.Fatalf("Subjects: %v", err)
	}
	if len(gotSubjects) != 1 || gotSubjects[0].ID != "subject-1" {
		t.Fatalf("Subjects = %+v, want subject-1 only", gotSubjects)
	}
	gotClaims, err := svc.ClaimsBySubject(ctx, "subject-1")
	if err != nil {
		t.Fatalf("ClaimsBySubject: %v", err)
	}
	if len(gotClaims) != 1 || gotClaims[0].ID != "claim-1" {
		t.Fatalf("ClaimsBySubject = %+v, want claim-1", gotClaims)
	}
	gotPage, err := svc.PageBySubject(ctx, "subject-1")
	if err != nil {
		t.Fatalf("PageBySubject: %v", err)
	}
	if gotPage.ID != "page-1" || gotPage.Title != "Acme Robotics" {
		t.Fatalf("PageBySubject = %+v, want page-1", gotPage)
	}
}

type recordingExtractor struct {
	calls   int
	err     error
	headers []extract.DocumentHeader
	texts   []string
	batches [][]extract.ExtractedSubject
}

func (e *recordingExtractor) Extract(_ context.Context, h extract.DocumentHeader, text string) ([]extract.ExtractedSubject, error) {
	e.calls++
	e.headers = append(e.headers, h)
	e.texts = append(e.texts, text)
	if e.err != nil {
		return nil, e.err
	}
	if len(e.batches) == 0 {
		return nil, nil
	}
	out := e.batches[0]
	e.batches = e.batches[1:]
	return out, nil
}

type blockingExtractor struct {
	entered  chan struct{}
	release  chan struct{}
	canceled chan struct{}
}

func (e *blockingExtractor) Extract(ctx context.Context, _ extract.DocumentHeader, _ string) ([]extract.ExtractedSubject, error) {
	close(e.entered)
	select {
	case <-e.release:
		return nil, nil
	case <-ctx.Done():
		if e.canceled != nil {
			close(e.canceled)
		}
		return nil, ctx.Err()
	}
}

type recordingCompiler struct {
	claimSets [][]Claim
	err       error
}

func (c *recordingCompiler) Compile(_ context.Context, subject Subject, claims []Claim) (string, string, error) {
	copied := append([]Claim(nil), claims...)
	c.claimSets = append(c.claimSets, copied)
	if c.err != nil {
		return "", "", c.err
	}
	var bodies []string
	for _, claim := range claims {
		bodies = append(bodies, claim.Body)
	}
	return subject.Name, strings.Join(bodies, "\n"), nil
}

func sequenceIDs(ids ...string) func() string {
	i := 0
	return func() string {
		if i >= len(ids) {
			return "extra-id"
		}
		id := ids[i]
		i++
		return id
	}
}

func sequenceTimes(times ...time.Time) func() time.Time {
	i := 0
	return func() time.Time {
		if i >= len(times) {
			return times[len(times)-1]
		}
		t := times[i]
		i++
		return t
	}
}

func clockAt(t time.Time) func() time.Time {
	return func() time.Time { return t }
}

func assertNoPagesFTS(t *testing.T, ctx context.Context, conn interface {
	QueryRowContext(context.Context, string, ...any) *sql.Row
}) {
	t.Helper()

	var count int
	if err := conn.QueryRowContext(ctx,
		`SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'pages_fts'`).
		Scan(&count); err != nil {
		t.Fatalf("count pages_fts table: %v", err)
	}
	if count != 0 {
		t.Fatalf("pages_fts table count = %d, want 0", count)
	}
}

func assertTableCount(t *testing.T, ctx context.Context, conn interface {
	QueryRowContext(context.Context, string, ...any) *sql.Row
}, table string, want int) {
	t.Helper()

	var got int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM `+table).Scan(&got); err != nil {
		t.Fatalf("count %s: %v", table, err)
	}
	if got != want {
		t.Fatalf("%s count = %d, want %d", table, got, want)
	}
}
