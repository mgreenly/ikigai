package page

import (
	"context"
	"testing"
)

func TestLookupResolvesWholePage(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	insertSubject(t, conn, "subj-A", TypeEntity, "Acme Corp")
	insertAlias(t, conn, TypeEntity, "Acme Corp", "subj-A")
	insertAlias(t, conn, TypeEntity, "ACME", "subj-A")
	insertPage(t, conn, "subj-A", "Acme Corp", "Acme Corp makes widgets. [01ARR]")

	// Exact name (normalized) resolves to the whole page.
	got, err := s.Lookup(ctx, "acme corp")
	if err != nil {
		t.Fatalf("lookup: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("lookup got %d pages, want 1", len(got))
	}
	if got[0].Subject != "subj-A" || got[0].Title != "Acme Corp" || got[0].Type != TypeEntity {
		t.Errorf("lookup page = %+v", got[0])
	}
	if got[0].Body == "" {
		t.Error("lookup should return the full page body")
	}

	// An alias resolves too.
	alias, err := s.Lookup(ctx, "ACME")
	if err != nil || len(alias) != 1 || alias[0].Subject != "subj-A" {
		t.Errorf("alias lookup = %v (err %v)", alias, err)
	}

	// An unknown name is no rows, not an error.
	none, err := s.Lookup(ctx, "nobody")
	if err != nil {
		t.Fatalf("lookup unknown: %v", err)
	}
	if len(none) != 0 {
		t.Errorf("unknown name returned %d pages", len(none))
	}
}

func TestReadWholePage(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	insertSubject(t, conn, "subj-A", TypeConcept, "RAG")
	insertPage(t, conn, "subj-A", "RAG", "Retrieval-augmented generation.")

	wp, ok, err := s.ReadWholePage(ctx, "subj-A")
	if err != nil || !ok {
		t.Fatalf("read whole page: ok=%v err=%v", ok, err)
	}
	if wp.Type != TypeConcept || wp.Title != "RAG" || wp.Body == "" {
		t.Errorf("whole page = %+v", wp)
	}

	// A subject with no page row reports ok=false (not an error).
	insertSubject(t, conn, "subj-B", TypeEntity, "Pageless")
	_, ok, err = s.ReadWholePage(ctx, "subj-B")
	if err != nil {
		t.Fatalf("read pageless: %v", err)
	}
	if ok {
		t.Error("pageless subject should report ok=false")
	}

	// A missing subject also reports ok=false.
	_, ok, err = s.ReadWholePage(ctx, "missing")
	if err != nil || ok {
		t.Errorf("missing subject: ok=%v err=%v", ok, err)
	}
}

func TestSearchPagesWholePageContract(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	insertSubject(t, conn, "subj-A", TypeEntity, "Acme Corp")
	insertPage(t, conn, "subj-A", "Acme Corp", "Acme Corp manufactures industrial widgets.")
	insertSubject(t, conn, "subj-B", TypeEntity, "Beta Inc")
	insertPage(t, conn, "subj-B", "Beta Inc", "Beta Inc sells gadgets to consumers.")

	hits, err := s.SearchPages(ctx, "widgets", 5)
	if err != nil {
		t.Fatalf("search: %v", err)
	}
	if len(hits) != 1 || hits[0].Subject != "subj-A" {
		t.Fatalf("search 'widgets' = %v", hits)
	}
	// A hit is the WHOLE page (title + type + body).
	if hits[0].Title != "Acme Corp" || hits[0].Type != TypeEntity || hits[0].Body == "" {
		t.Errorf("search hit not a whole page: %+v", hits[0])
	}

	// limit is honored.
	all, err := s.SearchPages(ctx, "Inc OR widgets", 1)
	if err != nil {
		t.Fatalf("search limit: %v", err)
	}
	if len(all) > 1 {
		t.Errorf("limit=1 returned %d hits", len(all))
	}

	// Empty/garbage query is no hits, not an error.
	empty, err := s.SearchPages(ctx, "   ", 5)
	if err != nil || len(empty) != 0 {
		t.Errorf("empty query: %v (err %v)", empty, err)
	}
}

func TestTimelineWindow(t *testing.T) {
	conn := newTestDB(t)
	s := NewStore(conn)
	ctx := context.Background()

	// Three event subjects across years, one non-event (must be excluded).
	mustExec(t, conn, `INSERT INTO subjects (id, type, kind, canonical_name, created_by_run, occurred_at) VALUES ('e1','event','launch','Launch A','r',?)`, "2023-05")
	mustExec(t, conn, `INSERT INTO subjects (id, type, kind, canonical_name, created_by_run, occurred_at) VALUES ('e2','event','launch','Launch B','r',?)`, "2024-03-15")
	mustExec(t, conn, `INSERT INTO subjects (id, type, kind, canonical_name, created_by_run, occurred_at) VALUES ('e3','event','launch','Launch C','r',?)`, "2025-01")
	mustExec(t, conn, `INSERT INTO subjects (id, type, kind, canonical_name, created_by_run) VALUES ('c1','concept','','Not an event','r')`)

	// [2024, 2024-12] should include only e2.
	got, err := s.Timeline(ctx, "2024", "2024-12", 0)
	if err != nil {
		t.Fatalf("timeline: %v", err)
	}
	if len(got) != 1 || got[0].Subject != "e2" {
		t.Fatalf("timeline 2024 window = %v", got)
	}
	if got[0].OccurredAt != "2024-03-15" || got[0].CanonicalName != "Launch B" {
		t.Errorf("timeline event = %+v", got[0])
	}

	// Open-ended (from only) includes 2024 and 2025, ascending.
	from, err := s.Timeline(ctx, "2024", "", 0)
	if err != nil {
		t.Fatalf("timeline open: %v", err)
	}
	if len(from) != 2 || from[0].Subject != "e2" || from[1].Subject != "e3" {
		t.Errorf("open-ended timeline = %v", from)
	}

	// Whole range, ascending order by occurred_at.
	all, err := s.Timeline(ctx, "", "", 0)
	if err != nil {
		t.Fatalf("timeline all: %v", err)
	}
	if len(all) != 3 || all[0].Subject != "e1" || all[2].Subject != "e3" {
		t.Errorf("full timeline = %v", all)
	}
}
