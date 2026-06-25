package wiki

import (
	"context"
	"fmt"
	"strings"

	agentkit "github.com/ikigenba/agentkit"
)

// PageEmbedder embeds compiled wiki page bodies.
type PageEmbedder interface {
	Embed(ctx context.Context, inputs []string, role agentkit.InputType) (*agentkit.EmbedResult, error)
}

// VectorCache updates the in-memory vector search cache for one subject.
type VectorCache func(subjectID, title string, vec []float32)

// ServiceOption configures optional service dependencies.
type ServiceOption func(*Service)

// WithPageEmbedder installs the embedder used to keep page vectors current.
func WithPageEmbedder(model string, embedder PageEmbedder) ServiceOption {
	return func(s *Service) {
		if s == nil {
			return
		}
		s.embedModel = strings.TrimSpace(model)
		s.pageEmbedder = embedder
	}
}

// WithVectorCacheUpdater installs the in-memory vector cache update hook.
func WithVectorCacheUpdater(update func(subjectID, title string, vec []float32)) ServiceOption {
	return func(s *Service) {
		if s == nil {
			return
		}
		s.vectorCache = update
	}
}

// WithVectorCacheRemover installs the in-memory vector cache removal hook.
func WithVectorCacheRemover(remove func(subjectID string)) ServiceOption {
	return func(s *Service) {
		if s == nil {
			return
		}
		s.vectorCacheRemove = remove
	}
}

// embedAndStore computes and stores the current vector for a compiled page.
func (s *Service) embedAndStore(ctx context.Context, p Page) error {
	if s == nil {
		return fmt.Errorf("wiki: nil service")
	}
	if s.pageEmbedder == nil {
		return nil
	}
	result, err := s.pageEmbedder.Embed(ctx, []string{p.Body}, agentkit.InputDocument)
	if err != nil {
		return err
	}
	if result == nil || len(result.Vectors) != 1 {
		return fmt.Errorf("wiki: page embedder returned %d vectors, want 1", vectorCount(result))
	}
	vec := append([]float32(nil), result.Vectors[0]...)
	embedding := Embedding{
		SubjectID:   p.SubjectID,
		Model:       s.embedModel,
		Dims:        len(vec),
		Vec:         vec,
		ContentHash: pageFingerprint(p),
		UpdatedAt:   s.now().Unix(),
	}
	if err := s.embeddings.Upsert(ctx, embedding); err != nil {
		return err
	}
	if s.vectorCache != nil {
		s.vectorCache(p.SubjectID, p.Title, vec)
	}
	return nil
}

func vectorCount(result *agentkit.EmbedResult) int {
	if result == nil {
		return 0
	}
	return len(result.Vectors)
}

func pageFingerprint(p Page) string {
	return hashText(p.Title + "\n\n" + p.Body)
}
