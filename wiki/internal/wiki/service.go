package wiki

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
	"sync"
	"time"

	"appkit/logging"

	"wiki/internal/extract"
	"wiki/internal/llm"
	"wiki/internal/page"
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
	write     *sql.DB
	jobs      *JobStore
	subjects  *SubjectStore
	claims    *ClaimStore
	pages     *PageStore
	extractor Extractor
	compiler  Compiler
	now       func() time.Time
	newID     func() string
	wake      chan struct{}
	mu        sync.Mutex
	cancels   map[string]*jobCancel
}

type jobCancel struct {
	cancel context.CancelFunc
}

// NewService builds the ingest service over wiki's read/write SQLite handles.
func NewService(db any, extractor Extractor, compiler Compiler, now func() time.Time) *Service {
	if now == nil {
		now = time.Now
	}
	c := mustConns(db)
	return &Service{
		write:     c.Write,
		jobs:      NewJobStore(c),
		subjects:  NewSubjectStore(c.Read),
		claims:    NewClaimStore(c.Read),
		pages:     NewPageStore(c.Read),
		extractor: extractor,
		compiler:  compiler,
		now:       now,
		newID:     logging.NewULID,
		wake:      make(chan struct{}, 1),
		cancels:   map[string]*jobCancel{},
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
		s.cancelJob(strings.TrimSpace(jobID))
		s.notify()
	}
	return result, nil
}

func (s *Service) Rerun(ctx context.Context, jobID string) (RerunResult, error) {
	if s == nil {
		return RerunResult{}, fmt.Errorf("wiki: nil service")
	}
	result, err := s.jobs.Rerun(ctx, strings.TrimSpace(jobID))
	if err != nil {
		return result, err
	}
	if result.Requeued {
		s.notify()
	}
	return result, nil
}

// Subjects lists registry subjects, optionally filtered by type and name substring.
func (s *Service) Subjects(ctx context.Context, typ, nameContains string) ([]Subject, error) {
	if s == nil {
		return nil, fmt.Errorf("wiki: nil service")
	}
	return listAllSubjects(ctx, s.subjects, typ, nameContains)
}

// ClaimsBySubject returns the stored claims for an existing subject.
func (s *Service) ClaimsBySubject(ctx context.Context, subjectID string) ([]Claim, error) {
	if s == nil {
		return nil, fmt.Errorf("wiki: nil service")
	}
	if _, err := s.subjects.Get(ctx, strings.TrimSpace(subjectID)); err != nil {
		return nil, err
	}
	return listAllClaims(ctx, s.claims, strings.TrimSpace(subjectID))
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
	jobCtx, cancel := context.WithCancel(ctx)
	registeredCancel := s.registerJobCancel(job.ID, cancel)
	defer s.unregisterJobCancel(job.ID, registeredCancel)
	if err := s.integrate(jobCtx, job); err != nil {
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

	tx, err := s.write.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	subjects := NewSubjectStore(tx)
	claims := NewClaimStore(tx)
	pages := NewPageStore(tx)
	affected, err := s.affectedSubjects(ctx, subjects, claims, job.ID)
	if err != nil {
		return err
	}
	if err := claims.DeleteByJob(ctx, job.ID); err != nil {
		return err
	}
	for _, item := range extracted {
		subject, err := s.subjectFor(ctx, subjects, item)
		if err != nil {
			return err
		}
		affected[subject.ID] = subject
		for _, body := range item.Claims {
			if err := claims.Save(ctx, Claim{
				ID:        s.newID(),
				SubjectID: subject.ID,
				JobID:     job.ID,
				Body:      strings.TrimSpace(body),
			}); err != nil {
				return err
			}
		}
	}
	for _, subject := range affected {
		subjectClaims, err := listAllClaims(ctx, claims, subject.ID)
		if err != nil {
			return err
		}
		if len(subjectClaims) == 0 {
			if err := pages.DeleteBySubject(ctx, subject.ID); err != nil {
				return err
			}
			continue
		}
		title, body, err := s.compiler.Compile(ctx, subject, subjectClaims)
		if err != nil {
			return err
		}
		if err := pages.Upsert(ctx, Page{
			ID:        subject.ID,
			SubjectID: subject.ID,
			Title:     title,
			Body:      body,
		}); err != nil {
			return err
		}
	}
	return tx.Commit()
}

func listAllSubjects(ctx context.Context, store *SubjectStore, typ, nameContains string) ([]Subject, error) {
	var out []Subject
	params := page.Params{Limit: page.MaxLimit}
	for {
		subjects, next, err := store.List(ctx, typ, nameContains, params)
		if err != nil {
			return nil, err
		}
		out = append(out, subjects...)
		if next == "" {
			return out, nil
		}
		params.Cursor = next
	}
}

func listAllClaims(ctx context.Context, store *ClaimStore, subjectID string) ([]Claim, error) {
	var out []Claim
	params := page.Params{Limit: page.MaxLimit}
	for {
		claims, next, err := store.ListBySubject(ctx, subjectID, params)
		if err != nil {
			return nil, err
		}
		out = append(out, claims...)
		if next == "" {
			return out, nil
		}
		params.Cursor = next
	}
}

func (s *Service) affectedSubjects(ctx context.Context, subjects *SubjectStore, claims *ClaimStore, jobID string) (map[string]Subject, error) {
	affected := map[string]Subject{}
	ids, err := claims.SubjectIDsByJob(ctx, jobID)
	if err != nil {
		return nil, err
	}
	for _, id := range ids {
		subject, err := subjects.Get(ctx, id)
		if err != nil {
			return nil, err
		}
		affected[subject.ID] = subject
	}
	return affected, nil
}

func (s *Service) subjectFor(ctx context.Context, subjects *SubjectStore, item extract.ExtractedSubject) (Subject, error) {
	subject, err := subjects.GetByNormName(ctx, item.Name)
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
	if err := subjects.Save(ctx, subject); err != nil {
		return Subject{}, err
	}
	return subject, nil
}

func (s *Service) registerJobCancel(jobID string, cancel context.CancelFunc) *jobCancel {
	registered := &jobCancel{cancel: cancel}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.cancels[jobID] = registered
	return registered
}

func (s *Service) unregisterJobCancel(jobID string, registered *jobCancel) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.cancels[jobID] == registered {
		delete(s.cancels, jobID)
	}
	registered.cancel()
}

func (s *Service) cancelJob(jobID string) {
	s.mu.Lock()
	registered := s.cancels[jobID]
	s.mu.Unlock()
	if registered != nil {
		registered.cancel()
	}
}

func (s *Service) notify() {
	select {
	case s.wake <- struct{}{}:
	default:
	}
}
