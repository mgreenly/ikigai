package wiki

import (
	"context"
	"database/sql"
	"fmt"
	"sort"
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

	plan, err := s.planIntegration(ctx, job, extracted)
	if err != nil {
		return err
	}

	tx, err := s.write.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	var status string
	if err := tx.QueryRowContext(ctx, `SELECT status FROM jobs WHERE id = ?`, job.ID).Scan(&status); err != nil {
		return err
	}
	if status != JobWorking {
		return nil
	}

	subjects := NewSubjectStore(tx)
	claims := NewClaimStore(tx)
	pages := NewPageStore(tx)
	if err := claims.DeleteByJob(ctx, job.ID); err != nil {
		return err
	}
	for _, subject := range plan.newSubjects {
		if err := subjects.Save(ctx, subject); err != nil {
			return err
		}
	}
	for _, claim := range plan.claims {
		if err := claims.Save(ctx, claim); err != nil {
			return err
		}
	}
	for _, page := range plan.pages {
		if page.delete {
			if err := pages.DeleteBySubject(ctx, page.subjectID); err != nil {
				return err
			}
			continue
		}
		if err := pages.Upsert(ctx, Page{
			ID:        page.subjectID,
			SubjectID: page.subjectID,
			Title:     page.title,
			Body:      page.body,
		}); err != nil {
			return err
		}
	}
	res, err := tx.ExecContext(ctx,
		`UPDATE jobs SET status = ?, finished_at = ?, error = '' WHERE id = ? AND status = ?`,
		JobDone, formatTime(s.now()), job.ID, JobWorking)
	if err != nil {
		return err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return err
	}
	if n == 0 {
		return nil
	}
	return tx.Commit()
}

type integrationPlan struct {
	newSubjects []Subject
	claims      []Claim
	pages       []plannedPage
}

type plannedPage struct {
	subjectID string
	title     string
	body      string
	delete    bool
}

func (s *Service) planIntegration(ctx context.Context, job Job, extracted []extract.ExtractedSubject) (integrationPlan, error) {
	affected, err := s.affectedSubjects(ctx, s.subjects, s.claims, job.ID)
	if err != nil {
		return integrationPlan{}, err
	}

	knownByNorm := map[string]Subject{}
	newByNorm := map[string]bool{}
	claimsBySubject := map[string][]Claim{}
	var plan integrationPlan
	for _, item := range extracted {
		subject, isNew, err := s.plannedSubject(ctx, knownByNorm, item)
		if err != nil {
			return integrationPlan{}, err
		}
		if isNew && !newByNorm[subject.NormName] {
			plan.newSubjects = append(plan.newSubjects, subject)
			newByNorm[subject.NormName] = true
		}
		affected[subject.ID] = subject
		for _, body := range item.Claims {
			claim := Claim{
				ID:        s.newID(),
				SubjectID: subject.ID,
				JobID:     job.ID,
				Body:      strings.TrimSpace(body),
			}
			plan.claims = append(plan.claims, claim)
			claimsBySubject[subject.ID] = append(claimsBySubject[subject.ID], claim)
		}
	}

	affectedSubjects := sortedSubjects(affected)
	for _, subject := range affectedSubjects {
		subjectClaims, err := s.plannedClaims(ctx, job.ID, subject.ID, claimsBySubject[subject.ID])
		if err != nil {
			return integrationPlan{}, err
		}
		if len(subjectClaims) == 0 {
			plan.pages = append(plan.pages, plannedPage{subjectID: subject.ID, delete: true})
			continue
		}
		title, body, err := s.compiler.Compile(ctx, subject, subjectClaims)
		if err != nil {
			return integrationPlan{}, err
		}
		plan.pages = append(plan.pages, plannedPage{
			subjectID: subject.ID,
			title:     title,
			body:      body,
		})
	}
	return plan, nil
}

func (s *Service) plannedSubject(ctx context.Context, known map[string]Subject, item extract.ExtractedSubject) (Subject, bool, error) {
	normName := normalize(item.Name)
	if subject, ok := known[normName]; ok {
		return subject, false, nil
	}
	subject, err := s.subjects.GetByNormName(ctx, item.Name)
	if err == nil {
		known[normName] = subject
		return subject, false, nil
	}
	if err != sql.ErrNoRows {
		return Subject{}, false, err
	}
	subject = Subject{
		ID:       s.newID(),
		Name:     strings.TrimSpace(item.Name),
		NormName: normName,
		Type:     item.Type,
	}
	known[normName] = subject
	return subject, true, nil
}

func (s *Service) plannedClaims(ctx context.Context, jobID, subjectID string, newClaims []Claim) ([]Claim, error) {
	claims, err := listAllClaims(ctx, s.claims, subjectID)
	if err != nil {
		return nil, err
	}
	out := claims[:0]
	for _, claim := range claims {
		if claim.JobID != jobID {
			out = append(out, claim)
		}
	}
	out = append(out, newClaims...)
	sort.Slice(out, func(i, j int) bool {
		return out[i].ID < out[j].ID
	})
	return out, nil
}

func sortedSubjects(subjects map[string]Subject) []Subject {
	out := make([]Subject, 0, len(subjects))
	for _, subject := range subjects {
		out = append(out, subject)
	}
	sort.Slice(out, func(i, j int) bool {
		return out[i].ID < out[j].ID
	})
	return out
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
