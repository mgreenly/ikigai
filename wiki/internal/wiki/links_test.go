package wiki

import (
	"context"
	"strings"
	"testing"
)

func TestMentionsFindsWholeNormalizedSubjectNames(t *testing.T) {
	// R-ZUDC-NJIP
	others := []SubjectKeys{
		{
			Subject: Subject{ID: "subject-1", Name: "Cafe Noir", NormName: "cafe-noir", Type: "entity"},
			Keys:    []string{"cafe-noir"},
		},
		{
			Subject: Subject{ID: "subject-2", Name: "Tulsa Launch", NormName: "tulsa-launch", Type: "event"},
			Keys:    []string{"tulsa-launch"},
		},
	}

	got := Mentions("CAFE\u0301 NOIR announced plans after the meeting.", others)

	if len(got) != 1 || got[0].ID != "subject-1" {
		t.Fatalf("Mentions returned %+v, want only normalized Cafe Noir match", got)
	}
}

func TestMentionsRequiresAlphanumericBoundaries(t *testing.T) {
	// R-ZVL9-1B9E
	others := []SubjectKeys{
		{
			Subject: Subject{ID: "subject-1", Name: "Cat", NormName: "cat", Type: "entity"},
			Keys:    []string{"cat"},
		},
		{
			Subject: Subject{ID: "subject-2", Name: "Category Theory", NormName: "category-theory", Type: "concept"},
			Keys:    []string{"category-theory"},
		},
	}

	got := Mentions("The category theory note mentions a cat, but not concatenate.", others)

	if len(got) != 2 || got[0].ID != "subject-1" || got[1].ID != "subject-2" {
		t.Fatalf("Mentions returned %+v, want cat as whole word and category theory phrase", got)
	}
}

func TestMentionsReturnsCanonicalSubjectForAliasKeys(t *testing.T) {
	// R-1WP9-CLM9
	others := []SubjectKeys{
		{
			Subject: Subject{ID: "subject-1", Name: "Current Initiative", NormName: "current-initiative", Type: "concept"},
			Keys:    []string{"current-initiative", "project-lumen"},
		},
	}

	got := Mentions("The update only named Project Lumen.", others)

	if len(got) != 1 || got[0].ID != "subject-1" || got[0].Name != "Current Initiative" {
		t.Fatalf("Mentions returned %+v, want canonical Current Initiative from alias key", got)
	}
}

func TestPageWithLinksProjectsOutboundMentions(t *testing.T) {
	// R-ZWT5-F303
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	pages := NewPageStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-1", Name: "Acme Robotics", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-2", Name: "Tulsa Launch", Type: "event"})
	upsertPage(t, ctx, pages, Page{
		ID:        "page-1",
		SubjectID: "subject-1",
		Title:     "Acme Robotics",
		Body:      "Acme Robotics prepared for the Tulsa Launch.",
	})

	got, err := svc.PageWithLinks(ctx, "subject-1")
	if err != nil {
		t.Fatalf("PageWithLinks: %v", err)
	}
	if got.ID != "page-1" || got.Title != "Acme Robotics" {
		t.Fatalf("Linked page = %+v, want stored page", got.Page)
	}
	if len(got.Mentions) != 1 || got.Mentions[0] != (Ref{Path: "event/tulsa-launch", Name: "Tulsa Launch"}) {
		t.Fatalf("Mentions = %+v, want Tulsa Launch ref", got.Mentions)
	}
}

func TestPageWithLinksOrdersOutboundMentionsByPath(t *testing.T) {
	// R-ZWT5-F303
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	pages := NewPageStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-1", Name: "Home Page", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-2", Name: "Alpha Entity", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-3", Name: "Zebra Concept", Type: "concept"})
	upsertPage(t, ctx, pages, Page{
		ID:        "page-1",
		SubjectID: "subject-1",
		Title:     "Home Page",
		Body:      "Home Page mentions Alpha Entity and Zebra Concept.",
	})

	got, err := svc.PageWithLinks(ctx, "subject-1")
	if err != nil {
		t.Fatalf("PageWithLinks: %v", err)
	}
	want := []Ref{
		{Path: "concept/zebra-concept", Name: "Zebra Concept"},
		{Path: "entity/alpha-entity", Name: "Alpha Entity"},
	}
	if len(got.Mentions) != len(want) {
		t.Fatalf("Mentions = %+v, want %+v", got.Mentions, want)
	}
	for i := range want {
		if got.Mentions[i] != want[i] {
			t.Fatalf("Mentions = %+v, want %+v", got.Mentions, want)
		}
	}
}

func TestPageWithLinksProjectsInboundFromPagedSubjectsOnly(t *testing.T) {
	// R-ZY11-SUQS
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	pages := NewPageStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-1", Name: "Acme Robotics", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-2", Name: "Tulsa Launch", Type: "event"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-3", Name: "No Page Lab", Type: "entity"})
	upsertPage(t, ctx, pages, Page{ID: "page-1", SubjectID: "subject-1", Title: "Acme Robotics", Body: "Acme Robotics overview."})
	upsertPage(t, ctx, pages, Page{ID: "page-2", SubjectID: "subject-2", Title: "Tulsa Launch", Body: "Tulsa Launch was run by Acme Robotics."})

	got, err := svc.PageWithLinks(ctx, "subject-1")
	if err != nil {
		t.Fatalf("PageWithLinks: %v", err)
	}
	if len(got.MentionedBy) != 1 || got.MentionedBy[0] != (Ref{Path: "event/tulsa-launch", Name: "Tulsa Launch"}) {
		t.Fatalf("MentionedBy = %+v, want only paged Tulsa Launch ref", got.MentionedBy)
	}
}

func TestPageWithLinksProjectsAliasAwareInboundAndOutbound(t *testing.T) {
	// R-1Z52-453N
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	pages := NewPageStore(conn)
	aliases := NewAliasStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-1", Name: "Acme Robotics", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-2", Name: "Current Initiative", Type: "concept"})
	if err := aliases.Insert(ctx, Alias{
		Name:      "Former Lab",
		SubjectID: "subject-1",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-24T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert alias for subject-1: %v", err)
	}
	if err := aliases.Insert(ctx, Alias{
		Name:      "Project Lumen",
		SubjectID: "subject-2",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-24T12:01:00Z",
	}); err != nil {
		t.Fatalf("Insert alias for subject-2: %v", err)
	}
	upsertPage(t, ctx, pages, Page{
		ID:        "page-1",
		SubjectID: "subject-1",
		Title:     "Acme Robotics",
		Body:      "Former Lab announced Project Lumen.",
	})
	upsertPage(t, ctx, pages, Page{
		ID:        "page-2",
		SubjectID: "subject-2",
		Title:     "Current Initiative",
		Body:      "Current Initiative depends on Former Lab.",
	})

	got, err := svc.PageWithLinks(ctx, "subject-1")
	if err != nil {
		t.Fatalf("PageWithLinks: %v", err)
	}
	if len(got.Mentions) != 1 || got.Mentions[0] != (Ref{Path: "concept/current-initiative", Name: "Current Initiative"}) {
		t.Fatalf("Mentions = %+v, want canonical Current Initiative ref from alias", got.Mentions)
	}
	if len(got.MentionedBy) != 1 || got.MentionedBy[0] != (Ref{Path: "concept/current-initiative", Name: "Current Initiative"}) {
		t.Fatalf("MentionedBy = %+v, want canonical Current Initiative ref from alias", got.MentionedBy)
	}
}

func TestMentionsInReturnsSubjectHrefRefsForWholeMatches(t *testing.T) {
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-cat", Name: "Cat", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-category", Name: "Category Theory", Type: "concept"})

	got, err := svc.MentionsIn(ctx, "Category theory mentions a cat, not concatenate.")
	if err != nil {
		t.Fatalf("MentionsIn: %v", err)
	}
	want := []Ref{
		{Path: "concept/category-theory", Name: "Category Theory"},
		{Path: "entity/cat", Name: "Cat"},
	}
	if !sameRefs(got, want) {
		t.Fatalf("MentionsIn = %+v, want whole-match subject refs %+v", got, want)
	}
}

func TestMentionsInResolvesAliasKeysToCanonicalSubject(t *testing.T) {
	// R-AWIU-P1OK
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	aliases := NewAliasStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-lumen", Name: "Current Initiative", Type: "concept"})
	if err := aliases.Insert(ctx, Alias{
		Name:      "Project Lumen",
		SubjectID: "subject-lumen",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-25T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert alias: %v", err)
	}

	got, err := svc.MentionsIn(ctx, "The answer names Project Lumen, never the canonical title.")
	if err != nil {
		t.Fatalf("MentionsIn: %v", err)
	}
	want := []Ref{{Path: "concept/current-initiative", Name: "Current Initiative"}}
	if !sameRefs(got, want) {
		t.Fatalf("MentionsIn = %+v, want alias to canonical ref %+v", got, want)
	}
}

func TestMentionsInOrdersAndDedupesWebRefs(t *testing.T) {
	// R-AXQR-2TF9
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	aliases := NewAliasStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-z", Name: "Zeta Lab", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-a", Name: "Alpha Plan", Type: "concept"})
	if err := aliases.Insert(ctx, Alias{
		Name:      "Project A",
		SubjectID: "subject-a",
		CreatedBy: "owner@example.com",
		CreatedAt: "2026-06-25T12:00:00Z",
	}); err != nil {
		t.Fatalf("Insert alias: %v", err)
	}

	got, err := svc.MentionsIn(ctx, "Zeta Lab discussed Alpha Plan, Project A, and Zeta Lab again.")
	if err != nil {
		t.Fatalf("MentionsIn: %v", err)
	}
	want := []Ref{
		{Path: "concept/alpha-plan", Name: "Alpha Plan"},
		{Path: "entity/zeta-lab", Name: "Zeta Lab"},
	}
	if !sameRefs(got, want) {
		t.Fatalf("MentionsIn = %+v, want sorted deduped web refs %+v", got, want)
	}
}

func TestRenderFooterAppendsMarkdownLinks(t *testing.T) {
	// R-ZZ8Y-6MHH
	got := RenderFooter("Page body.\n", []Ref{
		{Path: "event/tulsa-launch", Name: "Tulsa Launch"},
	}, []Ref{
		{Path: "entity/acme-robotics", Name: "Acme Robotics"},
	})

	for _, want := range []string{
		"Page body.\n\n---\n\n## Links",
		"### Mentions\n- [Tulsa Launch](event/tulsa-launch)",
		"### Mentioned by\n- [Acme Robotics](entity/acme-robotics)",
	} {
		if !strings.Contains(got, want) {
			t.Fatalf("RenderFooter output:\n%s\nmissing %q", got, want)
		}
	}
}

func TestRenderFooterOrdersAndDedupesMarkdownLinks(t *testing.T) {
	// R-ZZ8Y-6MHH
	got := RenderFooter("Page body.", []Ref{
		{Path: "entity/zeta-lab", Name: "Zeta Lab"},
		{Path: "concept/alpha-plan", Name: "Alpha Plan"},
		{Path: "entity/zeta-lab", Name: "Zeta Lab"},
	}, nil)

	alpha := "- [Alpha Plan](concept/alpha-plan)"
	zeta := "- [Zeta Lab](entity/zeta-lab)"
	if strings.Count(got, zeta) != 1 {
		t.Fatalf("RenderFooter output:\n%s\nwant exactly one %q", got, zeta)
	}
	if !strings.Contains(got, alpha) || strings.Index(got, alpha) > strings.Index(got, zeta) {
		t.Fatalf("RenderFooter output:\n%s\nwant refs ordered by path", got)
	}
}

func TestPageWithLinksExcludesThePageSubjectFromOutboundMentions(t *testing.T) {
	// R-00GU-KE86
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()
	svc := NewService(conn, nil, nil, nil)
	subjects := NewSubjectStore(conn)
	pages := NewPageStore(conn)

	saveSubject(t, ctx, subjects, Subject{ID: "subject-1", Name: "Acme Robotics", Type: "entity"})
	saveSubject(t, ctx, subjects, Subject{ID: "subject-2", Name: "Mira Patel", Type: "entity"})
	upsertPage(t, ctx, pages, Page{
		ID:        "page-1",
		SubjectID: "subject-1",
		Title:     "Acme Robotics",
		Body:      "Acme Robotics hired Mira Patel at Acme Robotics.",
	})

	got, err := svc.PageWithLinks(ctx, "subject-1")
	if err != nil {
		t.Fatalf("PageWithLinks: %v", err)
	}
	if len(got.Mentions) != 1 || got.Mentions[0] != (Ref{Path: "entity/mira-patel", Name: "Mira Patel"}) {
		t.Fatalf("Mentions = %+v, want only Mira Patel and no self link", got.Mentions)
	}
}

func saveSubject(t *testing.T, ctx context.Context, store *SubjectStore, subject Subject) {
	t.Helper()
	if err := store.Save(ctx, subject); err != nil {
		t.Fatalf("Save subject %s: %v", subject.ID, err)
	}
}

func upsertPage(t *testing.T, ctx context.Context, store *PageStore, page Page) {
	t.Helper()
	if err := store.Upsert(ctx, page); err != nil {
		t.Fatalf("Upsert page %s: %v", page.ID, err)
	}
}

func sameRefs(got, want []Ref) bool {
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
