package repos

import (
	"context"
	"database/sql"
	"errors"
	"path/filepath"
	"reflect"
	"testing"
	"time"

	appdb "appkit/db"
	reposdb "repos/internal/db"
)

func TestStoreRoundTripsRowsAndSelectsSessionQueueState(t *testing.T) {
	// R-ENOJ-LOXR
	store, _ := migratedStore(t)
	ctx := context.Background()
	created := time.Date(2026, 7, 15, 12, 30, 0, 123456789, time.UTC)
	repo := Repo{
		Name: "fixture", OwnerEmail: "owner@example.com",
		CloneURL: "https://github.com/example/fixture.git", DefaultBranch: "main", CreatedAt: created,
	}
	if err := store.InsertRepo(ctx, repo); err != nil {
		t.Fatalf("insert repo: %v", err)
	}
	gotRepo, err := store.GetRepo(ctx, repo.Name)
	if err != nil {
		t.Fatalf("get repo: %v", err)
	}
	if !reflect.DeepEqual(gotRepo, repo) {
		t.Fatalf("repo round trip = %#v, want %#v", gotRepo, repo)
	}

	issue := 42
	started := created.Add(time.Minute)
	ended := created.Add(2 * time.Minute)
	failure := "fixture failure"
	prURL := "https://github.com/example/fixture/pull/7"
	session := Session{
		ID: "01J00000000000000000000000", RepoName: repo.Name, OwnerEmail: repo.OwnerEmail,
		IssueNumber: &issue, Attempt: 3, Branch: "agent/issue-42-3",
		Instructions: "Implement the fixture.", Status: StatusFailed, Error: &failure, PRURL: &prURL,
		CreatedAt: created, StartedAt: &started, EndedAt: &ended, LogPath: "state/sessions/01J/output.jsonl",
	}
	if err := store.InsertSession(ctx, session); err != nil {
		t.Fatalf("insert session: %v", err)
	}
	gotSession, err := store.GetSession(ctx, session.ID)
	if err != nil {
		t.Fatalf("get session: %v", err)
	}
	if !reflect.DeepEqual(gotSession, session) {
		t.Fatalf("session round trip = %#v, want %#v", gotSession, session)
	}

	if max, err := store.MaxAttempt(ctx, repo.Name, 999); err != nil || max != 0 {
		t.Fatalf("unseen MaxAttempt = %d, %v; want 0, nil", max, err)
	}
	insertSession(t, store, Session{ID: "queued-new", RepoName: repo.Name, OwnerEmail: repo.OwnerEmail,
		IssueNumber: &issue, Attempt: 4, Branch: "queued-new", Instructions: "new", Status: StatusQueued,
		CreatedAt: created.Add(4 * time.Minute), LogPath: "new.jsonl"})
	insertSession(t, store, Session{ID: "queued-old", RepoName: repo.Name, OwnerEmail: repo.OwnerEmail,
		IssueNumber: &issue, Attempt: 5, Branch: "queued-old", Instructions: "old", Status: StatusQueued,
		CreatedAt: created.Add(3 * time.Minute), LogPath: "old.jsonl"})

	active, err := store.ActiveSessionForIssue(ctx, repo.Name, issue)
	if err != nil || active.ID != "queued-old" {
		t.Fatalf("active session = %q, %v; want queued-old", active.ID, err)
	}
	next, err := store.NextQueued(ctx)
	if err != nil || next.ID != "queued-old" {
		t.Fatalf("next queued = %q, %v; want queued-old", next.ID, err)
	}
	if max, err := store.MaxAttempt(ctx, repo.Name, issue); err != nil || max != 5 {
		t.Fatalf("MaxAttempt = %d, %v; want 5, nil", max, err)
	}
	for i, status := range []string{StatusSucceeded, StatusFailed, StatusCancelled} {
		terminalIssue := 100 + i
		insertSession(t, store, Session{ID: "terminal-" + status, RepoName: repo.Name, OwnerEmail: repo.OwnerEmail,
			IssueNumber: &terminalIssue, Attempt: 1, Branch: "terminal-" + status, Instructions: "done", Status: status,
			CreatedAt: created.Add(time.Duration(10+i) * time.Minute), LogPath: status + ".jsonl"})
		if _, err := store.ActiveSessionForIssue(ctx, repo.Name, terminalIssue); !errors.Is(err, ErrNotFound) {
			t.Errorf("%s-only issue lookup error = %v, want ErrNotFound", status, err)
		}
	}

	if err := store.MarkRunning(ctx, "queued-old", created.Add(5*time.Minute)); err != nil {
		t.Fatalf("mark running: %v", err)
	}
	active, err = store.ActiveSessionForIssue(ctx, repo.Name, issue)
	if err != nil || active.ID != "queued-old" || active.Status != StatusRunning {
		t.Fatalf("running active session = %#v, %v", active, err)
	}
}

func TestFinishSessionRollsBackTerminalWriteAndOutcomeOnAppenderFailure(t *testing.T) {
	// R-EOWF-ZGOG
	store, conn := migratedStore(t)
	ctx := context.Background()
	created := time.Date(2026, 7, 15, 9, 0, 0, 0, time.UTC)
	original := Session{ID: "rollback", RepoName: "fixture", OwnerEmail: "owner@example.com",
		Attempt: 1, Branch: "manual", Instructions: "do it", Status: StatusRunning,
		CreatedAt: created, LogPath: "rollback.jsonl"}
	insertSession(t, store, original)
	forced := errors.New("forced append failure")
	sessionError, prURL := "agent failed", "https://github.com/example/fixture/pull/8"
	err := store.FinishSession(ctx, original.ID, StatusFailed, &sessionError, &prURL, created.Add(time.Hour),
		func(ctx context.Context, tx *sql.Tx, _ Session) error {
			if _, err := tx.ExecContext(ctx, `INSERT INTO outbox
				(event_id, kind, subject, payload, created_at) VALUES (?, ?, ?, ?, ?)`,
				"event-rollback", "session.failed", "/repos/fixture", `{}`, formatTime(created)); err != nil {
				return err
			}
			return forced
		})
	if !errors.Is(err, forced) {
		t.Fatalf("FinishSession error = %v, want forced failure", err)
	}
	after, err := store.GetSession(ctx, original.ID)
	if err != nil {
		t.Fatalf("get rolled-back session: %v", err)
	}
	if !reflect.DeepEqual(after, original) {
		t.Fatalf("session changed across rollback: got %#v, want %#v", after, original)
	}
	var outboxRows int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM outbox`).Scan(&outboxRows); err != nil {
		t.Fatalf("count outbox rows: %v", err)
	}
	if outboxRows != 0 {
		t.Fatalf("outbox rows after rollback = %d, want 0", outboxRows)
	}
}

func TestFinishSessionCommitsTerminalFieldsAndOutcomeTogether(t *testing.T) {
	store, conn := migratedStore(t)
	ctx := context.Background()
	created := time.Date(2026, 7, 15, 10, 0, 0, 0, time.UTC)
	original := Session{ID: "success", RepoName: "fixture", OwnerEmail: "owner@example.com",
		Attempt: 1, Branch: "manual", Instructions: "do it", Status: StatusRunning,
		CreatedAt: created, LogPath: "success.jsonl"}
	insertSession(t, store, original)
	prURL := "https://github.com/example/fixture/pull/9"
	ended := created.Add(time.Hour)
	if err := store.FinishSession(ctx, original.ID, StatusSucceeded, nil, &prURL, ended,
		func(ctx context.Context, tx *sql.Tx, session Session) error {
			if session.Status != StatusSucceeded {
				t.Fatalf("appender observed status %q, want succeeded", session.Status)
			}
			_, err := tx.ExecContext(ctx, `INSERT INTO outbox
				(event_id, kind, subject, payload, created_at) VALUES (?, ?, ?, ?, ?)`,
				"event-success", "session.succeeded", "/repos/fixture", `{}`, formatTime(ended))
			return err
		}); err != nil {
		t.Fatalf("finish session: %v", err)
	}
	got, err := store.GetSession(ctx, original.ID)
	if err != nil {
		t.Fatalf("get finished session: %v", err)
	}
	if got.Status != StatusSucceeded || got.Error != nil || got.PRURL == nil || *got.PRURL != prURL || got.EndedAt == nil || !got.EndedAt.Equal(ended) {
		t.Fatalf("finished fields = %#v", got)
	}
	var outboxRows int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM outbox`).Scan(&outboxRows); err != nil || outboxRows != 1 {
		t.Fatalf("outbox rows = %d, %v; want 1, nil", outboxRows, err)
	}
}

func migratedStore(t *testing.T) (*Store, *sql.DB) {
	t.Helper()
	conn, err := appdb.Open(filepath.Join(t.TempDir(), "repos.db"))
	if err != nil {
		t.Fatalf("open temp database: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migrations, err := reposdb.Migrations()
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appdb.Migrate(context.Background(), conn, migrations); err != nil {
		t.Fatalf("migrate temp database: %v", err)
	}
	return NewStore(conn), conn
}

func insertSession(t *testing.T, store *Store, session Session) {
	t.Helper()
	if err := store.InsertSession(context.Background(), session); err != nil {
		t.Fatalf("insert session %s: %v", session.ID, err)
	}
}
