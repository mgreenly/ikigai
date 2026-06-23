package wiki_test

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
	"sync"
	"testing"
	"time"

	"wiki/internal/extract"
	wikidomain "wiki/internal/wiki"
)

func TestAbortDuringCompileLeavesWriterIdleAndCommitsNoRows(t *testing.T) {
	// R-0TKT-MXFO
	// R-FWS5-ACM0
	ctx := context.Background()
	conns, closeConns := migratedConns(t, ctx)
	defer closeConns()

	compiler := newBlockingCompiler()
	svc := wikidomain.NewService(
		conns,
		staticExtractor{subjects: []extract.ExtractedSubject{{
			Type:   "entity",
			Kind:   "company",
			Name:   "Acme Robotics",
			Claims: []string{"Acme Robotics opened a Tulsa lab."},
		}}},
		compiler,
		func() time.Time { return time.Date(2026, 6, 22, 10, 30, 0, 0, time.UTC) },
	)

	jobID, err := svc.Ingest(ctx, "owner@example.com", "Acme Robotics opened a Tulsa lab.", "Tulsa lab", nil)
	if err != nil {
		t.Fatalf("Ingest: %v", err)
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

	select {
	case <-compiler.entered:
	case <-time.After(2 * time.Second):
		t.Fatal("compiler was not entered")
	}

	readDone := make(chan error, 1)
	go func() {
		status, err := svc.JobStatus(ctx, jobID)
		if err != nil {
			readDone <- err
			return
		}
		if status.Status != wikidomain.JobWorking {
			readDone <- fmt.Errorf("status = %q, want working", status.Status)
			return
		}
		readDone <- nil
	}()
	select {
	case err := <-readDone:
		if err != nil {
			close(compiler.release)
			t.Fatalf("read during blocked compile: %v", err)
		}
	case <-time.After(500 * time.Millisecond):
		close(compiler.release)
		t.Fatal("read blocked during compile")
	}

	abortDone := make(chan error, 1)
	go func() {
		result, err := svc.Abort(ctx, jobID)
		if err != nil {
			abortDone <- err
			return
		}
		if !result.Aborted || result.Status != wikidomain.JobAborted {
			abortDone <- fmt.Errorf("Abort result = %+v, want aborted status", result)
			return
		}
		abortDone <- nil
	}()
	select {
	case err := <-abortDone:
		if err != nil {
			close(compiler.release)
			t.Fatalf("abort during blocked compile: %v", err)
		}
	case <-time.After(500 * time.Millisecond):
		close(compiler.release)
		t.Fatal("abort blocked behind compile")
	}

	select {
	case <-compiler.canceled:
	case <-time.After(2 * time.Second):
		t.Fatal("compiler context was not canceled")
	}
	select {
	case got := <-processed:
		if got.err != nil || !got.processed {
			t.Fatalf("ProcessNext = %v/%v, want true/nil", got.processed, got.err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("ProcessNext did not return after abort")
	}

	status, err := svc.JobStatus(ctx, jobID)
	if err != nil {
		t.Fatalf("JobStatus after abort: %v", err)
	}
	if status.Status != wikidomain.JobAborted || len(status.Subjects) != 0 {
		t.Fatalf("status after abort = %+v, want aborted without subjects", status)
	}
	assertTableCount(t, ctx, conns.Read, "subjects", 0)
	assertTableCount(t, ctx, conns.Read, "claims", 0)
	assertTableCount(t, ctx, conns.Read, "pages", 0)
}

type staticExtractor struct {
	subjects []extract.ExtractedSubject
}

func (e staticExtractor) Extract(context.Context, extract.DocumentHeader, string) ([]extract.ExtractedSubject, error) {
	return e.subjects, nil
}

type blockingCompiler struct {
	entered      chan struct{}
	canceled     chan struct{}
	release      chan struct{}
	enteredOnce  sync.Once
	canceledOnce sync.Once
}

func newBlockingCompiler() *blockingCompiler {
	return &blockingCompiler{
		entered:  make(chan struct{}),
		canceled: make(chan struct{}),
		release:  make(chan struct{}),
	}
}

func (c *blockingCompiler) Compile(ctx context.Context, subject wikidomain.Subject, claims []wikidomain.Claim) (string, string, error) {
	c.enteredOnce.Do(func() { close(c.entered) })
	select {
	case <-ctx.Done():
		c.canceledOnce.Do(func() { close(c.canceled) })
		return "", "", ctx.Err()
	case <-c.release:
		var bodies []string
		for _, claim := range claims {
			bodies = append(bodies, claim.Body)
		}
		return subject.Name, strings.Join(bodies, "\n"), nil
	}
}

func assertTableCount(t *testing.T, ctx context.Context, conn *sql.DB, table string, want int) {
	t.Helper()

	var got int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM `+table).Scan(&got); err != nil {
		t.Fatalf("count %s: %v", table, err)
	}
	if got != want {
		t.Fatalf("%s count = %d, want %d", table, got, want)
	}
}
