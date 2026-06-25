package wiki

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"

	"appkit"
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
	write      *sql.DB
	jobs       *JobStore
	subjects   *SubjectStore
	aliases    *AliasStore
	resolver   *Resolver
	claims     *ClaimStore
	pages      *PageStore
	embeddings *EmbeddingStore
	merges     *SubjectMergeStore
	extractor  Extractor
	compiler   Compiler
	now        func() time.Time
	newID      func() string
	wake       chan struct{}
	mu         sync.Mutex
	cancels    map[string]*jobCancel
	mergeMu    sync.Mutex
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
		write:      c.Write,
		jobs:       NewJobStore(c),
		subjects:   NewSubjectStore(c.Read),
		aliases:    NewAliasStore(c.Read),
		resolver:   NewResolver(c.Read),
		claims:     NewClaimStore(c.Read),
		pages:      NewPageStore(c.Read),
		embeddings: NewEmbeddingStore(c),
		merges:     NewSubjectMergeStore(c.Read),
		extractor:  extractor,
		compiler:   compiler,
		now:        now,
		newID:      logging.NewULID,
		wake:       make(chan struct{}, 1),
		cancels:    map[string]*jobCancel{},
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

// MergeSubjects queues a background job that folds one subject into another.
func (s *Service) MergeSubjects(ctx context.Context, fromSubjectID, toSubjectID string) (string, error) {
	if s == nil {
		return "", fmt.Errorf("wiki: nil service")
	}
	jobID := s.newID()
	tx, err := s.write.BeginTx(ctx, nil)
	if err != nil {
		return "", err
	}
	defer tx.Rollback()

	receivedAt := s.now()
	owner := ""
	if id, ok := appkit.IdentityFrom(ctx); ok {
		owner = strings.TrimSpace(id.OwnerEmail)
	}
	_, err = tx.ExecContext(ctx, `
		INSERT INTO jobs (
			id, owner, source_text, title, tags, source_hash, status, received_at
		) VALUES (?, ?, '', 'subject merge', '[]', ?, ?, ?)`,
		jobID, owner, hashText(""), JobPending, formatTime(receivedAt))
	if err != nil {
		return "", err
	}
	if err := NewSubjectMergeStore(tx).Save(ctx, SubjectMerge{
		JobID:         jobID,
		FromSubjectID: strings.TrimSpace(fromSubjectID),
		ToSubjectID:   strings.TrimSpace(toSubjectID),
	}); err != nil {
		return "", err
	}
	if err := tx.Commit(); err != nil {
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

func (s *Service) RequeueWorking(ctx context.Context) (int, error) {
	if s == nil {
		return 0, fmt.Errorf("wiki: nil service")
	}
	n, err := s.jobs.RequeueWorking(ctx)
	if err != nil {
		return 0, err
	}
	if n > 0 {
		s.notify()
	}
	return n, nil
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
	err = s.processClaimed(jobCtx, job)
	if err != nil {
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

func (s *Service) processClaimed(ctx context.Context, job Job) error {
	if _, ok, err := s.mergeForJob(ctx, job.ID); err != nil {
		return err
	} else if ok {
		return s.mergeSubjects(ctx, job)
	}
	return s.integrate(ctx, job)
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
		if Normalize(item.Name) == "" {
			continue
		}
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

func (s *Service) mergeSubjects(ctx context.Context, job Job) error {
	if s.compiler == nil {
		return fmt.Errorf("wiki: nil compiler")
	}
	ctx = llm.WithJobID(ctx, job.ID)
	merge, ok, err := s.mergeForJob(ctx, job.ID)
	if err != nil {
		return err
	}
	if !ok {
		return fmt.Errorf("wiki: missing merge payload for job %s", job.ID)
	}

	s.mergeMu.Lock()
	defer s.mergeMu.Unlock()

	winner, err := s.subjects.Get(ctx, merge.ToSubjectID)
	if errors.Is(err, sql.ErrNoRows) {
		return s.finishStaleMerge(ctx, job.ID)
	}
	if err != nil {
		return err
	}
	loser, err := s.subjects.Get(ctx, merge.FromSubjectID)
	if errors.Is(err, sql.ErrNoRows) {
		return s.finishStaleMerge(ctx, job.ID)
	} else if err != nil {
		return err
	}

	combined, err := s.mergeClaims(ctx, merge.FromSubjectID, merge.ToSubjectID)
	if err != nil {
		return err
	}
	title, body, err := s.compiler.Compile(ctx, winner, combined)
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
	aliases := NewAliasStore(tx)
	if _, err := subjects.Get(ctx, merge.ToSubjectID); errors.Is(err, sql.ErrNoRows) {
		return finishDoneInTx(ctx, tx, job.ID, s.now())
	} else if err != nil {
		return err
	}
	loser, err = subjects.Get(ctx, merge.FromSubjectID)
	if errors.Is(err, sql.ErrNoRows) {
		return finishDoneInTx(ctx, tx, job.ID, s.now())
	} else if err != nil {
		return err
	}
	if err := pages.DeleteBySubject(ctx, merge.FromSubjectID); err != nil {
		return err
	}
	if err := claims.RepointSubject(ctx, merge.FromSubjectID, merge.ToSubjectID); err != nil {
		return err
	}
	if err := aliases.RepointSubject(ctx, merge.FromSubjectID, merge.ToSubjectID); err != nil {
		return err
	}
	if _, err := aliases.GetByNormName(ctx, loser.Name); errors.Is(err, sql.ErrNoRows) {
		if err := aliases.Insert(ctx, Alias{
			NormName:  Normalize(loser.Name),
			SubjectID: merge.ToSubjectID,
			Name:      loser.Name,
			CreatedBy: job.Owner,
			CreatedAt: formatTime(s.now()),
		}); err != nil {
			return err
		}
	} else if err != nil {
		return err
	}
	if err := subjects.Delete(ctx, merge.FromSubjectID); err != nil {
		return err
	}
	if err := pages.Upsert(ctx, Page{
		ID:        merge.ToSubjectID,
		SubjectID: merge.ToSubjectID,
		Title:     title,
		Body:      body,
	}); err != nil {
		return err
	}
	return finishDoneInTx(ctx, tx, job.ID, s.now())
}

func (s *Service) mergeForJob(ctx context.Context, jobID string) (SubjectMerge, bool, error) {
	merge, err := s.merges.GetByJob(ctx, jobID)
	if errors.Is(err, sql.ErrNoRows) {
		return SubjectMerge{}, false, nil
	}
	if err != nil {
		return SubjectMerge{}, false, err
	}
	return merge, true, nil
}

func (s *Service) mergeClaims(ctx context.Context, fromSubjectID, toSubjectID string) ([]Claim, error) {
	winnerClaims, err := listAllClaims(ctx, s.claims, toSubjectID)
	if err != nil {
		return nil, err
	}
	loserClaims, err := listAllClaims(ctx, s.claims, fromSubjectID)
	if err != nil {
		return nil, err
	}
	combined := append([]Claim(nil), winnerClaims...)
	for _, claim := range loserClaims {
		claim.SubjectID = toSubjectID
		combined = append(combined, claim)
	}
	sort.Slice(combined, func(i, j int) bool {
		return combined[i].ID < combined[j].ID
	})
	return combined, nil
}

func (s *Service) finishStaleMerge(ctx context.Context, jobID string) error {
	_, err := s.jobs.FinishWorking(ctx, jobID, JobDone, s.now(), "")
	return err
}

func finishDoneInTx(ctx context.Context, tx *sql.Tx, jobID string, finishedAt time.Time) error {
	res, err := tx.ExecContext(ctx,
		`UPDATE jobs SET status = ?, finished_at = ?, error = '' WHERE id = ? AND status = ?`,
		JobDone, formatTime(finishedAt), jobID, JobWorking)
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

func (s *Service) plannedSubject(ctx context.Context, known map[string]Subject, item extract.ExtractedSubject) (Subject, bool, error) {
	normName := Normalize(item.Name)
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
	subject, err = s.resolver.ResolveByName(ctx, item.Name)
	if err == nil {
		known[normName] = subject
		return subject, false, nil
	}
	if !errors.Is(err, ErrSubjectNotFound) {
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
