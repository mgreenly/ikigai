package wiki

import (
	"context"
	"database/sql"
	"errors"
	"testing"
	"time"

	"wiki/internal/db"
	"wiki/internal/llm"
)

func TestNormalizePipeline(t *testing.T) {
	// R-7TVC-E7ZZ
	tests := map[string]string{
		"  Café\u0301\tNOIR  ": "cafe noir",
		"\u212bngström":        "angstrom",
		"ＡＬＰＨＡ   Beta":         "alpha beta",
	}
	for in, want := range tests {
		if got := normalize(in); got != want {
			t.Fatalf("normalize(%q) = %q, want %q", in, got, want)
		}
	}
}

func TestSubjectPathUsesTypeAndPathSafeSlug(t *testing.T) {
	// R-ZO9U-QOT8
	tests := map[string]string{
		Path(Subject{Type: "entity", NormName: "cafe noir"}):         "entity/cafe-noir",
		Path(Subject{Type: "event", NormName: " / alpha / beta// "}): "event/alpha-beta",
		Path(Subject{Type: "concept", NormName: "東京 / 京都"}):          "concept/東京-京都",
		Path(Subject{Type: "entity", NormName: "Camel Case"}):        "entity/Camel-Case",
	}
	for got, want := range tests {
		if got != want {
			t.Fatalf("Path(...) = %q, want %q", got, want)
		}
	}
}

func TestSubjectStoreGetByPathFindsExactTypeSlug(t *testing.T) {
	// R-ZQPN-I8AM
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := NewSubjectStore(conn)
	if err := subjects.Save(ctx, Subject{ID: "subject-1", Name: "Acme Robotics", Type: "entity"}); err != nil {
		t.Fatalf("Save subject-1: %v", err)
	}
	if err := subjects.Save(ctx, Subject{ID: "subject-2", Name: "Tulsa Launch", Type: "event"}); err != nil {
		t.Fatalf("Save subject-2: %v", err)
	}

	got, err := subjects.GetByPath(ctx, "entity/acme-robotics")
	if err != nil {
		t.Fatalf("GetByPath: %v", err)
	}
	if got.ID != "subject-1" || got.Type != "entity" || Path(got) != "entity/acme-robotics" {
		t.Fatalf("GetByPath returned %+v, want subject-1 at entity/acme-robotics", got)
	}
}

func TestSubjectStoreGetByPathRejectsFuzzyAndAliasMatches(t *testing.T) {
	// R-ZRXJ-W01B
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := NewSubjectStore(conn)
	if err := subjects.Save(ctx, Subject{ID: "subject-1", Name: "Acme Robotics", Type: "entity"}); err != nil {
		t.Fatalf("Save subject: %v", err)
	}

	for _, path := range []string{"entity/acme-robot", "concept/acme-robotics", "entity/acme robotics"} {
		if got, err := subjects.GetByPath(ctx, path); !errors.Is(err, ErrSubjectNotFound) {
			t.Fatalf("GetByPath(%q) = %+v, %v; want ErrSubjectNotFound", path, got, err)
		}
	}
}

func TestSubjectStoreGetByPathFailsOnAmbiguousSlug(t *testing.T) {
	// R-ZT5G-9RS0
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := NewSubjectStore(conn)
	if err := subjects.Save(ctx, Subject{ID: "subject-1", Name: "Alpha Beta", NormName: "alpha beta", Type: "entity"}); err != nil {
		t.Fatalf("Save subject-1: %v", err)
	}
	if err := subjects.Save(ctx, Subject{ID: "subject-2", Name: "Alpha/Beta", NormName: "alpha/beta", Type: "entity"}); err != nil {
		t.Fatalf("Save subject-2: %v", err)
	}

	if got, err := subjects.GetByPath(ctx, "entity/alpha-beta"); !errors.Is(err, ErrAmbiguousPath) {
		t.Fatalf("GetByPath returned %+v, %v; want ErrAmbiguousPath", got, err)
	}
}

func TestDomainStoresPersistPhaseOneModel(t *testing.T) {
	// R-7V38-RZQO
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	jobs := NewJobStore(conn)
	subjects := NewSubjectStore(conn)
	claims := NewClaimStore(conn)
	pages := NewPageStore(conn)

	if err := jobs.Save(ctx, Job{ID: "job-1", Status: "running"}); err != nil {
		t.Fatalf("Save job: %v", err)
	}
	if err := subjects.Save(ctx, Subject{ID: "subject-1", Name: " Café Noir ", Type: "entity"}); err != nil {
		t.Fatalf("Save subject: %v", err)
	}
	if err := claims.Save(ctx, Claim{ID: "claim-1", SubjectID: "subject-1", JobID: "job-1", Body: "Café Noir is a test subject."}); err != nil {
		t.Fatalf("Save claim: %v", err)
	}
	if err := pages.Upsert(ctx, Page{ID: "page-1", SubjectID: "subject-1", Title: "Café Noir", Body: "A generated page."}); err != nil {
		t.Fatalf("Upsert page: %v", err)
	}

	job, err := jobs.Get(ctx, "job-1")
	if err != nil {
		t.Fatalf("Get job: %v", err)
	}
	if job.Status != "running" {
		t.Fatalf("job.Status = %q, want running", job.Status)
	}

	subject, err := subjects.GetByNormName(ctx, "cafe noir")
	if err != nil {
		t.Fatalf("GetByNormName: %v", err)
	}
	if subject.ID != "subject-1" || subject.NormName != "cafe noir" {
		t.Fatalf("subject = %+v, want subject-1 with normalized name cafe noir", subject)
	}

	gotClaims, err := claims.ListBySubject(ctx, "subject-1")
	if err != nil {
		t.Fatalf("ListBySubject: %v", err)
	}
	if len(gotClaims) != 1 || gotClaims[0].JobID != "job-1" {
		t.Fatalf("claims = %+v, want one claim for job-1", gotClaims)
	}

	page, err := pages.Get(ctx, "page-1")
	if err != nil {
		t.Fatalf("Get page: %v", err)
	}
	if page.Title != "Café Noir" || page.Body != "A generated page." {
		t.Fatalf("page = %+v, want saved title and body", page)
	}
}

func TestLLMCallStorePersistsProviderCallFootprint(t *testing.T) {
	// R-VV3E-CLOB
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	started := time.Date(2026, 6, 22, 7, 3, 0, 0, time.UTC)
	ended := time.Date(2026, 6, 22, 7, 3, 1, 0, time.UTC)
	rec := llm.CallRecord{
		ID:        "call-1",
		Stage:     "extract",
		JobID:     "job-1",
		Attempt:   2,
		Provider:  "anthropic",
		Model:     "claude-test",
		Params:    `{"temperature":0}`,
		Request:   `{"system":"sys","user":"prompt"}`,
		Response:  `{"ok":true}`,
		Usage:     `{"total":12}`,
		Err:       "",
		StartedAt: started,
		EndedAt:   ended,
	}

	if err := NewLLMCallStore(conn).Record(ctx, rec); err != nil {
		t.Fatalf("Record: %v", err)
	}

	var got llm.CallRecord
	var startedRaw, endedRaw string
	err := conn.QueryRowContext(ctx, `
		SELECT id, stage, job_id, attempt, provider, model, params, request,
		       response, usage, err, started_at, ended_at
		FROM llm_calls
		WHERE id = ?`, "call-1").
		Scan(
			&got.ID,
			&got.Stage,
			&got.JobID,
			&got.Attempt,
			&got.Provider,
			&got.Model,
			&got.Params,
			&got.Request,
			&got.Response,
			&got.Usage,
			&got.Err,
			&startedRaw,
			&endedRaw,
		)
	if err != nil {
		t.Fatalf("query llm_calls: %v", err)
	}
	got.StartedAt = parseStoredTime(startedRaw)
	got.EndedAt = parseStoredTime(endedRaw)
	if got != rec {
		t.Fatalf("stored record = %+v, want %+v", got, rec)
	}
}

func migratedDB(t *testing.T, ctx context.Context) *sql.DB {
	t.Helper()

	conn, err := db.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if err := db.Migrate(ctx, conn); err != nil {
		conn.Close()
		t.Fatalf("Migrate: %v", err)
	}
	return conn
}
