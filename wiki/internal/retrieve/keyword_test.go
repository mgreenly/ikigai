package retrieve

import (
	"context"
	"database/sql"
	"testing"

	"wiki/internal/db"
	wikidomain "wiki/internal/wiki"
)

func TestFTSPhraseQuotesTermsAndORsLiterals(t *testing.T) {
	// R-23RE-KCMW
	tests := map[string]string{
		`alpha beta`:             `"alpha" OR "beta"`,
		`alpha "beta" NEAR(foo)`: `"alpha" OR """beta""" OR "NEAR(foo)"`,
		`  `:                     "",
	}
	for input, want := range tests {
		if got := ftsPhrase(input); got != want {
			t.Fatalf("ftsPhrase(%q) = %q, want %q", input, got, want)
		}
	}
}

func TestKeywordRetrieverSearchReturnsRankedLimitedPageHits(t *testing.T) {
	// R-24ZA-Y4DL
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	subjects := wikidomain.NewSubjectStore(conn)
	for _, subject := range []wikidomain.Subject{
		{ID: "subject-alpha", Name: "Alpha Lab", NormName: "alpha-lab", Type: "entity"},
		{ID: "subject-beta", Name: "Beta Lab", NormName: "beta-lab", Type: "entity"},
		{ID: "subject-gamma", Name: "Gamma Lab", NormName: "gamma-lab", Type: "entity"},
	} {
		if err := subjects.Save(ctx, subject); err != nil {
			t.Fatalf("Save subject %s: %v", subject.ID, err)
		}
	}
	pages := wikidomain.NewPageStore(conn)
	for _, page := range []wikidomain.Page{
		{ID: "page-alpha", SubjectID: "subject-alpha", Title: "Alpha Lab", Body: "Tulsa alpha launch notes include Tulsa alpha telemetry."},
		{ID: "page-beta", SubjectID: "subject-beta", Title: "Beta Lab", Body: "Tulsa logistics mention the beta warehouse."},
		{ID: "page-gamma", SubjectID: "subject-gamma", Title: "Gamma Lab", Body: "Unrelated archive entry."},
	} {
		if err := pages.Upsert(ctx, page); err != nil {
			t.Fatalf("Upsert page %s: %v", page.ID, err)
		}
	}

	got, err := NewKeywordRetriever(conn).Search(ctx, `Tulsa OR ignored`, SearchLimits{Limit: 1})
	if err != nil {
		t.Fatalf("Search: %v", err)
	}
	if len(got.Hits) != 1 {
		t.Fatalf("hits = %+v, want exactly one hit capped by limit", got.Hits)
	}
	hit := got.Hits[0]
	if hit.PageID != "subject-alpha" || hit.Path != "entity/alpha-lab" || hit.Title != "Alpha Lab" {
		t.Fatalf("first hit = %+v, want ranked Alpha Lab page identity", hit)
	}
	if hit.Snippet == "" {
		t.Fatalf("first hit snippet is empty, want matched snippet: %+v", hit)
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
