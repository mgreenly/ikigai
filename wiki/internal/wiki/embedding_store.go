package wiki

import (
	"context"
	"database/sql"
	"fmt"
	"math"
)

// Embedding is a stored, normalized vector for a page subject.
type Embedding struct {
	SubjectID   string
	Model       string
	Dims        int
	Vec         []float32
	ContentHash string
	UpdatedAt   int64
}

// EmbeddingStore persists page embeddings for startup search-cache loading.
type EmbeddingStore struct {
	read  sqlStore
	write sqlStore
}

func NewEmbeddingStore(db any) *EmbeddingStore {
	switch v := db.(type) {
	case Conns:
		c := mustConns(v)
		return &EmbeddingStore{read: c.Read, write: c.Write}
	case *sql.DB:
		c := mustConns(v)
		return &EmbeddingStore{read: c.Read, write: c.Write}
	case sqlStore:
		return &EmbeddingStore{read: v, write: v}
	default:
		c := mustConns(db)
		return &EmbeddingStore{read: c.Read, write: c.Write}
	}
}

// Upsert writes or replaces the single embedding row for a subject.
func (s *EmbeddingStore) Upsert(ctx context.Context, e Embedding) error {
	_, err := s.write.ExecContext(ctx, `
		INSERT INTO page_embeddings (subject_id, model, dims, vec, content_hash, updated_at)
		VALUES (?, ?, ?, ?, ?, ?)
		ON CONFLICT(subject_id) DO UPDATE SET
			model = excluded.model,
			dims = excluded.dims,
			vec = excluded.vec,
			content_hash = excluded.content_hash,
			updated_at = excluded.updated_at`,
		e.SubjectID,
		e.Model,
		e.Dims,
		encodeVec(e.Vec),
		e.ContentHash,
		e.UpdatedAt,
	)
	return err
}

// Delete removes the stored embedding for one subject.
func (s *EmbeddingStore) Delete(ctx context.Context, subjectID string) error {
	_, err := s.write.ExecContext(ctx, `DELETE FROM page_embeddings WHERE subject_id = ?`, subjectID)
	return err
}

// LoadAll returns every stored embedding in stable subject order.
func (s *EmbeddingStore) LoadAll(ctx context.Context) ([]Embedding, error) {
	rows, err := s.read.QueryContext(ctx, `
		SELECT subject_id, model, dims, vec, content_hash, updated_at
		FROM page_embeddings
		ORDER BY subject_id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var embeddings []Embedding
	for rows.Next() {
		var e Embedding
		var blob []byte
		if err := rows.Scan(&e.SubjectID, &e.Model, &e.Dims, &blob, &e.ContentHash, &e.UpdatedAt); err != nil {
			return nil, err
		}
		e.Vec, err = decodeVec(blob)
		if err != nil {
			return nil, err
		}
		embeddings = append(embeddings, e)
	}
	return embeddings, rows.Err()
}

func encodeVec(v []float32) []byte {
	b := make([]byte, len(v)*4)
	for i, f := range v {
		u := math.Float32bits(f)
		j := i * 4
		b[j] = byte(u)
		b[j+1] = byte(u >> 8)
		b[j+2] = byte(u >> 16)
		b[j+3] = byte(u >> 24)
	}
	return b
}

func decodeVec(b []byte) ([]float32, error) {
	if len(b)%4 != 0 {
		return nil, fmt.Errorf("wiki: embedding vector blob length %d is not a multiple of 4", len(b))
	}
	v := make([]float32, len(b)/4)
	for i := range v {
		j := i * 4
		u := uint32(b[j]) | uint32(b[j+1])<<8 | uint32(b[j+2])<<16 | uint32(b[j+3])<<24
		v[i] = math.Float32frombits(u)
	}
	return v, nil
}
