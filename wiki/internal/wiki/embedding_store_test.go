package wiki

import (
	"context"
	"reflect"
	"testing"
)

func TestEncodeDecodeVecRoundTripsFloat32Values(t *testing.T) {
	// R-9OCK-FJK1
	vec := []float32{-1, 0, 0.25, 1.5, 3.4028235e+38}

	encoded := encodeVec(vec)
	if got, want := len(encoded), len(vec)*4; got != want {
		t.Fatalf("len(encodeVec) = %d, want %d", got, want)
	}
	decoded, err := decodeVec(encoded)
	if err != nil {
		t.Fatalf("decodeVec: %v", err)
	}
	if !reflect.DeepEqual(decoded, vec) {
		t.Fatalf("decodeVec(encodeVec(vec)) = %#v, want %#v", decoded, vec)
	}
}

func TestDecodeVecErrorsOnNonFloat32Length(t *testing.T) {
	// R-9PKG-TBAQ
	if _, err := decodeVec([]byte{0, 1, 2}); err == nil {
		t.Fatal("decodeVec length 3 err = nil, want error")
	}
}

func TestEmbeddingStoreUpsertReplacesSubjectEmbedding(t *testing.T) {
	// R-9QSD-731F
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	store := NewEmbeddingStore(conn)
	if err := store.Upsert(ctx, Embedding{
		SubjectID:   "subject-1",
		Model:       "model-a",
		Dims:        2,
		Vec:         []float32{0.25, 0.75},
		ContentHash: "hash-a",
		UpdatedAt:   101,
	}); err != nil {
		t.Fatalf("initial Upsert: %v", err)
	}
	replacement := Embedding{
		SubjectID:   "subject-1",
		Model:       "model-b",
		Dims:        3,
		Vec:         []float32{0.1, 0.2, 0.3},
		ContentHash: "hash-b",
		UpdatedAt:   202,
	}
	if err := store.Upsert(ctx, replacement); err != nil {
		t.Fatalf("replacement Upsert: %v", err)
	}

	got, err := store.LoadAll(ctx)
	if err != nil {
		t.Fatalf("LoadAll: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("LoadAll len = %d, want 1", len(got))
	}
	if !reflect.DeepEqual(got[0], replacement) {
		t.Fatalf("LoadAll[0] = %#v, want %#v", got[0], replacement)
	}
}

func TestEmbeddingStoreLoadAllReturnsEveryEmbedding(t *testing.T) {
	// R-9S09-KUS4
	ctx := context.Background()
	conn := migratedDB(t, ctx)
	defer conn.Close()

	store := NewEmbeddingStore(conn)
	want := []Embedding{
		{SubjectID: "subject-a", Model: "model-a", Dims: 2, Vec: []float32{0.6, 0.8}, ContentHash: "hash-a", UpdatedAt: 1000},
		{SubjectID: "subject-b", Model: "model-b", Dims: 1, Vec: []float32{1}, ContentHash: "hash-b", UpdatedAt: 1001},
	}
	for _, embedding := range []Embedding{want[1], want[0]} {
		if err := store.Upsert(ctx, embedding); err != nil {
			t.Fatalf("Upsert %s: %v", embedding.SubjectID, err)
		}
	}

	got, err := store.LoadAll(ctx)
	if err != nil {
		t.Fatalf("LoadAll: %v", err)
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("LoadAll = %#v, want %#v", got, want)
	}
}
