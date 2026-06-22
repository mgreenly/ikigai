package wiki

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
	"time"

	"appkit/logging"

	"wiki/internal/extract"
	"wiki/internal/llm"
)

const (
	JobPending = "pending"
	JobWorking = "working"
	JobDone    = "done"
	JobFailed  = "failed"
	JobAborted = "aborted"
)

type AbortResult struct {
	Aborted bool
	Status  string
}

// Extractor is the injected extract-stage dependency.
type Extractor interface {
	Extract(ctx context.Context, h extract.DocumentHeader, text string) ([]extract.ExtractedSubject, error)
}

// Compiler is the injected compile-stage dependency.
type Compiler interface {
	Compile(ctx context.Context, subject Subject, claims []Claim) (title, body string, err error)
}

// Service coordinates ingest jobs and the single background integration worker.
type Service struct {
	db        *sql.DB
	jobs      *JobStore
	subjects  *SubjectStore
	claims    *ClaimStore
	pages     *PageStore
	extractor Extractor
	compiler  Compiler
	now       func() time.Time
	newID     func() string
	wake      chan struct{}
}

// NewService builds the ingest service over the shared SQLite handle.
func NewService(db *sql.DB, extractor Extractor, compiler Compiler, now func() time.Time) *Service {
	if now == nil {
		now = time.Now
	}
	return &Service{
		db:        db,
		jobs:      NewJobStore(db),
		subjects:  NewSubjectStore(db),
		claims:    NewClaimStore(db),
		pages:     NewPageStore(db),
		extractor: extractor,
		compiler:  compiler,
		now:       now,
		newID:     logging.NewULID,
		wake:      make(chan struct{}, 1),
	}
}

// Ingest records a pending job and returns immediately with its handle.
func (s *Service) Ingest(ctx context.Context, owner, text, title string, tags []string) (string, error) {
	if s == nil {
		return "", fmt.Errorf("wiki: nil service")
	}
	jobID := s.newID()
	job := Job{
		ID:         jobID,
		Owner:      strings.TrimSpace(owner),
		SourceText: text,
		Title:      strings.TrimSpace(title),
		Tags:       append([]string(nil), tags...),
		Status:     JobPending,
		ReceivedAt: s.now(),
	}
	if err := s.jobs.InsertIngest(ctx, job); err != nil {
		return "", err
	}
	s.notify()
	return jobID, nil
}

// JobStatus returns the visible lifecycle state and produced subject ids.
func (s *Service) JobStatus(ctx context.Context, jobID string) (JobStatus, error) {
	if s == nil {
		return JobStatus{}, fmt.Errorf("wiki: nil service")
	}
	return s.jobs.Status(ctx, jobID)
}

func (s *Service) Abort(ctx context.Context, jobID string) (AbortResult, error) {
	if s == nil {
		return AbortResult{}, fmt.Errorf("wiki: nil service")
	}
	result, err := s.jobs.Abort(ctx, strings.TrimSpace(jobID), s.now())
	if err != nil {
		return AbortResult{}, err
	}
	if result.Aborted {
		s.notify()
	}
	return result, nil
}

// Subjects lists registry subjects, optionally filtered by type and name substring.
func (s *Service) Subjects(ctx context.Context, typ, nameContains string) ([]Subject, error) {
	if s == nil {
		return nil, fmt.Errorf("wiki: nil service")
	}
	return s.subjects.List(ctx, typ, nameContains)
}

// ClaimsBySubject returns the stored claims for an existing subject.
func (s *Service) ClaimsBySubject(ctx context.Context, subjectID string) ([]Claim, error) {
	if s == nil {
		return nil, fmt.Errorf("wiki: nil service")
	}
	if _, err := s.subjects.Get(ctx, strings.TrimSpace(subjectID)); err != nil {
		return nil, err
	}
	return s.claims.ListBySubject(ctx, strings.TrimSpace(subjectID))
}

// PageBySubject returns the compiled page for an existing subject.
func (s *Service) PageBySubject(ctx context.Context, subjectID string) (Page, error) {
	if s == nil {
		return Page{}, fmt.Errorf("wiki: nil service")
	}
	return s.pages.GetBySubject(ctx, strings.TrimSpace(subjectID))
}

// ProcessNext integrates one pending job, if any.
func (s *Service) ProcessNext(ctx context.Context) (bool, error) {
	if s == nil {
		return false, fmt.Errorf("wiki: nil service")
	}
	job, ok, err := s.jobs.ClaimPending(ctx, s.now())
	if err != nil || !ok {
		return ok, err
	}
	if err := s.integrate(ctx, job); err != nil {
		_, _ = s.jobs.FinishWorking(ctx, job.ID, JobFailed, s.now(), err.Error())
		return true, nil
	}
	if _, err := s.jobs.FinishWorking(ctx, job.ID, JobDone, s.now(), ""); err != nil {
		return true, err
	}
	return true, nil
}

// Wait blocks until Ingest nudges the worker or the context is canceled.
func (s *Service) Wait(ctx context.Context) error {
	if s == nil {
		<-ctx.Done()
		return ctx.Err()
	}
	select {
	case <-s.wake:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

func (s *Service) integrate(ctx context.Context, job Job) error {
	if s.extractor == nil {
		return fmt.Errorf("wiki: nil extractor")
	}
	if s.compiler == nil {
		return fmt.Errorf("wiki: nil compiler")
	}
	ctx = llm.WithJobID(ctx, job.ID)
	extracted, err := s.extractor.Extract(ctx, extract.DocumentHeader{
		Source:     "mcp:ingest_text",
		Title:      job.Title,
		Tags:       job.Tags,
		ReceivedAt: job.ReceivedAt,
	}, job.SourceText)
	if err != nil {
		return err
	}
	for _, item := range extracted {
		subject, err := s.subjectFor(ctx, item)
		if err != nil {
			return err
		}
		for _, body := range item.Claims {
			if err := s.claims.Save(ctx, Claim{
				ID:        s.newID(),
				SubjectID: subject.ID,
				JobID:     job.ID,
				Body:      strings.TrimSpace(body),
			}); err != nil {
				return err
			}
		}
		claims, err := s.claims.ListBySubject(ctx, subject.ID)
		if err != nil {
			return err
		}
		title, body, err := s.compiler.Compile(ctx, subject, claims)
		if err != nil {
			return err
		}
		if err := s.pages.Upsert(ctx, Page{
			ID:        subject.ID,
			SubjectID: subject.ID,
			Title:     title,
			Body:      body,
		}); err != nil {
			return err
		}
	}
	return nil
}

func (s *Service) subjectFor(ctx context.Context, item extract.ExtractedSubject) (Subject, error) {
	subject, err := s.subjects.GetByNormName(ctx, item.Name)
	if err == nil {
		return subject, nil
	}
	if err != sql.ErrNoRows {
		return Subject{}, err
	}
	subject = Subject{
		ID:       s.newID(),
		Name:     strings.TrimSpace(item.Name),
		NormName: normalize(item.Name),
		Type:     item.Type,
	}
	if err := s.subjects.Save(ctx, subject); err != nil {
		return Subject{}, err
	}
	return subject, nil
}

func (s *Service) notify() {
	select {
	case s.wake <- struct{}{}:
	default:
	}
}
