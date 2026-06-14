package page

import (
	"context"
	"database/sql"
	"path/filepath"
	"reflect"
	"testing"

	"wiki/internal/db"

	_ "modernc.org/sqlite"
)

// newTestDB stands up a migrated in-temp-dir SQLite DB for the registry reads.
func newTestDB(t *testing.T) *sql.DB {
	t.Helper()
	dir := t.TempDir()
	conn, err := db.Open(filepath.Join(dir, "wiki.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	if err := db.Migrate(context.Background(), conn); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return conn
}

// insertSubject inserts a subjects row.
func insertSubject(t *testing.T, conn *sql.DB, id, typ, canonical string) {
	t.Helper()
	_, err := conn.Exec(
		`INSERT INTO subjects (id, type, kind, canonical_name, created_by_run) VALUES (?, ?, '', ?, 'run-1')`,
		id, typ, canonical)
	if err != nil {
		t.Fatalf("insert subject %s: %v", id, err)
	}
}

// insertAlias inserts an aliases row (norm is normalized by the caller's intent;
// here we normalize via Normalize to mirror the production write path).
func insertAlias(t *testing.T, conn *sql.DB, typ, name, subjectID string) {
	t.Helper()
	_, err := conn.Exec(
		`INSERT INTO aliases (type, norm, subject_id) VALUES (?, ?, ?)`,
		typ, Normalize(name), subjectID)
	if err != nil {
		t.Fatalf("insert alias %q: %v", name, err)
	}
}

// insertPage inserts a pages row AND its external-content pages_fts row (no
// triggers — the production commit syncs FTS by hand; tests mirror that).
func insertPage(t *testing.T, conn *sql.DB, subjectID, title, body string) {
	t.Helper()
	res, err := conn.Exec(
		`INSERT INTO pages (subject, title, body, version) VALUES (?, ?, ?, 1)`,
		subjectID, title, body)
	if err != nil {
		t.Fatalf("insert page %s: %v", subjectID, err)
	}
	rowid, err := res.LastInsertId()
	if err != nil {
		t.Fatalf("page rowid: %v", err)
	}
	if _, err := conn.Exec(
		`INSERT INTO pages_fts (rowid, title, body) VALUES (?, ?, ?)`,
		rowid, title, body); err != nil {
		t.Fatalf("insert pages_fts %s: %v", subjectID, err)
	}
}

func TestResolveByKeys(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	insertSubject(t, conn, "subj-A", TypeEntity, "Acme Corp")
	insertAlias(t, conn, TypeEntity, "Acme Corp", "subj-A")
	insertAlias(t, conn, TypeEntity, "ACME", "subj-A")

	insertSubject(t, conn, "subj-B", TypeEntity, "Beta Inc")
	insertAlias(t, conn, TypeEntity, "Beta Inc", "subj-B")

	// A same-norm name under a DIFFERENT type must not resolve across the type.
	insertSubject(t, conn, "subj-C", TypeConcept, "Acme Corp")
	insertAlias(t, conn, TypeConcept, "Acme Corp", "subj-C")

	cases := []struct {
		name string
		typ  Type
		keys []string
		want []string
	}{
		{"one id by name", TypeEntity, KeySet("Acme Corp", nil), []string{"subj-A"}},
		{"one id by alias", TypeEntity, KeySet("acme", nil), []string{"subj-A"}},
		{"zero ids", TypeEntity, KeySet("Nobody", nil), nil},
		{"many ids via bridging keys", TypeEntity, KeySet("Acme Corp", []string{"Beta Inc"}), []string{"subj-A", "subj-B"}},
		{"type-scoped", TypeConcept, KeySet("Acme Corp", nil), []string{"subj-C"}},
		{"empty keys", TypeEntity, nil, nil},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got, err := s.ResolveByKeys(ctx, tc.typ, tc.keys)
			if err != nil {
				t.Fatalf("ResolveByKeys: %v", err)
			}
			if !reflect.DeepEqual(got, tc.want) {
				t.Errorf("ResolveByKeys = %v, want %v", got, tc.want)
			}
		})
	}
}

func TestCandidates(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	insertSubject(t, conn, "subj-aws", TypeEntity, "AWS")
	insertPage(t, conn, "subj-aws", "AWS", "Amazon Cloud is the leading public cloud provider.")

	insertSubject(t, conn, "subj-gcp", TypeEntity, "GCP")
	insertPage(t, conn, "subj-gcp", "Google Cloud Platform", "GCP competes in the cloud market.")

	// A concept subject that must never appear among entity candidates.
	insertSubject(t, conn, "subj-cloud", TypeConcept, "Cloud Computing")
	insertPage(t, conn, "subj-cloud", "Cloud Computing", "Cloud computing delivers compute on demand.")

	t.Run("name lane matches a registry title", func(t *testing.T) {
		got, err := s.Candidates(ctx, TypeEntity, "AWS", "", 5)
		if err != nil {
			t.Fatalf("Candidates: %v", err)
		}
		if len(got) != 1 || got[0].SubjectID != "subj-aws" {
			t.Fatalf("name lane = %+v, want [subj-aws]", got)
		}
		if got[0].CanonicalName != "AWS" {
			t.Errorf("CanonicalName = %q, want AWS", got[0].CanonicalName)
		}
	})

	t.Run("body lane catches a zero-token-overlap synonym", func(t *testing.T) {
		// "Amazon Cloud" shares no token with the title "AWS" but appears in body.
		got, err := s.Candidates(ctx, TypeEntity, "", "Amazon Cloud", 5)
		if err != nil {
			t.Fatalf("Candidates: %v", err)
		}
		if len(got) != 1 || got[0].SubjectID != "subj-aws" {
			t.Fatalf("body lane = %+v, want [subj-aws]", got)
		}
	})

	t.Run("type scope excludes other-typed pages", func(t *testing.T) {
		got, err := s.Candidates(ctx, TypeEntity, "Cloud", "cloud computing", 5)
		if err != nil {
			t.Fatalf("Candidates: %v", err)
		}
		for _, c := range got {
			if c.SubjectID == "subj-cloud" {
				t.Errorf("concept subject leaked into entity candidates: %+v", got)
			}
		}
	})

	t.Run("dedup across lanes", func(t *testing.T) {
		// Both lanes hit subj-aws; it must appear exactly once.
		got, err := s.Candidates(ctx, TypeEntity, "AWS", "Amazon Cloud provider", 5)
		if err != nil {
			t.Fatalf("Candidates: %v", err)
		}
		count := 0
		for _, c := range got {
			if c.SubjectID == "subj-aws" {
				count++
			}
		}
		if count != 1 {
			t.Errorf("subj-aws appeared %d times, want 1: %+v", count, got)
		}
	})

	t.Run("free text with FTS operator chars is safe", func(t *testing.T) {
		// Raw text containing FTS5 operator characters must not error.
		_, err := s.Candidates(ctx, TypeEntity, `AWS OR "drop table" * (x)`, "", 5)
		if err != nil {
			t.Fatalf("Candidates with operator chars: %v", err)
		}
	})

	t.Run("zero limit returns nothing", func(t *testing.T) {
		got, err := s.Candidates(ctx, TypeEntity, "AWS", "", 0)
		if err != nil {
			t.Fatalf("Candidates: %v", err)
		}
		if got != nil {
			t.Errorf("limit 0 = %+v, want nil", got)
		}
	})
}
