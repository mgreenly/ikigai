package wiki

import (
	"context"
	"database/sql"
	"errors"
	"strings"
	"testing"
	"time"

	"wiki/internal/extract"
	"wiki/internal/page"
)

func TestAliasesMigrationCreatesConstrainedLookupTable(t *testing.T) {
	// R-BGPF-NVTU
	// R-BHXC-1NKJ
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	if _, err := conn.ExecContext(ctx, `PRAGMA foreign_keys = ON`); err != nil {
		t.Fatalf("enable foreign keys: %v", err)
	}

	var tableSQL string
	if err := conn.QueryRowContext(ctx,
		`SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'aliases'`).
		Scan(&tableSQL); err != nil {
		t.Fatalf("lookup aliases table: %v", err)
	}
	compactTableSQL := strings.Join(strings.Fields(tableSQL), " ")
	for _, want := range []string{
		"norm_name TEXT NOT NULL UNIQUE",
		"subject_id TEXT NOT NULL REFERENCES subjects(id) ON DELETE RESTRICT",
		"name TEXT NOT NULL",
		"created_by TEXT NOT NULL",
		"created_at TEXT NOT NULL",
	} {
		if !strings.Contains(compactTableSQL, want) {
			t.Fatalf("aliases SQL = %q, want %q", tableSQL, want)
		}
	}
	var indexName string
	if err := conn.QueryRowContext(ctx,
		`SELECT name FROM sqlite_master WHERE type = 'index' AND name = 'aliases_subject'`).
		Scan(&indexName); err != nil {
		t.Fatalf("lookup aliases_subject index: %v", err)
	}

	subjects := NewSubjectStore(conn)
	if err := subjects.Save(ctx, Subject{ID: "subject-survivor", Name: "Café Noir", Type: "entity"}); err != nil {
		t.Fatalf("Save subject: %v", err)
	}
	aliases := NewAliasStore(conn)
	al := Alias{
		NormName:  "Cafe Old",
		SubjectID: "subject-survivor",
		Name:      "Café Old",
		CreatedBy: "owner@example.com",
		CreatedAt: time.Date(2026, 6, 23, 12, 0, 0, 0, time.UTC).Format(time.RFC3339Nano),
	}
	if err := aliases.Insert(ctx, al); err != nil {
		t.Fatalf("Insert alias: %v", err)
	}
	if err := aliases.Insert(ctx, al); err == nil {
		t.Fatal("duplicate alias insert succeeded, want unique norm_name failure")
	}
	if _, err := conn.ExecContext(ctx, `DELETE FROM subjects WHERE id = ?`, "subject-survivor"); err == nil {
		t.Fatal("deleted aliased subject, want foreign-key restrict failure")
	}
}

func TestAliasStorePersistsLookupAndRepointsSubjects(t *testing.T) {
	// R-BJ58-FFB8
	// R-BKD4-T71X
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := NewSubjectStore(conn)
	for _, subject := range []Subject{
		{ID: "subject-old", Name: "Old Name", Type: "entity"},
		{ID: "subject-new", Name: "New Name", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save %s: %v", subject.ID, err)
		}
	}
	aliases := NewAliasStore(conn)
	if err := aliases.Insert(ctx, Alias{
		Name:      "  Café   Former  ",
		SubjectID: "subject-old",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-23T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert: %v", err)
	}
	got, err := aliases.GetByNormName(ctx, "cafe former")
	if err != nil {
		t.Fatalf("GetByNormName: %v", err)
	}
	if got.NormName != "cafe former" || got.SubjectID != "subject-old" || got.Name != "  Café   Former  " {
		t.Fatalf("alias = %+v, want normalized lookup pointing at subject-old", got)
	}

	if err := aliases.RepointSubject(ctx, "subject-old", "subject-new"); err != nil {
		t.Fatalf("RepointSubject: %v", err)
	}
	got, err = aliases.GetByNormName(ctx, "CAFÉ FORMER")
	if err != nil {
		t.Fatalf("GetByNormName after repoint: %v", err)
	}
	if got.SubjectID != "subject-new" {
		t.Fatalf("alias subject = %q, want subject-new", got.SubjectID)
	}
}

func TestResolverPrefersSubjectsThenAliasesAndReportsNotFound(t *testing.T) {
	// R-BLL1-6YSM
	// R-BMSX-KQJB
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := NewSubjectStore(conn)
	for _, subject := range []Subject{
		{ID: "subject-canonical", Name: "Current Name", Type: "entity"},
		{ID: "subject-direct", Name: "Legacy Name", Type: "event"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save %s: %v", subject.ID, err)
		}
	}
	if err := NewAliasStore(conn).Insert(ctx, Alias{
		Name:      "Legacy Name",
		SubjectID: "subject-canonical",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-23T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert alias: %v", err)
	}
	resolver := NewResolver(conn)

	got, err := resolver.ResolveByName(ctx, "legacy name")
	if err != nil {
		t.Fatalf("ResolveByName direct: %v", err)
	}
	if got.ID != "subject-direct" {
		t.Fatalf("direct resolution = %+v, want subject-direct before alias", got)
	}

	if _, err := conn.ExecContext(ctx, `DELETE FROM subjects WHERE id = ?`, "subject-direct"); err != nil {
		t.Fatalf("delete direct subject: %v", err)
	}
	got, err = resolver.ResolveByName(ctx, "LEGACY NAME")
	if err != nil {
		t.Fatalf("ResolveByName alias: %v", err)
	}
	if got.ID != "subject-canonical" {
		t.Fatalf("alias resolution = %+v, want subject-canonical", got)
	}

	if got, err := resolver.ResolveByName(ctx, "missing name"); !errors.Is(err, ErrSubjectNotFound) {
		t.Fatalf("missing resolution = %+v, %v; want ErrSubjectNotFound", got, err)
	}
}

func TestProcessNextAppliesAliasedNameToSurvivorSubject(t *testing.T) {
	// R-BO0T-YIA0
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	if err := NewSubjectStore(conn).Save(ctx, Subject{ID: "subject-survivor", Name: "Current Name", Type: "entity"}); err != nil {
		t.Fatalf("Save survivor: %v", err)
	}
	if err := NewAliasStore(conn).Insert(ctx, Alias{
		Name:      "Former Name",
		SubjectID: "subject-survivor",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-23T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert alias: %v", err)
	}
	extractor := &recordingExtractor{batches: [][]extract.ExtractedSubject{{
		{Type: "entity", Name: "Former Name", Claims: []string{"Former Name shipped the release."}},
	}}}
	compiler := &recordingCompiler{}
	svc := NewService(conn, extractor, compiler, sequenceTimes(
		time.Date(2026, 6, 23, 12, 0, 0, 0, time.UTC),
		time.Date(2026, 6, 23, 12, 0, 1, 0, time.UTC),
		time.Date(2026, 6, 23, 12, 0, 2, 0, time.UTC),
	))
	svc.newID = sequenceIDs("job-1", "claim-1")

	if _, err := svc.Ingest(ctx, "owner@example.com", "source", "Title", nil); err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	if processed, err := svc.ProcessNext(ctx); err != nil || !processed {
		t.Fatalf("ProcessNext = %v/%v, want true/nil", processed, err)
	}

	var subjectCount int
	if err := conn.QueryRowContext(ctx, `SELECT COUNT(*) FROM subjects`).Scan(&subjectCount); err != nil {
		t.Fatalf("count subjects: %v", err)
	}
	if subjectCount != 1 {
		t.Fatalf("subject count = %d, want no duplicate minted for alias", subjectCount)
	}
	claims, _, err := NewClaimStore(conn).ListBySubject(ctx, "subject-survivor", pageParamsAll())
	if err != nil {
		t.Fatalf("ListBySubject survivor: %v", err)
	}
	if len(claims) != 1 || claims[0].Body != "Former Name shipped the release." {
		t.Fatalf("survivor claims = %+v, want alias claim on survivor", claims)
	}
	if len(compiler.subjects) != 1 || compiler.subjects[0].ID != "subject-survivor" {
		t.Fatalf("compiled subjects = %+v, want survivor", compiler.subjects)
	}
	if _, err := NewPageStore(conn).GetBySubject(ctx, "subject-survivor"); err != nil {
		t.Fatalf("GetBySubject survivor: %v", err)
	}
	if _, err := NewSubjectStore(conn).GetByNormName(ctx, "Former Name"); !errors.Is(err, sql.ErrNoRows) {
		t.Fatalf("alias name subject lookup err = %v, want sql.ErrNoRows", err)
	}
}

func pageParamsAll() page.Params {
	return page.Params{Limit: page.MaxLimit}
}
