package wiki

import (
	"context"
	"database/sql"
	"errors"
	"testing"
	"time"

	"wiki/internal/db"
	"wiki/internal/llm"
	"wiki/internal/page"
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

	gotClaims, _, err := claims.ListBySubject(ctx, "subject-1", page.Params{})
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

func TestSubjectStoreCursorPaginationWalksRowsExactlyOnceInKeyOrder(t *testing.T) {
	// R-17C5-VP2I
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := NewSubjectStore(conn)
	for _, subject := range []Subject{
		{ID: "subject-2", Name: "Alpha", Type: "entity"},
		{ID: "subject-1", Name: "Alpha", NormName: "alpha one", Type: "entity"},
		{ID: "subject-3", Name: "Beta", Type: "event"},
		{ID: "subject-4", Name: "Gamma", Type: "concept"},
		{ID: "subject-5", Name: "Omega", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save %s: %v", subject.ID, err)
		}
	}

	var got []string
	params := page.Params{Limit: 2}
	for pageNum := 0; ; pageNum++ {
		batch, next, err := subjects.List(ctx, "", "", params)
		if err != nil {
			t.Fatalf("List page %d: %v", pageNum, err)
		}
		for _, subject := range batch {
			got = append(got, subject.ID)
		}
		switch pageNum {
		case 0, 1:
			if next == "" {
				t.Fatalf("page %d next cursor is empty before final page", pageNum)
			}
		case 2:
			if next != "" {
				t.Fatalf("final page next cursor = %q, want empty", next)
			}
		}
		if next == "" {
			break
		}
		params.Cursor = next
	}

	want := []string{"subject-1", "subject-2", "subject-3", "subject-4", "subject-5"}
	if len(got) != len(want) {
		t.Fatalf("walked ids = %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("walked ids = %v, want %v", got, want)
		}
	}
}

func TestListFiltersApplyBeforePaging(t *testing.T) {
	// R-18K2-9GT7
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	t1 := time.Date(2026, 6, 22, 8, 0, 0, 0, time.UTC)
	t2 := t1.Add(time.Minute)
	t3 := t2.Add(time.Minute)
	t4 := t3.Add(time.Minute)
	jobs := NewJobStore(conn)
	for _, job := range []Job{
		{ID: "job-pending", Status: JobPending, ReceivedAt: t1},
		{ID: "job-done-1", Status: JobDone, ReceivedAt: t2},
		{ID: "job-done-2", Status: JobDone, ReceivedAt: t3},
		{ID: "job-late", Status: JobDone, ReceivedAt: t4},
	} {
		if err := jobs.InsertIngest(ctx, job); err != nil {
			t.Fatalf("InsertIngest %s: %v", job.ID, err)
		}
	}
	gotJobs, next, err := jobs.ListJobs(ctx, JobFilter{Statuses: []string{JobDone}, Since: t2, Until: t3}, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("ListJobs: %v", err)
	}
	if next != "" || len(gotJobs) != 2 || gotJobs[0].ID != "job-done-2" || gotJobs[1].ID != "job-done-1" {
		t.Fatalf("ListJobs = %+v, next %q; want two filtered done jobs in time range", gotJobs, next)
	}

	subjects := NewSubjectStore(conn)
	for _, subject := range []Subject{
		{ID: "subject-robot", Name: "Acme Robot Lab", Type: "entity"},
		{ID: "subject-event", Name: "Acme Robot Launch", Type: "event"},
		{ID: "subject-other", Name: "Zed Archives", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save subject %s: %v", subject.ID, err)
		}
	}
	gotSubjects, next, err := subjects.List(ctx, "entity", "robot", page.Params{Limit: 1})
	if err != nil {
		t.Fatalf("Subject List: %v", err)
	}
	if next != "" || len(gotSubjects) != 1 || gotSubjects[0].ID != "subject-robot" {
		t.Fatalf("Subject List = %+v, next %q; want only matching entity robot subject", gotSubjects, next)
	}

	claims := NewClaimStore(conn)
	for _, claim := range []Claim{
		{ID: "claim-1", SubjectID: "subject-robot", JobID: "job-done-1", Body: "Robot lab opened."},
		{ID: "claim-2", SubjectID: "subject-event", JobID: "job-done-1", Body: "Robot launch happened."},
		{ID: "claim-3", SubjectID: "subject-robot", JobID: "job-done-2", Body: "Robot lab expanded."},
	} {
		if err := claims.Save(ctx, claim); err != nil {
			t.Fatalf("Save claim %s: %v", claim.ID, err)
		}
	}
	var gotClaimIDs []string
	claimParams := page.Params{Limit: 1}
	for {
		gotClaims, next, err := claims.ListBySubject(ctx, "subject-robot", claimParams)
		if err != nil {
			t.Fatalf("ListBySubject: %v", err)
		}
		for _, claim := range gotClaims {
			gotClaimIDs = append(gotClaimIDs, claim.ID)
		}
		if next == "" {
			break
		}
		claimParams.Cursor = next
	}
	if len(gotClaimIDs) != 2 || gotClaimIDs[0] != "claim-1" || gotClaimIDs[1] != "claim-3" {
		t.Fatalf("ListBySubject walked %v, want only subject-robot claims", gotClaimIDs)
	}

	calls := NewLLMCallStore(conn)
	for _, rec := range []CallRecord{
		{ID: "call-1", Stage: "compile", JobID: "job-done-1", Attempt: 1, Provider: "test", Model: "m", StartedAt: t1, EndedAt: t1},
		{ID: "call-2", Stage: "extract", JobID: "job-done-1", Attempt: 1, Provider: "test", Model: "m", StartedAt: t2, EndedAt: t2},
		{ID: "call-3", Stage: "extract", JobID: "job-done-2", Attempt: 1, Provider: "test", Model: "m", StartedAt: t3, EndedAt: t3},
		{ID: "call-4", Stage: "extract", JobID: "job-done-1", Attempt: 1, Provider: "test", Model: "m", StartedAt: t4, EndedAt: t4},
	} {
		if err := calls.Record(ctx, rec); err != nil {
			t.Fatalf("Record %s: %v", rec.ID, err)
		}
	}
	gotCalls, next, err := calls.List(ctx, LLMCallFilter{JobID: "job-done-1", Stage: "extract", Since: t2, Until: t3}, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("LLMCall List: %v", err)
	}
	if next != "" || len(gotCalls) != 1 || gotCalls[0].ID != "call-2" {
		t.Fatalf("LLMCall List = %+v, next %q; want only job/stage/time filtered call-2", gotCalls, next)
	}
}

func TestListJobsOrdersNewestFirstFilteredOrAll(t *testing.T) {
	// R-XYAZ-V0XE
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	t1 := time.Date(2026, 6, 22, 8, 0, 0, 0, time.UTC)
	t2 := t1.Add(time.Minute)
	jobs := NewJobStore(conn)
	for _, job := range []Job{
		{ID: "job-old", Status: JobDone, ReceivedAt: t1},
		{ID: "job-new-a", Status: JobPending, ReceivedAt: t2},
		{ID: "job-new-b", Status: JobDone, ReceivedAt: t2},
	} {
		if err := jobs.InsertIngest(ctx, job); err != nil {
			t.Fatalf("InsertIngest %s: %v", job.ID, err)
		}
	}

	got, next, err := jobs.ListJobs(ctx, JobFilter{}, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("ListJobs all: %v", err)
	}
	if next != "" || !sameStrings(jobIDs(got), []string{"job-new-b", "job-new-a", "job-old"}) {
		t.Fatalf("ListJobs all ids = %v, next %q; want newest-first across all states", jobIDs(got), next)
	}

	got, next, err = jobs.ListJobs(ctx, JobFilter{Statuses: []string{JobDone}}, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("ListJobs done: %v", err)
	}
	if next != "" || !sameStrings(jobIDs(got), []string{"job-new-b", "job-old"}) {
		t.Fatalf("ListJobs done ids = %v, next %q; want newest-first within filtered jobs", jobIDs(got), next)
	}
}

func TestListJobsStatusesMatchAnyAndEmptyMeansAll(t *testing.T) {
	// R-XZIW-8SO3
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	base := time.Date(2026, 6, 22, 9, 0, 0, 0, time.UTC)
	jobs := NewJobStore(conn)
	for i, job := range []Job{
		{ID: "job-pending", Status: JobPending},
		{ID: "job-done", Status: JobDone},
		{ID: "job-failed", Status: JobFailed},
		{ID: "job-aborted", Status: JobAborted},
	} {
		job.ReceivedAt = base.Add(time.Duration(i) * time.Minute)
		if err := jobs.InsertIngest(ctx, job); err != nil {
			t.Fatalf("InsertIngest %s: %v", job.ID, err)
		}
	}

	got, next, err := jobs.ListJobs(ctx, JobFilter{Statuses: []string{JobPending, JobFailed}}, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("ListJobs multi-status: %v", err)
	}
	if next != "" || !sameStrings(jobIDs(got), []string{"job-failed", "job-pending"}) {
		t.Fatalf("ListJobs multi-status ids = %v, next %q; want failed and pending only", jobIDs(got), next)
	}

	got, next, err = jobs.ListJobs(ctx, JobFilter{Statuses: []string{}}, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("ListJobs empty statuses: %v", err)
	}
	if next != "" || !sameStrings(jobIDs(got), []string{"job-aborted", "job-failed", "job-done", "job-pending"}) {
		t.Fatalf("ListJobs empty-status ids = %v, next %q; want all states", jobIDs(got), next)
	}
}

func TestCountJobsMatchesFilteredListCountWithoutPaging(t *testing.T) {
	// R-Y1YP-0C5H
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	t1 := time.Date(2026, 6, 22, 10, 0, 0, 0, time.UTC)
	t2 := t1.Add(time.Minute)
	t3 := t2.Add(time.Minute)
	t4 := t3.Add(time.Minute)
	jobs := NewJobStore(conn)
	for _, job := range []Job{
		{ID: "job-old", Status: JobDone, ReceivedAt: t1},
		{ID: "job-done", Status: JobDone, ReceivedAt: t2},
		{ID: "job-failed", Status: JobFailed, ReceivedAt: t3},
		{ID: "job-pending", Status: JobPending, ReceivedAt: t4},
	} {
		if err := jobs.InsertIngest(ctx, job); err != nil {
			t.Fatalf("InsertIngest %s: %v", job.ID, err)
		}
	}

	filter := JobFilter{
		Statuses: []string{JobDone, JobFailed},
		Since:    t2,
		Until:    t3,
	}
	listed, next, err := jobs.ListJobs(ctx, filter, page.Params{Limit: 10})
	if err != nil {
		t.Fatalf("ListJobs filtered: %v", err)
	}
	if next != "" || !sameStrings(jobIDs(listed), []string{"job-failed", "job-done"}) {
		t.Fatalf("ListJobs filtered ids = %v, next %q; want two filtered jobs", jobIDs(listed), next)
	}

	count, err := jobs.CountJobs(ctx, filter)
	if err != nil {
		t.Fatalf("CountJobs: %v", err)
	}
	if count != len(listed) {
		t.Fatalf("CountJobs = %d, ListJobs count = %d", count, len(listed))
	}

	paged, next, err := jobs.ListJobs(ctx, filter, page.Params{Limit: 1})
	if err != nil {
		t.Fatalf("ListJobs paged: %v", err)
	}
	if len(paged) != 1 || next == "" {
		t.Fatalf("ListJobs paged = %v, next %q; want one row plus next cursor", jobIDs(paged), next)
	}
	count, err = jobs.CountJobs(ctx, filter)
	if err != nil {
		t.Fatalf("CountJobs after paged list: %v", err)
	}
	if count != 2 {
		t.Fatalf("CountJobs = %d, want unpaged filtered total 2", count)
	}
}

func TestListMethodsRejectUndecodableCursor(t *testing.T) {
	// R-1DFN-SJRZ
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	params := page.Params{Limit: 1, Cursor: "not a cursor"}
	if _, _, err := NewJobStore(conn).ListJobs(ctx, JobFilter{}, params); !errors.Is(err, ErrInvalidCursor) {
		t.Fatalf("ListJobs err = %v, want ErrInvalidCursor", err)
	}
	if _, _, err := NewSubjectStore(conn).List(ctx, "", "", params); !errors.Is(err, ErrInvalidCursor) {
		t.Fatalf("Subject List err = %v, want ErrInvalidCursor", err)
	}
	if _, _, err := NewClaimStore(conn).ListBySubject(ctx, "subject-1", params); !errors.Is(err, ErrInvalidCursor) {
		t.Fatalf("ListBySubject err = %v, want ErrInvalidCursor", err)
	}
	if _, _, err := NewLLMCallStore(conn).List(ctx, LLMCallFilter{}, params); !errors.Is(err, ErrInvalidCursor) {
		t.Fatalf("LLMCall List err = %v, want ErrInvalidCursor", err)
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

func jobIDs(jobs []Job) []string {
	ids := make([]string, 0, len(jobs))
	for _, job := range jobs {
		ids = append(ids, job.ID)
	}
	return ids
}

func sameStrings(got, want []string) bool {
	if len(got) != len(want) {
		return false
	}
	for i := range want {
		if got[i] != want[i] {
			return false
		}
	}
	return true
}
