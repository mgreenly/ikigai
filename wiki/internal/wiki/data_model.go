package wiki

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
	"unicode"

	"golang.org/x/text/unicode/norm"

	"wiki/internal/page"
)

// Subject is a canonical entity, event, or concept in the wiki.
type Subject struct {
	ID       string
	Name     string
	NormName string
	Type     string
}

var (
	ErrSubjectNotFound = errors.New("wiki: subject not found")
	ErrAmbiguousPath   = errors.New("wiki: ambiguous subject path")
	ErrJobNotTerminal  = errors.New("wiki: job is not terminal")
	ErrInvalidCursor   = errors.New("wiki: invalid cursor")
)

// Path is the public type/slug identifier for a subject.
func Path(s Subject) string {
	return s.Type + "/" + slug(s.NormName)
}

func slug(normName string) string {
	var b strings.Builder
	hyphen := true
	for _, r := range normName {
		if r == '/' || unicode.IsSpace(r) {
			if !hyphen {
				b.WriteByte('-')
				hyphen = true
			}
			continue
		}
		b.WriteRune(r)
		hyphen = false
	}
	return strings.TrimSuffix(b.String(), "-")
}

// Claim is an extracted statement about a subject.
type Claim struct {
	ID        string
	SubjectID string
	JobID     string
	Body      string
}

// Page is a generated wiki page for a subject.
type Page struct {
	ID        string
	SubjectID string
	Title     string
	Body      string
}

// Job is a phase-1 wiki data-model job.
type Job struct {
	ID         string
	Owner      string
	SourceText string
	Title      string
	Tags       []string
	SourceHash string
	Status     string
	ReceivedAt time.Time
	StartedAt  time.Time
	FinishedAt time.Time
	Error      string
}

// JobStatus is the inspectable state of an ingest job.
type JobStatus struct {
	ID         string
	Status     string
	ReceivedAt time.Time
	StartedAt  *time.Time
	FinishedAt *time.Time
	Error      string
	Subjects   []string
}

type RerunResult struct {
	Requeued bool
	Status   string
}

func normalize(name string) string {
	s := norm.NFKC.String(name)
	s = strings.ToLower(s)
	s = strings.Join(strings.Fields(s), " ")
	return stripDiacritics(s)
}

// Normalize returns the public path-safe subject-name form.
func Normalize(name string) string {
	s := stripDiacritics(strings.ToLower(norm.NFKC.String(name)))
	var b strings.Builder
	hyphen := true
	for _, r := range s {
		if r >= 'a' && r <= 'z' || r >= '0' && r <= '9' {
			b.WriteRune(r)
			hyphen = false
			continue
		}
		if !hyphen {
			b.WriteByte('-')
			hyphen = true
		}
	}
	return strings.TrimSuffix(b.String(), "-")
}

func stripDiacritics(s string) string {
	var b strings.Builder
	for _, r := range norm.NFD.String(s) {
		if unicode.Is(unicode.Mn, r) {
			continue
		}
		b.WriteRune(r)
	}
	return norm.NFC.String(b.String())
}

// JobStore persists wiki jobs.
type JobStore struct {
	read  *sql.DB
	write *sql.DB
}

// Conns carries wiki's concurrent read pool and single-writer SQLite handles.
type Conns struct {
	Read  *sql.DB
	Write *sql.DB
}

func NewJobStore(db any) *JobStore {
	c := mustConns(db)
	return &JobStore{read: c.Read, write: c.Write}
}

func mustConns(db any) Conns {
	switch v := db.(type) {
	case Conns:
		if v.Read == nil {
			v.Read = v.Write
		}
		if v.Write == nil {
			v.Write = v.Read
		}
		if v.Read == nil || v.Write == nil {
			panic("wiki: nil database handle")
		}
		return v
	case *sql.DB:
		if v == nil {
			panic("wiki: nil database handle")
		}
		return Conns{Read: v, Write: v}
	default:
		panic(fmt.Sprintf("wiki: unsupported database handle %T", db))
	}
}

func (s *JobStore) Save(ctx context.Context, job Job) error {
	_, err := s.write.ExecContext(ctx,
		`INSERT INTO jobs (id, status) VALUES (?, ?)`,
		job.ID, job.Status)
	return err
}

func (s *JobStore) InsertIngest(ctx context.Context, job Job) error {
	tags, err := json.Marshal(job.Tags)
	if err != nil {
		return err
	}
	_, err = s.write.ExecContext(ctx, `
		INSERT INTO jobs (
			id, owner, source_text, title, tags, source_hash, status, received_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
		job.ID,
		job.Owner,
		job.SourceText,
		job.Title,
		string(tags),
		hashText(job.SourceText),
		job.Status,
		formatTime(job.ReceivedAt),
	)
	return err
}

func (s *JobStore) Get(ctx context.Context, id string) (Job, error) {
	var job Job
	err := s.read.QueryRowContext(ctx,
		`SELECT id, status FROM jobs WHERE id = ?`, id).
		Scan(&job.ID, &job.Status)
	return job, err
}

func (s *JobStore) ClaimPending(ctx context.Context, startedAt time.Time) (Job, bool, error) {
	tx, err := s.write.BeginTx(ctx, nil)
	if err != nil {
		return Job{}, false, err
	}
	defer tx.Rollback()

	job, err := scanJob(tx.QueryRowContext(ctx, `
		SELECT id, owner, source_text, title, tags, source_hash, status,
		       received_at, started_at, finished_at, error
		FROM jobs
		WHERE status = 'pending'
		ORDER BY received_at, id
		LIMIT 1`))
	if err == sql.ErrNoRows {
		return Job{}, false, nil
	}
	if err != nil {
		return Job{}, false, err
	}
	res, err := tx.ExecContext(ctx,
		`UPDATE jobs SET status = 'working', started_at = ? WHERE id = ? AND status = 'pending'`,
		formatTime(startedAt), job.ID)
	if err != nil {
		return Job{}, false, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return Job{}, false, err
	}
	if n == 0 {
		return Job{}, false, tx.Commit()
	}
	if err := tx.Commit(); err != nil {
		return Job{}, false, err
	}
	job.Status = "working"
	job.StartedAt = startedAt
	return job, true, nil
}

func (s *JobStore) Finish(ctx context.Context, id, status string, finishedAt time.Time, jobErr string) error {
	_, err := s.write.ExecContext(ctx,
		`UPDATE jobs SET status = ?, finished_at = ?, error = ? WHERE id = ?`,
		status, formatTime(finishedAt), jobErr, id)
	return err
}

func (s *JobStore) FinishWorking(ctx context.Context, id, status string, finishedAt time.Time, jobErr string) (bool, error) {
	res, err := s.write.ExecContext(ctx,
		`UPDATE jobs SET status = ?, finished_at = ?, error = ? WHERE id = ? AND status = ?`,
		status, formatTime(finishedAt), jobErr, id, JobWorking)
	if err != nil {
		return false, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return false, err
	}
	return n > 0, nil
}

func (s *JobStore) RequeueWorking(ctx context.Context) (int, error) {
	res, err := s.write.ExecContext(ctx, `
		UPDATE jobs
		SET status = ?, started_at = '', finished_at = '', error = ''
		WHERE status = ?`,
		JobPending, JobWorking)
	if err != nil {
		return 0, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return 0, err
	}
	return int(n), nil
}

func (s *JobStore) Abort(ctx context.Context, id string, finishedAt time.Time) (AbortResult, error) {
	tx, err := s.write.BeginTx(ctx, nil)
	if err != nil {
		return AbortResult{}, err
	}
	defer tx.Rollback()

	var status string
	if err := tx.QueryRowContext(ctx, `SELECT status FROM jobs WHERE id = ?`, id).Scan(&status); err != nil {
		return AbortResult{}, err
	}
	if status != JobPending && status != JobWorking {
		if err := tx.Commit(); err != nil {
			return AbortResult{}, err
		}
		return AbortResult{Status: status}, nil
	}

	res, err := tx.ExecContext(ctx,
		`UPDATE jobs SET status = ?, finished_at = ?, error = '' WHERE id = ? AND status = ?`,
		JobAborted, formatTime(finishedAt), id, status)
	if err != nil {
		return AbortResult{}, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return AbortResult{}, err
	}
	if err := tx.Commit(); err != nil {
		return AbortResult{}, err
	}
	if n == 0 {
		return AbortResult{Status: status}, nil
	}
	return AbortResult{Aborted: true, Status: JobAborted}, nil
}

func (s *JobStore) Rerun(ctx context.Context, id string) (RerunResult, error) {
	tx, err := s.write.BeginTx(ctx, nil)
	if err != nil {
		return RerunResult{}, err
	}
	defer tx.Rollback()

	var status string
	if err := tx.QueryRowContext(ctx, `SELECT status FROM jobs WHERE id = ?`, id).Scan(&status); err != nil {
		return RerunResult{}, err
	}
	switch status {
	case JobDone, JobFailed, JobAborted:
	default:
		if err := tx.Commit(); err != nil {
			return RerunResult{}, err
		}
		return RerunResult{Status: status}, ErrJobNotTerminal
	}

	res, err := tx.ExecContext(ctx, `
		UPDATE jobs
		SET status = ?, started_at = '', finished_at = '', error = ''
		WHERE id = ? AND status = ?`,
		JobPending, id, status)
	if err != nil {
		return RerunResult{}, err
	}
	n, err := res.RowsAffected()
	if err != nil {
		return RerunResult{}, err
	}
	if err := tx.Commit(); err != nil {
		return RerunResult{}, err
	}
	if n == 0 {
		return RerunResult{Status: status}, ErrJobNotTerminal
	}
	return RerunResult{Requeued: true, Status: JobPending}, nil
}

func (s *JobStore) Status(ctx context.Context, id string) (JobStatus, error) {
	job, err := scanJob(s.read.QueryRowContext(ctx, `
		SELECT id, owner, source_text, title, tags, source_hash, status,
		       received_at, started_at, finished_at, error
		FROM jobs
		WHERE id = ?`, id))
	if err != nil {
		return JobStatus{}, err
	}

	rows, err := s.read.QueryContext(ctx,
		`SELECT DISTINCT subject_id FROM claims WHERE job_id = ? ORDER BY subject_id`, id)
	if err != nil {
		return JobStatus{}, err
	}
	defer rows.Close()

	var subjects []string
	for rows.Next() {
		var subjectID string
		if err := rows.Scan(&subjectID); err != nil {
			return JobStatus{}, err
		}
		subjects = append(subjects, subjectID)
	}
	if err := rows.Err(); err != nil {
		return JobStatus{}, err
	}

	return JobStatus{
		ID:         job.ID,
		Status:     job.Status,
		ReceivedAt: job.ReceivedAt,
		StartedAt:  timePtr(job.StartedAt),
		FinishedAt: timePtr(job.FinishedAt),
		Error:      job.Error,
		Subjects:   subjects,
	}, nil
}

type JobFilter struct {
	Statuses     []string
	Kinds        []string
	Since, Until time.Time
}

func (s *JobStore) ListJobs(ctx context.Context, f JobFilter, p page.Params) ([]Job, string, error) {
	cursor, err := decodeCursor(p.Cursor, 2)
	if err != nil {
		return nil, "", err
	}
	limit := p.ResolvedLimit()
	var args []any
	query := `
		SELECT id, owner, source_text, title, tags, source_hash, status,
		       received_at, started_at, finished_at, error
		FROM jobs
		WHERE 1 = 1`
	query, args = appendJobFilter(query, args, f)
	if len(cursor) > 0 {
		query += `
		  AND (received_at < ? OR (received_at = ? AND id < ?))`
		args = append(args, cursor[0], cursor[0], cursor[1])
	}
	query += `
		ORDER BY received_at DESC, id DESC
		LIMIT ?`
	args = append(args, limit+1)
	rows, err := s.read.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, "", err
	}
	defer rows.Close()

	var jobs []Job
	for rows.Next() {
		job, err := scanJob(rows)
		if err != nil {
			return nil, "", err
		}
		jobs = append(jobs, job)
	}
	if err := rows.Err(); err != nil {
		return nil, "", err
	}
	return pageJobs(jobs, limit), nextJobCursor(jobs, limit), nil
}

func (s *JobStore) CountJobs(ctx context.Context, f JobFilter) (int, error) {
	var args []any
	query := `SELECT COUNT(*) FROM jobs WHERE 1 = 1`
	query, args = appendJobFilter(query, args, f)

	var n int
	if err := s.read.QueryRowContext(ctx, query, args...).Scan(&n); err != nil {
		return 0, err
	}
	return n, nil
}

func appendJobFilter(query string, args []any, f JobFilter) (string, []any) {
	statuses := normalizedStatuses(f.Statuses)
	if len(statuses) > 0 {
		query += `
		  AND status IN (` + placeholders(len(statuses)) + `)`
		for _, status := range statuses {
			args = append(args, status)
		}
	}
	kinds := normalizedJobKinds(f.Kinds)
	if len(kinds) == 1 {
		switch kinds[0] {
		case "ingest":
			query += `
		  AND NOT EXISTS (SELECT 1 FROM subject_merges WHERE subject_merges.job_id = jobs.id)`
		case "merge":
			query += `
		  AND EXISTS (SELECT 1 FROM subject_merges WHERE subject_merges.job_id = jobs.id)`
		}
	}
	if since := formatTime(f.Since); since != "" {
		query += `
		  AND received_at >= ?`
		args = append(args, since)
	}
	if until := formatTime(f.Until); until != "" {
		query += `
		  AND received_at <= ?`
		args = append(args, until)
	}
	return query, args
}

func normalizedJobKinds(kinds []string) []string {
	seen := map[string]bool{}
	var out []string
	for _, kind := range kinds {
		kind = strings.TrimSpace(kind)
		if kind == "" || seen[kind] {
			continue
		}
		seen[kind] = true
		out = append(out, kind)
	}
	return out
}

func normalizedStatuses(statuses []string) []string {
	var out []string
	for _, status := range statuses {
		status = strings.TrimSpace(status)
		if status != "" {
			out = append(out, status)
		}
	}
	return out
}

func placeholders(n int) string {
	var b strings.Builder
	for i := 0; i < n; i++ {
		if i > 0 {
			b.WriteString(", ")
		}
		b.WriteByte('?')
	}
	return b.String()
}

type rowScanner interface {
	Scan(dest ...any) error
}

type sqlStore interface {
	ExecContext(context.Context, string, ...any) (sql.Result, error)
	QueryContext(context.Context, string, ...any) (*sql.Rows, error)
	QueryRowContext(context.Context, string, ...any) *sql.Row
}

func scanJob(row rowScanner) (Job, error) {
	var job Job
	var tagsJSON, receivedAt, startedAt, finishedAt string
	if err := row.Scan(
		&job.ID,
		&job.Owner,
		&job.SourceText,
		&job.Title,
		&tagsJSON,
		&job.SourceHash,
		&job.Status,
		&receivedAt,
		&startedAt,
		&finishedAt,
		&job.Error,
	); err != nil {
		return Job{}, err
	}
	if strings.TrimSpace(tagsJSON) != "" {
		if err := json.Unmarshal([]byte(tagsJSON), &job.Tags); err != nil {
			return Job{}, err
		}
	}
	job.ReceivedAt = parseStoredTime(receivedAt)
	job.StartedAt = parseStoredTime(startedAt)
	job.FinishedAt = parseStoredTime(finishedAt)
	return job, nil
}

// SubjectStore persists canonical subjects.
type SubjectStore struct {
	db sqlStore
}

func NewSubjectStore(db sqlStore) *SubjectStore {
	return &SubjectStore{db: db}
}

func (s *SubjectStore) Save(ctx context.Context, subject Subject) error {
	normName := subject.NormName
	if normName == "" {
		normName = normalize(subject.Name)
	}
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO subjects (id, name, norm_name, type) VALUES (?, ?, ?, ?)`,
		subject.ID, subject.Name, normName, subject.Type)
	return err
}

func (s *SubjectStore) GetByNormName(ctx context.Context, name string) (Subject, error) {
	var subject Subject
	err := s.db.QueryRowContext(ctx,
		`SELECT id, name, norm_name, type FROM subjects WHERE norm_name = ?`,
		normalize(name)).
		Scan(&subject.ID, &subject.Name, &subject.NormName, &subject.Type)
	return subject, err
}

func (s *SubjectStore) Get(ctx context.Context, id string) (Subject, error) {
	var subject Subject
	err := s.db.QueryRowContext(ctx,
		`SELECT id, name, norm_name, type FROM subjects WHERE id = ?`, id).
		Scan(&subject.ID, &subject.Name, &subject.NormName, &subject.Type)
	return subject, err
}

func (s *SubjectStore) Delete(ctx context.Context, id string) error {
	_, err := s.db.ExecContext(ctx, `DELETE FROM subjects WHERE id = ?`, id)
	return err
}

func (s *SubjectStore) GetByPath(ctx context.Context, path string) (Subject, error) {
	typ, wantSlug, ok := strings.Cut(path, "/")
	if !ok || typ == "" || wantSlug == "" {
		return Subject{}, ErrSubjectNotFound
	}

	rows, err := s.db.QueryContext(ctx,
		`SELECT id, name, norm_name, type FROM subjects WHERE type = ? ORDER BY id`,
		typ)
	if err != nil {
		return Subject{}, err
	}
	defer rows.Close()

	var found Subject
	matches := 0
	for rows.Next() {
		var subject Subject
		if err := rows.Scan(&subject.ID, &subject.Name, &subject.NormName, &subject.Type); err != nil {
			return Subject{}, err
		}
		if slug(subject.NormName) != wantSlug {
			continue
		}
		matches++
		if matches == 1 {
			found = subject
		}
	}
	if err := rows.Err(); err != nil {
		return Subject{}, err
	}
	switch matches {
	case 0:
		return Subject{}, ErrSubjectNotFound
	case 1:
		return found, nil
	default:
		return Subject{}, ErrAmbiguousPath
	}
}

func (s *SubjectStore) List(ctx context.Context, typ, nameContains string, p page.Params) ([]Subject, string, error) {
	cursor, err := decodeCursor(p.Cursor, 2)
	if err != nil {
		return nil, "", err
	}
	limit := p.ResolvedLimit()
	typ = strings.TrimSpace(typ)
	nameContains = normalize(nameContains)
	args := []any{typ, typ, nameContains, nameContains}
	query := `
		SELECT id, name, norm_name, type
		FROM subjects
		WHERE (? = '' OR type = ?)
		  AND (? = '' OR norm_name LIKE '%' || ? || '%')`
	if len(cursor) > 0 {
		query += `
		  AND (name > ? OR (name = ? AND id > ?))`
		args = append(args, cursor[0], cursor[0], cursor[1])
	}
	query += `
		ORDER BY name, id
		LIMIT ?`
	args = append(args, limit+1)
	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, "", err
	}
	defer rows.Close()

	var subjects []Subject
	for rows.Next() {
		var subject Subject
		if err := rows.Scan(&subject.ID, &subject.Name, &subject.NormName, &subject.Type); err != nil {
			return nil, "", err
		}
		subjects = append(subjects, subject)
	}
	if err := rows.Err(); err != nil {
		return nil, "", err
	}
	return pageSubjects(subjects, limit), nextSubjectCursor(subjects, limit), nil
}

// ClaimStore persists extracted claims.
type ClaimStore struct {
	db sqlStore
}

func NewClaimStore(db sqlStore) *ClaimStore {
	return &ClaimStore{db: db}
}

func (s *ClaimStore) Save(ctx context.Context, claim Claim) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO claims (id, subject_id, job_id, body) VALUES (?, ?, ?, ?)`,
		claim.ID, claim.SubjectID, claim.JobID, claim.Body)
	return err
}

func (s *ClaimStore) SubjectIDsByJob(ctx context.Context, jobID string) ([]string, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT DISTINCT subject_id FROM claims WHERE job_id = ? ORDER BY subject_id`,
		jobID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var ids []string
	for rows.Next() {
		var id string
		if err := rows.Scan(&id); err != nil {
			return nil, err
		}
		ids = append(ids, id)
	}
	return ids, rows.Err()
}

func (s *ClaimStore) DeleteByJob(ctx context.Context, jobID string) error {
	_, err := s.db.ExecContext(ctx, `DELETE FROM claims WHERE job_id = ?`, jobID)
	return err
}

func (s *ClaimStore) RepointSubject(ctx context.Context, from, to string) error {
	_, err := s.db.ExecContext(ctx, `UPDATE claims SET subject_id = ? WHERE subject_id = ?`, to, from)
	return err
}

func hashText(text string) string {
	sum := sha256.Sum256([]byte(text))
	return hex.EncodeToString(sum[:])
}

func formatTime(t time.Time) string {
	if t.IsZero() {
		return ""
	}
	return t.UTC().Format(time.RFC3339Nano)
}

func parseStoredTime(s string) time.Time {
	if strings.TrimSpace(s) == "" {
		return time.Time{}
	}
	t, err := time.Parse(time.RFC3339Nano, s)
	if err != nil {
		return time.Time{}
	}
	return t
}

func timePtr(t time.Time) *time.Time {
	if t.IsZero() {
		return nil
	}
	return &t
}

func (s *ClaimStore) ListBySubject(ctx context.Context, subjectID string, p page.Params) ([]Claim, string, error) {
	cursor, err := decodeCursor(p.Cursor, 1)
	if err != nil {
		return nil, "", err
	}
	limit := p.ResolvedLimit()
	args := []any{subjectID}
	query := `SELECT id, subject_id, job_id, body FROM claims WHERE subject_id = ?`
	if len(cursor) > 0 {
		query += ` AND id > ?`
		args = append(args, cursor[0])
	}
	query += ` ORDER BY id LIMIT ?`
	args = append(args, limit+1)
	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, "", err
	}
	defer rows.Close()

	var claims []Claim
	for rows.Next() {
		var claim Claim
		if err := rows.Scan(&claim.ID, &claim.SubjectID, &claim.JobID, &claim.Body); err != nil {
			return nil, "", err
		}
		claims = append(claims, claim)
	}
	if err := rows.Err(); err != nil {
		return nil, "", err
	}
	return pageClaims(claims, limit), nextClaimCursor(claims, limit), nil
}

// PageStore persists pages.
type PageStore struct {
	db sqlStore
}

func NewPageStore(db sqlStore) *PageStore {
	return &PageStore{db: db}
}

func (s *PageStore) Upsert(ctx context.Context, page Page) error {
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO pages (id, subject_id, title, body)
		VALUES (?, ?, ?, ?)
		ON CONFLICT(id) DO UPDATE SET
			subject_id = excluded.subject_id,
			title = excluded.title,
			body = excluded.body`,
		page.ID, page.SubjectID, page.Title, page.Body)
	return err
}

func (s *PageStore) Get(ctx context.Context, id string) (Page, error) {
	var page Page
	err := s.db.QueryRowContext(ctx,
		`SELECT id, subject_id, title, body FROM pages WHERE id = ?`, id).
		Scan(&page.ID, &page.SubjectID, &page.Title, &page.Body)
	return page, err
}

func (s *PageStore) GetBySubject(ctx context.Context, subjectID string) (Page, error) {
	var page Page
	err := s.db.QueryRowContext(ctx, `
		SELECT id, subject_id, title, body
		FROM pages
		WHERE subject_id = ?
		ORDER BY id
		LIMIT 1`, subjectID).
		Scan(&page.ID, &page.SubjectID, &page.Title, &page.Body)
	return page, err
}

func (s *PageStore) DeleteBySubject(ctx context.Context, subjectID string) error {
	_, err := s.db.ExecContext(ctx, `DELETE FROM pages WHERE subject_id = ?`, subjectID)
	return err
}

// SubjectMerge is the payload for a queued subject-folding work item.
type SubjectMerge struct {
	JobID         string
	FromSubjectID string
	ToSubjectID   string
}

// SubjectMergeStore persists subject merge payloads keyed by jobs.id.
type SubjectMergeStore struct {
	db sqlStore
}

func NewSubjectMergeStore(db sqlStore) *SubjectMergeStore {
	return &SubjectMergeStore{db: db}
}

func (s *SubjectMergeStore) Save(ctx context.Context, merge SubjectMerge) error {
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO subject_merges (job_id, from_subject_id, to_subject_id) VALUES (?, ?, ?)`,
		merge.JobID, merge.FromSubjectID, merge.ToSubjectID)
	return err
}

func (s *SubjectMergeStore) GetByJob(ctx context.Context, jobID string) (SubjectMerge, error) {
	var merge SubjectMerge
	err := s.db.QueryRowContext(ctx, `
		SELECT job_id, from_subject_id, to_subject_id
		FROM subject_merges
		WHERE job_id = ?`, jobID).
		Scan(&merge.JobID, &merge.FromSubjectID, &merge.ToSubjectID)
	return merge, err
}
