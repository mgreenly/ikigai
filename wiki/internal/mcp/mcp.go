// Package mcp implements wiki's domain MCP tools over the appkit transport.
package mcp

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"reflect"
	"strings"
	"time"

	"appkit"
	appkitmcp "appkit/mcp"
	"appkit/server"

	paging "wiki/internal/page"
	"wiki/internal/wiki"
)

const Instructions = "wiki is a knowledge base built from ingested source text. Call ingest to queue text for extraction; the pipeline distills subjects (entity/event/concept) and claims and compiles a cited page per subject. Use ask for a grounded, cited answer over the owner's wiki; subjects, claims, and page to read the compiled knowledge by type/slug path; jobs and status to track ingestion; and merge to fold a duplicate subject into another. health and reflection report service status and the event graph."

// Handler holds configured wiki domain tool dependencies.
type Handler struct {
	pageBase  string
	ingest    func(context.Context, string, string, string, []string) (string, error)
	status    func(context.Context, string) (any, error)
	abort     func(context.Context, string) (any, error)
	rerun     func(context.Context, string) (any, error)
	jobs      func(context.Context, JobFilter, paging.Params) (any, string, error)
	jobsCount func(context.Context, JobFilter) (int, error)
	resolve   func(context.Context, string) (any, error)
	merge     func(context.Context, string, string) (string, error)
	merges    func(context.Context, paging.Params) (any, string, error)
	ask       func(context.Context, string, string) (any, error)
	subjects  func(context.Context, string, string, paging.Params) (any, string, error)
	claims    func(context.Context, string, paging.Params) (any, string, error)
	page      func(context.Context, string) (any, error)
	calls     func(context.Context, LLMCallFilter, paging.Params) (any, string, error)
}

// JobFilter is a paginated MCP job-list filter.
type JobFilter struct {
	Statuses     []string
	Kinds        []string
	Since, Until time.Time
}

// LLMCallFilter is a paginated MCP LLM-call footprint filter.
type LLMCallFilter struct {
	JobID, Stage string
	Since, Until time.Time
}

type ingestService interface {
	Ingest(ctx context.Context, owner, text, title string, tags []string) (string, error)
}

type jobStatusFunc[T any] interface {
	JobStatus(ctx context.Context, jobID string) (T, error)
}

type jobAbortFunc[T any] interface {
	Abort(ctx context.Context, jobID string) (T, error)
}

type jobRerunFunc[T any] interface {
	Rerun(ctx context.Context, jobID string) (T, error)
}

type jobListFunc[T any] interface {
	ListJobs(ctx context.Context, f JobFilter, p paging.Params) (T, string, error)
}

type jobsCountFunc interface {
	CountJobs(ctx context.Context, f JobFilter) (int, error)
}

type subjectPathFunc[T any] interface {
	GetByPath(ctx context.Context, path string) (T, error)
}

type mergeFunc interface {
	MergeSubjects(ctx context.Context, fromSubjectID, toSubjectID string) (string, error)
}

type mergeListFunc[T any] interface {
	ListMerges(ctx context.Context, p paging.Params) (T, string, error)
}

type subjectsFunc[T any] interface {
	Subjects(ctx context.Context, typ, nameContains string) (T, error)
}

type subjectListFunc[T any] interface {
	List(ctx context.Context, typ, nameContains string, p paging.Params) (T, string, error)
}

type claimsFunc[T any] interface {
	ClaimsBySubject(ctx context.Context, subjectID string) (T, error)
}

type claimListFunc[T any] interface {
	ListBySubject(ctx context.Context, subjectID string, p paging.Params) (T, string, error)
}

type llmCallListFunc[T any] interface {
	List(ctx context.Context, f LLMCallFilter, p paging.Params) (T, string, error)
}

type pageFunc[T any] interface {
	PageBySubject(ctx context.Context, subjectID string) (T, error)
}

type pageByPathFunc[T any] interface {
	PageByPath(ctx context.Context, path string) (T, error)
}

// Option configures optional MCP tools backed by wiki domain services.
type Option func(*Handler)

// WithIngestService enables the ingest tool.
func WithIngestService(s ingestService) Option {
	return func(h *Handler) {
		if s != nil {
			h.ingest = s.Ingest
		}
	}
}

// WithJobStatusService enables the job-status tool.
func WithJobStatusService[T any](s jobStatusFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.status = func(ctx context.Context, jobID string) (any, error) {
				return s.JobStatus(ctx, jobID)
			}
		}
	}
}

// WithJobAbortService enables the abort tool.
func WithJobAbortService[T any](s jobAbortFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.abort = func(ctx context.Context, jobID string) (any, error) {
				return s.Abort(ctx, jobID)
			}
		}
	}
}

// WithJobRerunService enables the rerun tool.
func WithJobRerunService[T any](s jobRerunFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.rerun = func(ctx context.Context, jobID string) (any, error) {
				return s.Rerun(ctx, jobID)
			}
		}
	}
}

// WithJobListService enables the paginated jobs tool.
func WithJobListService[T any](s jobListFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.jobs = func(ctx context.Context, f JobFilter, p paging.Params) (any, string, error) {
				return s.ListJobs(ctx, f, p)
			}
		}
	}
}

// WithJobsService enables the paginated jobs tool.
func WithJobsService[T any](s jobListFunc[T]) Option {
	return WithJobListService(s)
}

// WithJobsCountService enables the jobs_count tool.
func WithJobsCountService(s jobsCountFunc) Option {
	return func(h *Handler) {
		if s != nil {
			h.jobsCount = s.CountJobs
		}
	}
}

// WithMergeService enables the merge tool.
func WithMergeService[T any](resolver subjectPathFunc[T], s mergeFunc) Option {
	return func(h *Handler) {
		if resolver != nil {
			h.resolve = func(ctx context.Context, path string) (any, error) {
				return resolver.GetByPath(ctx, path)
			}
		}
		if s != nil {
			h.merge = s.MergeSubjects
		}
	}
}

// WithMergeListService enables the paginated merges audit tool.
func WithMergeListService[T any](s mergeListFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.merges = func(ctx context.Context, p paging.Params) (any, string, error) {
				return s.ListMerges(ctx, p)
			}
		}
	}
}

// WithAbortService enables the abort tool.
func WithAbortService[T any](s jobAbortFunc[T]) Option {
	return WithJobAbortService(s)
}

// WithRerunService enables the rerun tool.
func WithRerunService[T any](s jobRerunFunc[T]) Option {
	return WithJobRerunService(s)
}

// WithSubjectsService enables the registry-list subjects tool.
func WithSubjectsService[T any](s subjectsFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.subjects = func(ctx context.Context, typ, nameContains string, _ paging.Params) (any, string, error) {
				subjects, err := s.Subjects(ctx, typ, nameContains)
				return subjects, "", err
			}
		}
	}
}

// WithSubjectListService enables the paginated registry-list subjects tool.
func WithSubjectListService[T any](s subjectListFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.subjects = func(ctx context.Context, typ, nameContains string, p paging.Params) (any, string, error) {
				return s.List(ctx, typ, nameContains, p)
			}
		}
	}
}

// WithClaimsService enables the claims-by-subject tool.
func WithClaimsService[T any](s claimsFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.claims = func(ctx context.Context, subjectID string, _ paging.Params) (any, string, error) {
				claims, err := s.ClaimsBySubject(ctx, subjectID)
				return claims, "", err
			}
		}
	}
}

// WithClaimListService enables the paginated claims-by-subject tool.
func WithClaimListService[T any](s claimListFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.claims = func(ctx context.Context, subjectID string, p paging.Params) (any, string, error) {
				return s.ListBySubject(ctx, subjectID, p)
			}
		}
	}
}

// WithPageService enables the page tool from the legacy subject-id service.
func WithPageService[T any](s pageFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.page = func(ctx context.Context, subjectID string) (any, error) {
				return s.PageBySubject(ctx, subjectID)
			}
		}
	}
}

// WithPagePathService enables the page tool from a public type/norm_name path service.
func WithPagePathService[T any](s pageByPathFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.page = func(ctx context.Context, path string) (any, error) {
				return s.PageByPath(ctx, path)
			}
		}
	}
}

// WithLLMCallListService enables the paginated llm_calls footprint tool.
func WithLLMCallListService[T any](s llmCallListFunc[T]) Option {
	return func(h *Handler) {
		if s != nil {
			h.calls = func(ctx context.Context, f LLMCallFilter, p paging.Params) (any, string, error) {
				return s.List(ctx, f, p)
			}
		}
	}
}

// WithLLMCallsService enables the paginated llm_calls footprint tool.
func WithLLMCallsService[T any](s llmCallListFunc[T]) Option {
	return WithLLMCallListService(s)
}

// WithAskFunc enables the grounded ask tool.
func WithAskFunc[T any](fn func(context.Context, string, string) (T, error)) Option {
	return func(h *Handler) {
		if fn != nil {
			h.ask = func(ctx context.Context, owner, question string) (any, error) {
				return fn(ctx, owner, question)
			}
		}
	}
}

// Tools returns wiki's configured domain MCP tools. Chassis health and
// reflection are supplied by appkit/mcp and are not declared here.
func Tools(opts ...Option) []appkitmcp.Tool {
	h := &Handler{}
	for _, opt := range opts {
		opt(h)
	}
	tools := []appkitmcp.Tool{}
	if h.ingest != nil {
		tools = append(tools, domainTool(ingestTool(), h.handleIngestCall))
	}
	if h.status != nil {
		tools = append(tools, domainTool(jobStatusTool(), h.handleJobStatusCall))
	}
	if h.abort != nil {
		tools = append(tools, domainTool(jobAbortTool(), h.handleJobAbortCall))
	}
	if h.rerun != nil {
		tools = append(tools, domainTool(jobRerunTool(), h.handleJobRerunCall))
	}
	if h.jobs != nil {
		tools = append(tools, domainTool(jobsTool(), h.handleJobsCall))
	}
	if h.jobsCount != nil {
		tools = append(tools, domainTool(jobsCountTool(), h.handleJobsCountCall))
	}
	if h.resolve != nil && h.merge != nil {
		tools = append(tools, domainTool(mergeTool(), h.handleMergeCall))
	}
	if h.merges != nil {
		tools = append(tools, domainTool(mergesTool(), h.handleMergesCall))
	}
	if h.ask != nil {
		tools = append(tools, domainTool(askTool(), h.handleAskCall))
	}
	if h.subjects != nil {
		tools = append(tools, domainTool(subjectsTool(), h.handleSubjectsCall))
	}
	if h.claims != nil {
		tools = append(tools, domainTool(claimsTool(), h.handleClaimsCall))
	}
	if h.page != nil {
		tools = append(tools, domainTool(pageTool(), h.handlePageCall))
	}
	if h.calls != nil {
		tools = append(tools, domainTool(llmCallsTool(), h.handleLLMCallsCall))
	}
	return tools
}

// NewHandler builds the MCP handler from appkit's route-time service metadata.
func NewHandler(rt *appkit.Router, opts ...Option) (http.Handler, error) {
	pageBase := strings.TrimRight(rt.AuthServer(), "/") + wiki.Mount
	handlerOpts := append([]Option{}, opts...)
	handlerOpts = append(handlerOpts, func(h *Handler) {
		h.pageBase = pageBase
	})
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  Instructions,
		Tools:         Tools(handlerOpts...),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}

func domainTool(desc map[string]any, handler func(context.Context, json.RawMessage, server.Identity) (map[string]any, error)) appkitmcp.Tool {
	return appkitmcp.Tool{
		Name:        desc["name"].(string),
		Description: desc["description"].(string),
		InputSchema: desc["inputSchema"].(map[string]any),
		Handler:     handler,
	}
}

func (h *Handler) handleIngestCall(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	if h.ingest == nil {
		return toolError("ingest tool is not configured"), nil
	}
	if strings.TrimSpace(id.OwnerEmail) == "" {
		return toolError("missing authenticated identity"), nil
	}
	var args struct {
		Text  string   `json:"text"`
		Title string   `json:"title"`
		Tags  []string `json:"tags"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	if strings.TrimSpace(args.Text) == "" {
		return toolError("text is required"), nil
	}
	jobID, err := h.ingest(ctx, id.OwnerEmail, args.Text, args.Title, args.Tags)
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]string{"job_id": jobID})
}

func (h *Handler) handleJobStatusCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.status == nil {
		return toolError("job_status tool is not configured"), nil
	}
	var args struct {
		JobID string `json:"job_id"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	if strings.TrimSpace(args.JobID) == "" {
		return toolError("job_id is required"), nil
	}
	status, err := h.status(ctx, args.JobID)
	if errors.Is(err, sql.ErrNoRows) {
		return appkitmcp.JSONResult(notFound("job", args.JobID))
	}
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(publicStatusResult(status))
}

func (h *Handler) handleJobAbortCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.abort == nil {
		return toolError("abort tool is not configured"), nil
	}
	args, err := decodeJobIDArgs(raw)
	if err != nil {
		return toolError(err.Error()), nil
	}
	result, err := h.abort(ctx, args.JobID)
	if errors.Is(err, sql.ErrNoRows) {
		return appkitmcp.JSONResult(notFound("job", args.JobID))
	}
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(publicAbortResult(result))
}

func (h *Handler) handleJobRerunCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.rerun == nil {
		return toolError("rerun tool is not configured"), nil
	}
	args, err := decodeJobIDArgs(raw)
	if err != nil {
		return toolError(err.Error()), nil
	}
	result, err := h.rerun(ctx, args.JobID)
	if errors.Is(err, sql.ErrNoRows) {
		return appkitmcp.JSONResult(notFound("job", args.JobID))
	}
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(publicRerunResult(result))
}

func (h *Handler) handleJobsCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.jobs == nil {
		return toolError("jobs tool is not configured"), nil
	}
	filter, limit, cursor, err := decodeJobsArgs(raw, true)
	if err != nil {
		return toolError(err.Error()), nil
	}
	jobs, next, err := h.jobs(ctx, filter, paging.Params{Limit: limit, Cursor: cursor})
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(pagedResult("jobs", publicJobsResult(jobs), next))
}

func (h *Handler) handleJobsCountCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.jobsCount == nil {
		return toolError("jobs_count tool is not configured"), nil
	}
	filter, _, _, err := decodeJobsArgs(raw, false)
	if err != nil {
		return toolError(err.Error()), nil
	}
	count, err := h.jobsCount(ctx, filter)
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]int{"count": count})
}

func (h *Handler) handleMergeCall(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	if h.resolve == nil || h.merge == nil {
		return toolError("merge tool is not configured"), nil
	}
	if strings.TrimSpace(id.OwnerEmail) == "" {
		return toolError("missing authenticated identity"), nil
	}
	var args struct {
		From string `json:"from"`
		To   string `json:"to"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	fromPath := strings.TrimSpace(args.From)
	toPath := strings.TrimSpace(args.To)
	if fromPath == "" {
		return toolError("from is required"), nil
	}
	if toPath == "" {
		return toolError("to is required"), nil
	}
	from, err := h.resolve(ctx, fromPath)
	if errors.Is(err, sql.ErrNoRows) {
		return appkitmcp.JSONResult(notFound("subject", fromPath))
	}
	if err != nil {
		return toolError(err.Error()), nil
	}
	to, err := h.resolve(ctx, toPath)
	if errors.Is(err, sql.ErrNoRows) {
		return appkitmcp.JSONResult(notFound("subject", toPath))
	}
	if err != nil {
		return toolError(err.Error()), nil
	}
	fromID := stringField(indirect(reflect.ValueOf(from)), "ID")
	toID := stringField(indirect(reflect.ValueOf(to)), "ID")
	if fromID == toID {
		return toolError("from and to resolve to the same subject"), nil
	}
	jobID, err := h.merge(ctx, fromID, toID)
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(map[string]string{"job_id": jobID})
}

func (h *Handler) handleMergesCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.merges == nil {
		return toolError("merges tool is not configured"), nil
	}
	var args struct {
		Limit  int    `json:"limit"`
		Cursor string `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	if strings.TrimSpace(args.Cursor) != "" {
		if _, ok := paging.DecodeCursor(args.Cursor); !ok {
			return toolError("cursor is invalid"), nil
		}
	}
	merges, next, err := h.merges(ctx, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(pagedResult("merges", publicMergesResult(merges), next))
}

func (h *Handler) handleAskCall(ctx context.Context, raw json.RawMessage, id server.Identity) (map[string]any, error) {
	if h.ask == nil {
		return toolError("ask tool is not configured"), nil
	}
	if strings.TrimSpace(id.OwnerEmail) == "" {
		return toolError("missing authenticated identity"), nil
	}
	var args struct {
		Question string `json:"question"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	if strings.TrimSpace(args.Question) == "" {
		return toolError("question is required"), nil
	}
	answer, err := h.ask(ctx, id.OwnerEmail, args.Question)
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(askToolResult(answer, h.pageBase))
}

func (h *Handler) handleSubjectsCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.subjects == nil {
		return toolError("subjects tool is not configured"), nil
	}
	var args struct {
		Type   string `json:"type"`
		Name   string `json:"name"`
		Limit  int    `json:"limit"`
		Cursor string `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	subjects, next, err := h.subjects(ctx, args.Type, args.Name, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(pagedResult("subjects", publicSubjectsResult(subjects), next))
}

func (h *Handler) handleClaimsCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.claims == nil {
		return toolError("claims tool is not configured"), nil
	}
	var args struct {
		Subject string `json:"subject"`
		Path    string `json:"path"`
		Limit   int    `json:"limit"`
		Cursor  string `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	subject := strings.TrimSpace(args.Subject)
	if subject == "" {
		subject = strings.TrimSpace(args.Path)
	}
	if subject == "" {
		return toolError("subject is required"), nil
	}
	claims, next, err := h.claims(ctx, subject, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if errors.Is(err, sql.ErrNoRows) {
		return appkitmcp.JSONResult(notFound("subject", subject))
	}
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(pagedResult("claims", publicClaimsResult(claims), next))
}

func (h *Handler) handlePageCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.page == nil {
		return toolError("page tool is not configured"), nil
	}
	var args struct {
		Subject string `json:"subject"`
		Path    string `json:"path"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	subject := strings.TrimSpace(args.Subject)
	if subject == "" {
		subject = strings.TrimSpace(args.Path)
	}
	if subject == "" {
		return toolError("subject is required"), nil
	}
	page, err := h.page(ctx, subject)
	if errors.Is(err, sql.ErrNoRows) {
		return appkitmcp.JSONResult(notFound("subject", subject))
	}
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(publicPageResult(page, subject))
}

func (h *Handler) handleLLMCallsCall(ctx context.Context, raw json.RawMessage, _ server.Identity) (map[string]any, error) {
	if h.calls == nil {
		return toolError("llm_calls tool is not configured"), nil
	}
	var args struct {
		JobID  string `json:"job_id"`
		Stage  string `json:"stage"`
		Since  string `json:"since"`
		Until  string `json:"until"`
		Limit  int    `json:"limit"`
		Cursor string `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return toolError(err.Error()), nil
	}
	since, err := parseOptionalTime(args.Since)
	if err != nil {
		return toolError("since must be RFC3339"), nil
	}
	until, err := parseOptionalTime(args.Until)
	if err != nil {
		return toolError("until must be RFC3339"), nil
	}
	calls, next, err := h.calls(ctx, LLMCallFilter{JobID: args.JobID, Stage: args.Stage, Since: since, Until: until}, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if err != nil {
		return toolError(err.Error()), nil
	}
	return appkitmcp.JSONResult(pagedResult("llm_calls", publicLLMCallsResult(calls), next))
}

func ingestTool() map[string]any {
	return map[string]any{
		"name":        "ingest",
		"description": "Queue source text for wiki ingestion.",
		"inputSchema": objectSchema(map[string]any{
			"text":  map[string]any{"type": "string"},
			"title": map[string]any{"type": "string"},
			"tags":  map[string]any{"type": "array", "items": map[string]any{"type": "string"}},
		}, []string{"text"}),
	}
}

func jobStatusTool() map[string]any {
	return map[string]any{
		"name":        "status",
		"description": "Return the status of a wiki ingest job.",
		"inputSchema": objectSchema(map[string]any{
			"job_id": map[string]any{"type": "string"},
		}, []string{"job_id"}),
	}
}

func jobAbortTool() map[string]any {
	return map[string]any{
		"name":        "abort",
		"description": "Abort a pending or working wiki ingest job.",
		"inputSchema": objectSchema(map[string]any{
			"job_id": map[string]any{"type": "string"},
		}, []string{"job_id"}),
	}
}

func jobRerunTool() map[string]any {
	return map[string]any{
		"name":        "rerun",
		"description": "Requeue a completed, failed, or aborted wiki ingest job.",
		"inputSchema": objectSchema(map[string]any{
			"job_id": map[string]any{"type": "string"},
		}, []string{"job_id"}),
	}
}

func jobsTool() map[string]any {
	return map[string]any{
		"name":        "jobs",
		"description": "List wiki ingest jobs with cursor pagination.",
		"inputSchema": listSchema(map[string]any{
			"status": jobStatusArraySchema(),
			"kind":   jobKindArraySchema(),
			"since":  map[string]any{"type": "string"},
			"until":  map[string]any{"type": "string"},
		}),
	}
}

func jobsCountTool() map[string]any {
	return map[string]any{
		"name":        "jobs_count",
		"description": "Count wiki ingest jobs matching the supplied filters.",
		"inputSchema": objectSchema(map[string]any{
			"status": jobStatusArraySchema(),
			"kind":   jobKindArraySchema(),
			"since":  map[string]any{"type": "string"},
			"until":  map[string]any{"type": "string"},
		}, nil),
	}
}

func mergeTool() map[string]any {
	return map[string]any{
		"name":        "merge",
		"description": "Queue a subject merge job from one subject path into another.",
		"inputSchema": objectSchema(map[string]any{
			"from": map[string]any{"type": "string"},
			"to":   map[string]any{"type": "string"},
		}, []string{"from", "to"}),
	}
}

func mergesTool() map[string]any {
	return map[string]any{
		"name":        "merges",
		"description": "List subject merge aliases with cursor pagination.",
		"inputSchema": listSchema(map[string]any{}),
	}
}

func jobStatusArraySchema() map[string]any {
	return map[string]any{
		"type": "array",
		"items": map[string]any{
			"type": "string",
			"enum": validJobStatuses,
		},
	}
}

func askTool() map[string]any {
	return map[string]any{
		"name":        "ask",
		"description": "Answer a question using the authenticated owner's wiki.",
		"inputSchema": objectSchema(map[string]any{
			"question": map[string]any{"type": "string"},
		}, []string{"question"}),
	}
}

func askToolResult(answer any, pageBase string) map[string]any {
	found, text, sourceCitations := answerFields(answer)
	citations := make([]map[string]string, 0, sourceCitations.Len())
	for i := 0; i < sourceCitations.Len(); i++ {
		citation := indirect(sourceCitations.Index(i))
		citations = append(citations, map[string]string{
			"url":   pageBase + stringField(citation, "Path"),
			"title": stringField(citation, "Title"),
		})
	}
	return map[string]any{
		"found":     found,
		"answer":    text,
		"citations": citations,
	}
}

func answerFields(answer any) (bool, string, reflect.Value) {
	v := indirect(reflect.ValueOf(answer))
	if !v.IsValid() || v.Kind() != reflect.Struct {
		return false, "", reflect.ValueOf([]any{})
	}
	return boolField(v, "Found"), stringField(v, "Text"), sliceField(v, "Citations")
}

func indirect(v reflect.Value) reflect.Value {
	for v.IsValid() && (v.Kind() == reflect.Pointer || v.Kind() == reflect.Interface) {
		if v.IsNil() {
			return reflect.Value{}
		}
		v = v.Elem()
	}
	return v
}

func boolField(v reflect.Value, name string) bool {
	field := v.FieldByName(name)
	return field.IsValid() && field.Kind() == reflect.Bool && field.Bool()
}

func stringField(v reflect.Value, name string) string {
	field := v.FieldByName(name)
	if !field.IsValid() || field.Kind() != reflect.String {
		return ""
	}
	return field.String()
}

func intField(v reflect.Value, name string) int64 {
	field := v.FieldByName(name)
	if !field.IsValid() {
		return 0
	}
	switch field.Kind() {
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		return field.Int()
	default:
		return 0
	}
}

func sliceField(v reflect.Value, name string) reflect.Value {
	field := v.FieldByName(name)
	if !field.IsValid() || field.Kind() != reflect.Slice {
		return reflect.ValueOf([]any{})
	}
	return field
}

func subjectsTool() map[string]any {
	return map[string]any{
		"name":        "subjects",
		"description": "List wiki registry subjects, optionally filtered by type and name substring.",
		"inputSchema": listSchema(map[string]any{
			"type": map[string]any{"type": "string"},
			"name": map[string]any{"type": "string"},
		}),
	}
}

func claimsTool() map[string]any {
	return map[string]any{
		"name":        "claims",
		"description": "Return claims attached to a wiki subject path.",
		"inputSchema": objectSchema(map[string]any{
			"subject": map[string]any{"type": "string"},
			"limit":   map[string]any{"type": "integer"},
			"cursor":  map[string]any{"type": "string"},
		}, []string{"subject"}),
	}
}

func pageTool() map[string]any {
	return map[string]any{
		"name":        "page",
		"description": "Return the compiled wiki page for a subject path.",
		"inputSchema": objectSchema(map[string]any{
			"subject": map[string]any{"type": "string"},
		}, []string{"subject"}),
	}
}

func llmCallsTool() map[string]any {
	return map[string]any{
		"name":        "llm_calls",
		"description": "List recorded LLM provider-call footprints with cursor pagination.",
		"inputSchema": listSchema(map[string]any{
			"job_id": map[string]any{"type": "string"},
			"stage":  map[string]any{"type": "string"},
			"since":  map[string]any{"type": "string"},
			"until":  map[string]any{"type": "string"},
		}),
	}
}

func listSchema(properties map[string]any) map[string]any {
	properties["limit"] = map[string]any{"type": "integer"}
	properties["cursor"] = map[string]any{"type": "string"}
	return objectSchema(properties, nil)
}

func objectSchema(properties map[string]any, required []string) map[string]any {
	schema := map[string]any{
		"type":                 "object",
		"additionalProperties": false,
		"properties":           properties,
	}
	if len(required) > 0 {
		schema["required"] = required
	}
	return schema
}

type jobIDArgs struct {
	JobID string `json:"job_id"`
}

func decodeJobIDArgs(raw json.RawMessage) (jobIDArgs, error) {
	var args jobIDArgs
	if err := decodeArgs(raw, &args); err != nil {
		return args, err
	}
	if strings.TrimSpace(args.JobID) == "" {
		return args, fmt.Errorf("job_id is required")
	}
	return args, nil
}

var validJobStatuses = []string{"pending", "working", "done", "failed", "aborted"}

type jobStatusArgs []string

func (s *jobStatusArgs) UnmarshalJSON(raw []byte) error {
	if string(raw) == "null" {
		*s = nil
		return nil
	}
	var values []string
	if err := json.Unmarshal(raw, &values); err != nil {
		var value string
		if err := json.Unmarshal(raw, &value); err != nil {
			return fmt.Errorf("status must be an array of strings")
		}
		values = []string{value}
	}
	out := make([]string, 0, len(values))
	for _, value := range values {
		value = strings.TrimSpace(value)
		if value == "" {
			continue
		}
		if !validJobStatus(value) {
			return fmt.Errorf("status must be one of %s", strings.Join(validJobStatuses, ", "))
		}
		out = append(out, value)
	}
	*s = out
	return nil
}

func validJobStatus(value string) bool {
	for _, valid := range validJobStatuses {
		if value == valid {
			return true
		}
	}
	return false
}

var validJobKinds = []string{"ingest", "merge"}

type jobKindArgs []string

func (k *jobKindArgs) UnmarshalJSON(raw []byte) error {
	if string(raw) == "null" {
		*k = nil
		return nil
	}
	var values []string
	if err := json.Unmarshal(raw, &values); err != nil {
		var value string
		if err := json.Unmarshal(raw, &value); err != nil {
			return fmt.Errorf("kind must be an array of strings")
		}
		values = []string{value}
	}
	out := make([]string, 0, len(values))
	for _, value := range values {
		value = strings.TrimSpace(value)
		if value == "" {
			continue
		}
		if !validJobKind(value) {
			return fmt.Errorf("kind must be one of %s", strings.Join(validJobKinds, ", "))
		}
		out = append(out, value)
	}
	*k = out
	return nil
}

func validJobKind(value string) bool {
	for _, valid := range validJobKinds {
		if value == valid {
			return true
		}
	}
	return false
}

func jobKindArraySchema() map[string]any {
	return map[string]any{
		"type": "array",
		"items": map[string]any{
			"type": "string",
			"enum": validJobKinds,
		},
	}
}

func decodeJobsArgs(raw json.RawMessage, withPaging bool) (JobFilter, int, string, error) {
	var args struct {
		Status jobStatusArgs `json:"status"`
		Kind   jobKindArgs   `json:"kind"`
		Since  string        `json:"since"`
		Until  string        `json:"until"`
		Limit  int           `json:"limit"`
		Cursor string        `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		return JobFilter{}, 0, "", err
	}
	since, err := parseOptionalTime(args.Since)
	if err != nil {
		return JobFilter{}, 0, "", fmt.Errorf("since must be RFC3339")
	}
	until, err := parseOptionalTime(args.Until)
	if err != nil {
		return JobFilter{}, 0, "", fmt.Errorf("until must be RFC3339")
	}
	cursor := strings.TrimSpace(args.Cursor)
	if withPaging && cursor != "" {
		if _, ok := paging.DecodeCursor(cursor); !ok {
			return JobFilter{}, 0, "", fmt.Errorf("cursor is invalid")
		}
	}
	kinds := []string(args.Kind)
	if len(kinds) == 0 {
		kinds = []string{"ingest"}
	}
	return JobFilter{Statuses: []string(args.Status), Kinds: kinds, Since: since, Until: until}, args.Limit, cursor, nil
}

func parseOptionalTime(s string) (time.Time, error) {
	s = strings.TrimSpace(s)
	if s == "" {
		return time.Time{}, nil
	}
	return time.Parse(time.RFC3339Nano, s)
}

func pagedResult(name string, values any, next string) map[string]any {
	return map[string]any{
		name:          values,
		"next_cursor": next,
	}
}

func publicStatusResult(status any) map[string]any {
	v := indirect(reflect.ValueOf(status))
	if !v.IsValid() || v.Kind() != reflect.Struct {
		return map[string]any{}
	}
	return map[string]any{
		"status":      stringField(v, "Status"),
		"received_at": interfaceField(v, "ReceivedAt"),
		"started_at":  interfaceField(v, "StartedAt"),
		"finished_at": interfaceField(v, "FinishedAt"),
		"error":       stringField(v, "Error"),
		"subjects":    stringSliceField(v, "Subjects"),
	}
}

func publicAbortResult(result any) map[string]any {
	v := indirect(reflect.ValueOf(result))
	if !v.IsValid() || v.Kind() != reflect.Struct {
		return map[string]any{}
	}
	return map[string]any{
		"aborted": boolField(v, "Aborted"),
		"status":  stringField(v, "Status"),
	}
}

func publicRerunResult(result any) map[string]any {
	v := indirect(reflect.ValueOf(result))
	if !v.IsValid() || v.Kind() != reflect.Struct {
		return map[string]any{}
	}
	return map[string]any{
		"requeued": boolField(v, "Requeued"),
		"status":   stringField(v, "Status"),
	}
}

func publicJobsResult(jobs any) []map[string]any {
	values := sliceValue(reflect.ValueOf(jobs))
	out := make([]map[string]any, 0, values.Len())
	for i := 0; i < values.Len(); i++ {
		job := indirect(values.Index(i))
		if !job.IsValid() || job.Kind() != reflect.Struct {
			continue
		}
		out = append(out, map[string]any{
			"id":          stringField(job, "ID"),
			"owner":       stringField(job, "Owner"),
			"title":       stringField(job, "Title"),
			"tags":        stringSliceField(job, "Tags"),
			"status":      stringField(job, "Status"),
			"received_at": interfaceField(job, "ReceivedAt"),
			"started_at":  interfaceField(job, "StartedAt"),
			"finished_at": interfaceField(job, "FinishedAt"),
			"error":       stringField(job, "Error"),
		})
	}
	return out
}

func publicMergesResult(merges any) []map[string]string {
	values := sliceValue(reflect.ValueOf(merges))
	out := make([]map[string]string, 0, values.Len())
	for i := 0; i < values.Len(); i++ {
		merge := indirect(values.Index(i))
		if !merge.IsValid() || merge.Kind() != reflect.Struct {
			continue
		}
		out = append(out, map[string]string{
			"norm_name":  stringField(merge, "NormName"),
			"subject_id": stringField(merge, "SubjectID"),
			"name":       stringField(merge, "Name"),
			"created_by": stringField(merge, "CreatedBy"),
			"created_at": stringField(merge, "CreatedAt"),
		})
	}
	return out
}

func publicSubjectsResult(subjects any) []map[string]any {
	values := sliceValue(reflect.ValueOf(subjects))
	out := make([]map[string]any, 0, values.Len())
	for i := 0; i < values.Len(); i++ {
		subject := indirect(values.Index(i))
		if !subject.IsValid() || subject.Kind() != reflect.Struct {
			continue
		}
		out = append(out, map[string]any{
			"path":     pathField(subject),
			"type":     stringField(subject, "Type"),
			"name":     stringField(subject, "Name"),
			"has_page": boolField(subject, "HasPage"),
		})
	}
	return out
}

func publicLLMCallsResult(calls any) []map[string]any {
	values := sliceValue(reflect.ValueOf(calls))
	out := make([]map[string]any, 0, values.Len())
	for i := 0; i < values.Len(); i++ {
		call := indirect(values.Index(i))
		if !call.IsValid() || call.Kind() != reflect.Struct {
			continue
		}
		out = append(out, map[string]any{
			"id":         stringField(call, "ID"),
			"stage":      stringField(call, "Stage"),
			"job_id":     stringField(call, "JobID"),
			"attempt":    intField(call, "Attempt"),
			"provider":   stringField(call, "Provider"),
			"model":      stringField(call, "Model"),
			"params":     stringField(call, "Params"),
			"request":    stringField(call, "Request"),
			"response":   stringField(call, "Response"),
			"usage":      stringField(call, "Usage"),
			"error":      stringField(call, "Err"),
			"started_at": interfaceField(call, "StartedAt"),
			"ended_at":   interfaceField(call, "EndedAt"),
		})
	}
	return out
}

func publicClaimsResult(claims any) []map[string]string {
	values := sliceValue(reflect.ValueOf(claims))
	out := make([]map[string]string, 0, values.Len())
	for i := 0; i < values.Len(); i++ {
		claim := indirect(values.Index(i))
		if !claim.IsValid() || claim.Kind() != reflect.Struct {
			continue
		}
		text := stringField(claim, "Text")
		if text == "" {
			text = stringField(claim, "Body")
		}
		job := stringField(claim, "Job")
		if job == "" {
			job = stringField(claim, "JobID")
		}
		out = append(out, map[string]string{
			"id":   stringField(claim, "ID"),
			"text": text,
			"job":  job,
		})
	}
	return out
}

func publicPageResult(page any, path string) map[string]string {
	v := indirect(reflect.ValueOf(page))
	if !v.IsValid() || v.Kind() != reflect.Struct {
		return map[string]string{}
	}
	pagePath := stringField(v, "Path")
	if pagePath == "" {
		pagePath = strings.TrimSpace(path)
	}
	return map[string]string{
		"subject": pagePath,
		"title":   stringField(v, "Title"),
		"body":    stringField(v, "Body"),
	}
}

func pathField(v reflect.Value) string {
	if path := stringField(v, "Path"); path != "" {
		return path
	}
	typ := stringField(v, "Type")
	normName := stringField(v, "NormName")
	if typ == "" || normName == "" {
		return ""
	}
	return typ + "/" + normName
}

func interfaceField(v reflect.Value, name string) any {
	field := v.FieldByName(name)
	if !field.IsValid() || !field.CanInterface() {
		return nil
	}
	if (field.Kind() == reflect.Pointer || field.Kind() == reflect.Interface) && field.IsNil() {
		return nil
	}
	return field.Interface()
}

func stringSliceField(v reflect.Value, name string) []string {
	field := v.FieldByName(name)
	if !field.IsValid() || field.Kind() != reflect.Slice {
		return nil
	}
	out := make([]string, 0, field.Len())
	for i := 0; i < field.Len(); i++ {
		item := field.Index(i)
		if item.Kind() == reflect.String {
			out = append(out, item.String())
		}
	}
	return out
}

func sliceValue(v reflect.Value) reflect.Value {
	v = indirect(v)
	if !v.IsValid() || v.Kind() != reflect.Slice {
		return reflect.ValueOf([]any{})
	}
	return v
}

func notFound(kind, id string) map[string]any {
	return map[string]any{
		"found": false,
		"kind":  kind,
		"id":    id,
	}
}

func decodeArgs(raw json.RawMessage, dst any) error {
	if len(raw) == 0 {
		raw = []byte(`{}`)
	}
	if err := json.Unmarshal(raw, dst); err != nil {
		return fmt.Errorf("invalid arguments: %w", err)
	}
	return nil
}

func toolError(text string) map[string]any {
	return appkitmcp.ErrorResult(text)
}
