package wiki

import (
	"context"
	"errors"
	"reflect"
	"testing"
	"time"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/db"
	"wiki/internal/extract"
)

func TestEmbedAndStoreUsesDocumentRoleAndUpdatesStoreAndCache(t *testing.T) {
	// R-6XNX-FNXO
	// R-6YVT-TFOD
	// R-703Q-77F2
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	cache := &recordingVectorCache{}
	embedder := &recordingPageEmbedder{vectors: [][]float32{{0.25, 0.75}}}
	svc := NewService(
		conn,
		nil,
		nil,
		clockAt(time.Date(2026, 6, 25, 12, 30, 0, 0, time.UTC)),
		WithPageEmbedder("embed-model", embedder),
		WithVectorCacheUpdater(cache.Upsert),
	)
	page := Page{
		ID:        "subject-1",
		SubjectID: "subject-1",
		Title:     "Acme Robotics",
		Body:      "Acme Robotics opened a Tulsa lab.",
	}

	if err := svc.embedAndStore(ctx, page); err != nil {
		t.Fatalf("embedAndStore: %v", err)
	}
	if len(embedder.inputs) != 1 || !reflect.DeepEqual(embedder.inputs[0], []string{page.Body}) {
		t.Fatalf("embed inputs = %#v, want page body only", embedder.inputs)
	}
	if len(embedder.roles) != 1 || embedder.roles[0] != agentkit.InputDocument {
		t.Fatalf("embed roles = %#v, want document role", embedder.roles)
	}

	embeddings, err := NewEmbeddingStore(conn).LoadAll(ctx)
	if err != nil {
		t.Fatalf("LoadAll: %v", err)
	}
	if len(embeddings) != 1 {
		t.Fatalf("embeddings len = %d, want 1", len(embeddings))
	}
	got := embeddings[0]
	if got.SubjectID != page.SubjectID || got.Model != "embed-model" || got.Dims != 2 ||
		got.ContentHash != pageFingerprint(page) || got.UpdatedAt != time.Date(2026, 6, 25, 12, 30, 0, 0, time.UTC).Unix() {
		t.Fatalf("embedding metadata = %+v, want current page fingerprint/model/dims/time", got)
	}
	if !reflect.DeepEqual(got.Vec, []float32{0.25, 0.75}) {
		t.Fatalf("embedding vec = %#v, want stored page vector", got.Vec)
	}

	if len(cache.entries) != 1 ||
		cache.entries[0].subjectID != page.SubjectID ||
		cache.entries[0].title != page.Title ||
		!reflect.DeepEqual(cache.entries[0].vec, []float32{0.25, 0.75}) {
		t.Fatalf("cache entries = %+v, want upserted page vector", cache.entries)
	}
}

func TestEmbedAndStoreOverwritesExistingPageVector(t *testing.T) {
	// R-6YVT-TFOD
	// R-703Q-77F2
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	store := NewEmbeddingStore(conn)
	if err := store.Upsert(ctx, Embedding{
		SubjectID:   "subject-1",
		Model:       "old-model",
		Dims:        3,
		Vec:         []float32{9, 8, 7},
		ContentHash: "old-fingerprint",
		UpdatedAt:   time.Date(2026, 6, 25, 11, 0, 0, 0, time.UTC).Unix(),
	}); err != nil {
		t.Fatalf("seed old embedding: %v", err)
	}

	cache := &recordingVectorCache{}
	embedder := &recordingPageEmbedder{vectors: [][]float32{{0.5, 0.25}}}
	svc := NewService(
		conn,
		nil,
		nil,
		clockAt(time.Date(2026, 6, 25, 12, 0, 0, 0, time.UTC)),
		WithPageEmbedder("new-model", embedder),
		WithVectorCacheUpdater(cache.Upsert),
	)
	page := Page{
		ID:        "subject-1",
		SubjectID: "subject-1",
		Title:     "Acme Robotics",
		Body:      "Acme Robotics opened a refreshed Tulsa lab.",
	}

	if err := svc.embedAndStore(ctx, page); err != nil {
		t.Fatalf("embedAndStore: %v", err)
	}

	embeddings, err := store.LoadAll(ctx)
	if err != nil {
		t.Fatalf("LoadAll: %v", err)
	}
	if len(embeddings) != 1 {
		t.Fatalf("embeddings len = %d, want one row for overwritten subject", len(embeddings))
	}
	got := embeddings[0]
	if got.SubjectID != page.SubjectID ||
		got.Model != "new-model" ||
		got.Dims != 2 ||
		got.ContentHash != pageFingerprint(page) ||
		got.UpdatedAt != time.Date(2026, 6, 25, 12, 0, 0, 0, time.UTC).Unix() ||
		!reflect.DeepEqual(got.Vec, []float32{0.5, 0.25}) {
		t.Fatalf("embedding = %+v, want overwritten current page vector", got)
	}
	if len(cache.entries) != 1 ||
		cache.entries[0].subjectID != page.SubjectID ||
		cache.entries[0].title != page.Title ||
		!reflect.DeepEqual(cache.entries[0].vec, []float32{0.5, 0.25}) {
		t.Fatalf("cache entries = %+v, want updated current page vector", cache.entries)
	}
}

func TestProcessNextEmbedsCommittedPageAfterIngest(t *testing.T) {
	// R-71BM-KZ5R
	// R-72JI-YQWG
	// R-73RF-CIN5
	ctx := context.Background()
	conns, cleanup := migratedEmbeddingConns(t, ctx)
	defer cleanup()

	embedder := &recordingPageEmbedder{
		vectors: [][]float32{{1, 0}},
		onEmbed: func(_ context.Context, inputs []string, _ agentkit.InputType) error {
			page, err := NewPageStore(conns.Read).GetBySubject(ctx, "subject-1")
			if err != nil {
				return err
			}
			if len(inputs) != 1 || inputs[0] != page.Body {
				return errors.New("embed input did not match committed page body")
			}
			return nil
		},
	}
	svc := NewService(
		conns,
		&recordingExtractor{batches: [][]extract.ExtractedSubject{{
			{
				Type:   "entity",
				Kind:   "company",
				Name:   "Acme Robotics",
				Claims: []string{"Acme Robotics opened a committed Tulsa lab."},
			},
		}}},
		&recordingCompiler{},
		sequenceTimes(
			time.Date(2026, 6, 25, 13, 0, 0, 0, time.UTC),
			time.Date(2026, 6, 25, 13, 0, 1, 0, time.UTC),
			time.Date(2026, 6, 25, 13, 0, 2, 0, time.UTC),
			time.Date(2026, 6, 25, 13, 0, 3, 0, time.UTC),
		),
		WithPageEmbedder("embed-model", embedder),
	)
	svc.newID = sequenceIDs("job-1", "subject-1", "claim-1")

	if _, err := svc.Ingest(ctx, "owner@example.com", "source", "Source", nil); err != nil {
		t.Fatalf("Ingest: %v", err)
	}
	processed, err := svc.ProcessNext(ctx)
	if err != nil {
		t.Fatalf("ProcessNext: %v", err)
	}
	if !processed {
		t.Fatal("ProcessNext processed = false, want true")
	}

	embeddings, err := NewEmbeddingStore(conns.Read).LoadAll(ctx)
	if err != nil {
		t.Fatalf("LoadAll: %v", err)
	}
	if len(embeddings) != 1 {
		t.Fatalf("embeddings len = %d, want 1", len(embeddings))
	}
	if embeddings[0].SubjectID != "subject-1" || !reflect.DeepEqual(embeddings[0].Vec, []float32{1, 0}) {
		t.Fatalf("embedding = %+v, want committed subject-1 vector", embeddings[0])
	}
	if embeddings[0].UpdatedAt != time.Date(2026, 6, 25, 13, 0, 3, 0, time.UTC).Unix() {
		t.Fatalf("updated_at = %d, want post-commit embedding time", embeddings[0].UpdatedAt)
	}
}

type recordingPageEmbedder struct {
	vectors [][]float32
	inputs  [][]string
	roles   []agentkit.InputType
	onEmbed func(context.Context, []string, agentkit.InputType) error
}

func (e *recordingPageEmbedder) Embed(ctx context.Context, inputs []string, role agentkit.InputType) (*agentkit.EmbedResult, error) {
	e.inputs = append(e.inputs, append([]string(nil), inputs...))
	e.roles = append(e.roles, role)
	if e.onEmbed != nil {
		if err := e.onEmbed(ctx, inputs, role); err != nil {
			return nil, err
		}
	}
	vec := []float32(nil)
	if len(e.vectors) > 0 {
		vec = append([]float32(nil), e.vectors[0]...)
		e.vectors = e.vectors[1:]
	}
	return &agentkit.EmbedResult{Vectors: [][]float32{vec}}, nil
}

func migratedEmbeddingConns(t *testing.T, ctx context.Context) (Conns, func()) {
	t.Helper()

	path := t.TempDir() + "/wiki.db"
	write, err := db.Open(path)
	if err != nil {
		t.Fatalf("Open writer: %v", err)
	}
	if err := db.Migrate(ctx, write); err != nil {
		write.Close()
		t.Fatalf("Migrate: %v", err)
	}
	read, err := db.OpenRead(path)
	if err != nil {
		write.Close()
		t.Fatalf("OpenRead: %v", err)
	}
	return Conns{Read: read, Write: write}, func() {
		read.Close()
		write.Close()
	}
}

type recordingVectorCache struct {
	entries []recordingVectorEntry
}

type recordingVectorEntry struct {
	subjectID string
	title     string
	vec       []float32
}

func (c *recordingVectorCache) Upsert(subjectID, title string, vec []float32) {
	c.entries = append(c.entries, recordingVectorEntry{
		subjectID: subjectID,
		title:     title,
		vec:       append([]float32(nil), vec...),
	})
}

var _ PageEmbedder = (*recordingPageEmbedder)(nil)
