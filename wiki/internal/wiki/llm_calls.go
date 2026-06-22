package wiki

import (
	"context"
	"database/sql"

	"wiki/internal/llm"
)

// LLMCallStore persists LLM provider-call footprints.
type LLMCallStore struct {
	db *sql.DB
}

func NewLLMCallStore(db *sql.DB) *LLMCallStore {
	return &LLMCallStore{db: db}
}

func (s *LLMCallStore) Record(ctx context.Context, rec llm.CallRecord) error {
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO llm_calls (
			id, stage, job_id, attempt, provider, model, params, request,
			response, usage, err, started_at, ended_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		rec.ID,
		rec.Stage,
		rec.JobID,
		rec.Attempt,
		rec.Provider,
		rec.Model,
		rec.Params,
		rec.Request,
		rec.Response,
		rec.Usage,
		rec.Err,
		formatTime(rec.StartedAt),
		formatTime(rec.EndedAt),
	)
	return err
}
