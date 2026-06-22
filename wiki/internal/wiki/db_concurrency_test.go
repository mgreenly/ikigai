package wiki

import (
	"context"
	"database/sql"
	"testing"
	"time"

	wikidb "wiki/internal/db"
)

func TestConcurrentReadReturnsLastCommittedSnapshotDuringOpenWrite(t *testing.T) {
	// R-FUCC-IT4M
	ctx := context.Background()
	conns, closeConns := migratedConcurrentConns(t, ctx)
	defer closeConns()

	if err := NewJobStore(conns).InsertIngest(ctx, Job{
		ID:         "job-snapshot",
		Owner:      "owner@example.com",
		SourceText: "source",
		Status:     JobPending,
		ReceivedAt: time.Date(2026, 6, 22, 9, 0, 0, 0, time.UTC),
	}); err != nil {
		t.Fatalf("InsertIngest: %v", err)
	}

	tx, err := conns.Write.BeginTx(ctx, nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	defer tx.Rollback()
	if _, err := tx.ExecContext(ctx, `UPDATE jobs SET status = ? WHERE id = ?`, JobWorking, "job-snapshot"); err != nil {
		t.Fatalf("uncommitted update: %v", err)
	}

	statusCh := make(chan JobStatus, 1)
	errCh := make(chan error, 1)
	go func() {
		status, err := NewJobStore(conns).Status(ctx, "job-snapshot")
		if err != nil {
			errCh <- err
			return
		}
		statusCh <- status
	}()

	select {
	case err := <-errCh:
		t.Fatalf("read while write tx open: %v", err)
	case status := <-statusCh:
		if status.Status != JobPending {
			t.Fatalf("read status = %q, want last committed %q", status.Status, JobPending)
		}
	case <-time.After(500 * time.Millisecond):
		t.Fatal("read blocked behind open write transaction")
	}
}

func TestConcurrentWritesSerializeWithoutBusy(t *testing.T) {
	// R-FVK8-WKVB
	ctx := context.Background()
	conns, closeConns := migratedConcurrentConns(t, ctx)
	defer closeConns()

	errCh := make(chan error, 2)
	for _, id := range []string{"job-write-1", "job-write-2"} {
		id := id
		go func() {
			errCh <- NewJobStore(conns).InsertIngest(ctx, Job{
				ID:         id,
				Owner:      "owner@example.com",
				SourceText: id + " source",
				Status:     JobPending,
				ReceivedAt: time.Date(2026, 6, 22, 9, 1, 0, 0, time.UTC),
			})
		}()
	}
	for i := 0; i < 2; i++ {
		select {
		case err := <-errCh:
			if err != nil {
				t.Fatalf("concurrent write %d returned error: %v", i+1, err)
			}
		case <-time.After(2 * time.Second):
			t.Fatalf("concurrent write %d did not complete", i+1)
		}
	}
	assertJobExists(t, ctx, conns.Read, "job-write-1")
	assertJobExists(t, ctx, conns.Read, "job-write-2")
	if got := conns.Write.Stats().MaxOpenConnections; got != 1 {
		t.Fatalf("write MaxOpenConnections = %d, want 1", got)
	}
}

func TestConcurrentReadAndControlWriteCompleteWhenWriterIdle(t *testing.T) {
	// R-FWS5-ACM0
	ctx := context.Background()
	conns, closeConns := migratedConcurrentConns(t, ctx)
	defer closeConns()

	if err := NewJobStore(conns).InsertIngest(ctx, Job{
		ID:         "job-readable",
		Owner:      "owner@example.com",
		SourceText: "source",
		Status:     JobPending,
		ReceivedAt: time.Date(2026, 6, 22, 9, 2, 0, 0, time.UTC),
	}); err != nil {
		t.Fatalf("InsertIngest seed: %v", err)
	}

	readDone := make(chan error, 1)
	writeDone := make(chan error, 1)
	go func() {
		status, err := NewJobStore(conns).Status(ctx, "job-readable")
		if err != nil {
			readDone <- err
			return
		}
		if status.Status != JobPending {
			readDone <- sql.ErrNoRows
			return
		}
		readDone <- nil
	}()
	go func() {
		writeDone <- NewJobStore(conns).InsertIngest(ctx, Job{
			ID:         "job-control-write",
			Owner:      "owner@example.com",
			SourceText: "control source",
			Status:     JobPending,
			ReceivedAt: time.Date(2026, 6, 22, 9, 2, 1, 0, time.UTC),
		})
	}()

	for name, ch := range map[string]chan error{"read": readDone, "write": writeDone} {
		select {
		case err := <-ch:
			if err != nil {
				t.Fatalf("%s completed with error: %v", name, err)
			}
		case <-time.After(2 * time.Second):
			t.Fatalf("%s did not complete while writer was idle", name)
		}
	}
	assertJobExists(t, ctx, conns.Read, "job-control-write")
}

func TestReadHandleSeesWriteHandleCommit(t *testing.T) {
	// R-FY01-O4CP
	ctx := context.Background()
	conns, closeConns := migratedConcurrentConns(t, ctx)
	defer closeConns()

	if err := NewJobStore(conns).InsertIngest(ctx, Job{
		ID:         "job-committed",
		Owner:      "owner@example.com",
		SourceText: "source",
		Status:     JobPending,
		ReceivedAt: time.Date(2026, 6, 22, 9, 3, 0, 0, time.UTC),
	}); err != nil {
		t.Fatalf("InsertIngest through writer: %v", err)
	}

	status, err := NewJobStore(conns).Status(ctx, "job-committed")
	if err != nil {
		t.Fatalf("Status through reader after commit: %v", err)
	}
	if status.ID != "job-committed" || status.Status != JobPending {
		t.Fatalf("status = %+v, want committed pending job visible through reader", status)
	}
	if _, err := conns.Read.ExecContext(ctx, `INSERT INTO jobs (id, status) VALUES ('read-write', 'pending')`); err == nil {
		t.Fatal("read handle accepted INSERT, want query_only failure")
	}
}

func migratedConcurrentConns(t *testing.T, ctx context.Context) (Conns, func()) {
	t.Helper()

	path := t.TempDir() + "/wiki.db"
	write, err := wikidb.Open(path)
	if err != nil {
		t.Fatalf("Open writer: %v", err)
	}
	if err := wikidb.Migrate(ctx, write); err != nil {
		write.Close()
		t.Fatalf("Migrate: %v", err)
	}
	read, err := wikidb.OpenRead(path)
	if err != nil {
		write.Close()
		t.Fatalf("OpenRead: %v", err)
	}
	return Conns{Read: read, Write: write}, func() {
		read.Close()
		write.Close()
	}
}

func assertJobExists(t *testing.T, ctx context.Context, conn *sql.DB, id string) {
	t.Helper()

	var got string
	if err := conn.QueryRowContext(ctx, `SELECT id FROM jobs WHERE id = ?`, id).Scan(&got); err != nil {
		t.Fatalf("select job %s: %v", id, err)
	}
	if got != id {
		t.Fatalf("selected job = %q, want %q", got, id)
	}
}
