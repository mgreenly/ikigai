package wiki

import (
	"context"
	"strings"
	"testing"
)

func TestMentionsFindsWholeNormalizedSubjectNames(t *testing.T) {
	// R-ZUDC-NJIP
	others := []Subject{
		{ID: "subject-1", Name: "Cafe Noir", NormName: "cafe noir", Type: "entity"},
		{ID: "subject-2", Name: "Tulsa Launch", NormName: "tulsa launch", Type: "event"},
	}

	got := Mentions("CAFE\u0301 NOIR announced plans after the meeting.", others)

	if len(got) != 1 || got[0].ID != "subject-1" {
		t.Fatalf("Mentions returned %+v, want only normalized Cafe Noir match", got)
	}
}

func TestMentionsRequiresAlphanumericBoundaries(t *testing.T) {
	// R-ZVL9-1B9E
	others := []Subject{
		{ID: "subject-1", Name: "Cat", NormName: "cat", Type: "entity"},
		{ID: "subject-2", Name: "Category Theory", NormName: "category theory", Type: "concept"},
	}

	got := Mentions("The category theory note mentions a cat, but not concatenate.", others)

	if len(got) != 2 || got[0].ID != "subject-1" || got[1].ID != "subject-2" {
		t.Fatalf("Mentions returned %+v, want cat as whole word and category theory phrase", got)
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
