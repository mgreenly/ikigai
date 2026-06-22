package wiki

import (
	"context"
	"database/sql"
	"strings"
	"time"

	"wiki/internal/llm"
	"wiki/internal/page"
)

type CallRecord = llm.CallRecord

// LLMCallStore persists LLM provider-call footprints.
type LLMCallStore struct {
	read  *sql.DB
	write *sql.DB
}

func NewLLMCallStore(db any) *LLMCallStore {
	c := mustConns(db)
	return &LLMCallStore{read: c.Read, write: c.Write}
}

func (s *LLMCallStore) Record(ctx context.Context, rec llm.CallRecord) error {
	_, err := s.write.ExecContext(ctx, `
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

type LLMCallFilter struct {
	JobID, Stage string
	Since, Until time.Time
}

func (s *LLMCallStore) List(ctx context.Context, f LLMCallFilter, p page.Params) ([]CallRecord, string, error) {
	cursor, err := decodeCursor(p.Cursor, 2)
	if err != nil {
		return nil, "", err
	}
	limit := p.ResolvedLimit()
	jobID := strings.TrimSpace(f.JobID)
	stage := strings.TrimSpace(f.Stage)
	since := formatTime(f.Since)
	until := formatTime(f.Until)
	args := []any{jobID, jobID, stage, stage, since, since, until, until}
	query := `
		SELECT id, stage, job_id, attempt, provider, model, params, request,
		       response, usage, err, started_at, ended_at
		FROM llm_calls
		WHERE (? = '' OR job_id = ?)
		  AND (? = '' OR stage = ?)
		  AND (? = '' OR started_at >= ?)
		  AND (? = '' OR started_at <= ?)`
	if len(cursor) > 0 {
		query += `
		  AND (started_at > ? OR (started_at = ? AND id > ?))`
		args = append(args, cursor[0], cursor[0], cursor[1])
	}
	query += `
		ORDER BY started_at, id
		LIMIT ?`
	args = append(args, limit+1)
	rows, err := s.read.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, "", err
	}
	defer rows.Close()

	var calls []CallRecord
	for rows.Next() {
		var call CallRecord
		var startedAt, endedAt string
		if err := rows.Scan(
			&call.ID,
			&call.Stage,
			&call.JobID,
			&call.Attempt,
			&call.Provider,
			&call.Model,
			&call.Params,
			&call.Request,
			&call.Response,
			&call.Usage,
			&call.Err,
			&startedAt,
			&endedAt,
		); err != nil {
			return nil, "", err
		}
		call.StartedAt = parseStoredTime(startedAt)
		call.EndedAt = parseStoredTime(endedAt)
		calls = append(calls, call)
	}
	if err := rows.Err(); err != nil {
		return nil, "", err
	}
	return pageCalls(calls, limit), nextCallCursor(calls, limit), nil
}
