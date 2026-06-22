// Package mcp implements wiki's initial JSON-RPC MCP smoke surface.
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
	"unicode"

	"appkit"

	paging "wiki/internal/page"
)

// Handler serves a small MCP-compatible JSON-RPC endpoint for Phase 01.
type Handler struct {
	version  string
	service  string
	health   func(context.Context) (map[string]any, error)
	ingest   func(context.Context, string, string, string, []string) (string, error)
	status   func(context.Context, string) (any, error)
	abort    func(context.Context, string) (any, error)
	rerun    func(context.Context, string) (any, error)
	jobs     func(context.Context, JobFilter, paging.Params) (any, string, error)
	ask      func(context.Context, string, string) (any, error)
	subjects func(context.Context, string, string, paging.Params) (any, string, error)
	claims   func(context.Context, string, paging.Params) (any, string, error)
	page     func(context.Context, string) (any, error)
	calls    func(context.Context, LLMCallFilter, paging.Params) (any, string, error)
}

// JobFilter is a paginated MCP job-list filter.
type JobFilter struct {
	Status       string
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

// WithPagePathService enables the page tool from a public type/slug path service.
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

// NewHandler builds the MCP handler from appkit's route-time service metadata.
func NewHandler(version, service string, health func(context.Context) (map[string]any, error), opts ...Option) *Handler {
	h := &Handler{version: version, service: service, health: health}
	for _, opt := range opts {
		opt(h)
	}
	return h
}

func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	var req request
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeError(w, nil, -32700, "parse error")
		return
	}

	switch req.Method {
	case "initialize":
		writeResult(w, req.ID, map[string]any{
			"protocolVersion": "2025-03-26",
			"capabilities":    map[string]any{"tools": map[string]any{}},
			"serverInfo":      map[string]any{"name": "Wiki", "version": h.version},
		})
	case "notifications/initialized":
		w.WriteHeader(http.StatusAccepted)
	case "tools/list":
		writeResult(w, req.ID, map[string]any{"tools": h.tools()})
	case "tools/call":
		h.handleToolCall(r.Context(), w, req)
	default:
		writeError(w, req.ID, -32601, "method not found")
	}
}

func (h *Handler) handleToolCall(ctx context.Context, w http.ResponseWriter, req request) {
	var params struct {
		Name      string          `json:"name"`
		Arguments json.RawMessage `json:"arguments"`
	}
	if err := json.Unmarshal(req.Params, &params); err != nil {
		writeError(w, req.ID, -32602, "invalid params")
		return
	}
	switch params.Name {
	case "ingest":
		h.handleIngestCall(ctx, w, req, params.Arguments)
	case "status":
		h.handleJobStatusCall(ctx, w, req, params.Arguments)
	case "abort":
		h.handleJobAbortCall(ctx, w, req, params.Arguments)
	case "rerun":
		h.handleJobRerunCall(ctx, w, req, params.Arguments)
	case "jobs":
		h.handleJobsCall(ctx, w, req, params.Arguments)
	case "ask":
		h.handleAskCall(ctx, w, req, params.Arguments)
	case "subjects":
		h.handleSubjectsCall(ctx, w, req, params.Arguments)
	case "claims":
		h.handleClaimsCall(ctx, w, req, params.Arguments)
	case "page":
		h.handlePageCall(ctx, w, req, params.Arguments)
	case "llm_calls":
		h.handleLLMCallsCall(ctx, w, req, params.Arguments)
	case "health":
		h.handleHealthCall(ctx, w, req)
	case "reflection":
		h.handleReflectionCall(w, req)
	default:
		writeResult(w, req.ID, toolError("unknown tool"))
		return
	}
}

func (h *Handler) handleHealthCall(ctx context.Context, w http.ResponseWriter, req request) {
	details := map[string]any{}
	if h.health != nil {
		d, err := h.health(ctx)
		if err != nil {
			details = map[string]any{"error": err.Error()}
		} else if d != nil {
			details = d
		}
	}
	env := appkit.Envelope(h.version, h.service, details)
	b, err := json.Marshal(env)
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeResult(w, req.ID, toolText(string(b)))
}

func (h *Handler) handleIngestCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.ingest == nil {
		writeResult(w, req.ID, toolError("ingest tool is not configured"))
		return
	}
	id, ok := appkit.IdentityFrom(ctx)
	if !ok {
		writeResult(w, req.ID, toolError("missing authenticated identity"))
		return
	}
	var args struct {
		Text  string   `json:"text"`
		Title string   `json:"title"`
		Tags  []string `json:"tags"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	if strings.TrimSpace(args.Text) == "" {
		writeResult(w, req.ID, toolError("text is required"))
		return
	}
	jobID, err := h.ingest(ctx, id.OwnerEmail, args.Text, args.Title, args.Tags)
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, map[string]string{"job_id": jobID})
}

func (h *Handler) handleJobStatusCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.status == nil {
		writeResult(w, req.ID, toolError("job_status tool is not configured"))
		return
	}
	var args struct {
		JobID string `json:"job_id"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	if strings.TrimSpace(args.JobID) == "" {
		writeResult(w, req.ID, toolError("job_id is required"))
		return
	}
	status, err := h.status(ctx, args.JobID)
	if errors.Is(err, sql.ErrNoRows) {
		writeJSONTextResult(w, req.ID, notFound("job", args.JobID))
		return
	}
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, publicStatusResult(status))
}

func (h *Handler) handleJobAbortCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.abort == nil {
		writeResult(w, req.ID, toolError("abort tool is not configured"))
		return
	}
	args, ok := decodeJobIDArgs(w, req, raw)
	if !ok {
		return
	}
	result, err := h.abort(ctx, args.JobID)
	if errors.Is(err, sql.ErrNoRows) {
		writeJSONTextResult(w, req.ID, notFound("job", args.JobID))
		return
	}
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, publicAbortResult(result))
}

func (h *Handler) handleJobRerunCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.rerun == nil {
		writeResult(w, req.ID, toolError("rerun tool is not configured"))
		return
	}
	args, ok := decodeJobIDArgs(w, req, raw)
	if !ok {
		return
	}
	result, err := h.rerun(ctx, args.JobID)
	if errors.Is(err, sql.ErrNoRows) {
		writeJSONTextResult(w, req.ID, notFound("job", args.JobID))
		return
	}
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, publicRerunResult(result))
}

func (h *Handler) handleJobsCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.jobs == nil {
		writeResult(w, req.ID, toolError("jobs tool is not configured"))
		return
	}
	var args struct {
		Status string `json:"status"`
		Since  string `json:"since"`
		Until  string `json:"until"`
		Limit  int    `json:"limit"`
		Cursor string `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	since, err := parseOptionalTime(args.Since)
	if err != nil {
		writeResult(w, req.ID, toolError("since must be RFC3339"))
		return
	}
	until, err := parseOptionalTime(args.Until)
	if err != nil {
		writeResult(w, req.ID, toolError("until must be RFC3339"))
		return
	}
	jobs, next, err := h.jobs(ctx, JobFilter{Status: args.Status, Since: since, Until: until}, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, pagedResult("jobs", publicJobsResult(jobs), next))
}

func (h *Handler) handleAskCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.ask == nil {
		writeResult(w, req.ID, toolError("ask tool is not configured"))
		return
	}
	id, ok := appkit.IdentityFrom(ctx)
	if !ok {
		writeResult(w, req.ID, toolError("missing authenticated identity"))
		return
	}
	var args struct {
		Question string `json:"question"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	if strings.TrimSpace(args.Question) == "" {
		writeResult(w, req.ID, toolError("question is required"))
		return
	}
	answer, err := h.ask(ctx, id.OwnerEmail, args.Question)
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, askToolResult(answer))
}

func (h *Handler) handleSubjectsCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.subjects == nil {
		writeResult(w, req.ID, toolError("subjects tool is not configured"))
		return
	}
	var args struct {
		Type   string `json:"type"`
		Name   string `json:"name"`
		Limit  int    `json:"limit"`
		Cursor string `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	subjects, next, err := h.subjects(ctx, args.Type, args.Name, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, pagedResult("subjects", publicSubjectsResult(subjects), next))
}

func (h *Handler) handleClaimsCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.claims == nil {
		writeResult(w, req.ID, toolError("claims tool is not configured"))
		return
	}
	var args struct {
		Subject string `json:"subject"`
		Path    string `json:"path"`
		Limit   int    `json:"limit"`
		Cursor  string `json:"cursor"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	subject := strings.TrimSpace(args.Subject)
	if subject == "" {
		subject = strings.TrimSpace(args.Path)
	}
	if subject == "" {
		writeResult(w, req.ID, toolError("subject is required"))
		return
	}
	claims, next, err := h.claims(ctx, subject, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if errors.Is(err, sql.ErrNoRows) {
		writeJSONTextResult(w, req.ID, notFound("subject", subject))
		return
	}
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, pagedResult("claims", publicClaimsResult(claims), next))
}

func (h *Handler) handlePageCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.page == nil {
		writeResult(w, req.ID, toolError("page tool is not configured"))
		return
	}
	var args struct {
		Subject string `json:"subject"`
		Path    string `json:"path"`
	}
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	subject := strings.TrimSpace(args.Subject)
	if subject == "" {
		subject = strings.TrimSpace(args.Path)
	}
	if subject == "" {
		writeResult(w, req.ID, toolError("subject is required"))
		return
	}
	page, err := h.page(ctx, subject)
	if errors.Is(err, sql.ErrNoRows) {
		writeJSONTextResult(w, req.ID, notFound("subject", subject))
		return
	}
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, publicPageResult(page, subject))
}

func (h *Handler) handleLLMCallsCall(ctx context.Context, w http.ResponseWriter, req request, raw json.RawMessage) {
	if h.calls == nil {
		writeResult(w, req.ID, toolError("llm_calls tool is not configured"))
		return
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
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	since, err := parseOptionalTime(args.Since)
	if err != nil {
		writeResult(w, req.ID, toolError("since must be RFC3339"))
		return
	}
	until, err := parseOptionalTime(args.Until)
	if err != nil {
		writeResult(w, req.ID, toolError("until must be RFC3339"))
		return
	}
	calls, next, err := h.calls(ctx, LLMCallFilter{JobID: args.JobID, Stage: args.Stage, Since: since, Until: until}, paging.Params{Limit: args.Limit, Cursor: args.Cursor})
	if err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return
	}
	writeJSONTextResult(w, req.ID, pagedResult("llm_calls", publicLLMCallsResult(calls), next))
}

func (h *Handler) handleReflectionCall(w http.ResponseWriter, req request) {
	writeJSONTextResult(w, req.ID, map[string][]string{
		"publishes":  {},
		"subscribes": {},
	})
}

func (h *Handler) tools() []map[string]any {
	tools := []map[string]any{healthTool(), reflectionTool()}
	if h.ingest != nil {
		tools = append(tools, ingestTool())
	}
	if h.status != nil {
		tools = append(tools, jobStatusTool())
	}
	if h.abort != nil {
		tools = append(tools, jobAbortTool())
	}
	if h.rerun != nil {
		tools = append(tools, jobRerunTool())
	}
	if h.jobs != nil {
		tools = append(tools, jobsTool())
	}
	if h.ask != nil {
		tools = append(tools, askTool())
	}
	if h.subjects != nil {
		tools = append(tools, subjectsTool())
	}
	if h.claims != nil {
		tools = append(tools, claimsTool())
	}
	if h.page != nil {
		tools = append(tools, pageTool())
	}
	if h.calls != nil {
		tools = append(tools, llmCallsTool())
	}
	return tools
}

func healthTool() map[string]any {
	return map[string]any{
		"name":        "health",
		"description": "Report wiki service health.",
		"inputSchema": map[string]any{
			"type":                 "object",
			"additionalProperties": false,
			"properties":           map[string]any{},
		},
	}
}

func reflectionTool() map[string]any {
	return map[string]any{
		"name":        "reflection",
		"description": "Report wiki service event-plane publications and subscriptions.",
		"inputSchema": map[string]any{
			"type":                 "object",
			"additionalProperties": false,
			"properties":           map[string]any{},
		},
	}
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
			"status": map[string]any{"type": "string"},
			"since":  map[string]any{"type": "string"},
			"until":  map[string]any{"type": "string"},
		}),
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

func askToolResult(answer any) map[string]any {
	found, text, sourceCitations := answerFields(answer)
	citations := make([]map[string]string, 0, sourceCitations.Len())
	for i := 0; i < sourceCitations.Len(); i++ {
		citation := indirect(sourceCitations.Index(i))
		citations = append(citations, map[string]string{
			"path":  stringField(citation, "Path"),
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

func decodeJobIDArgs(w http.ResponseWriter, req request, raw json.RawMessage) (jobIDArgs, bool) {
	var args jobIDArgs
	if err := decodeArgs(raw, &args); err != nil {
		writeResult(w, req.ID, toolError(err.Error()))
		return args, false
	}
	if strings.TrimSpace(args.JobID) == "" {
		writeResult(w, req.ID, toolError("job_id is required"))
		return args, false
	}
	return args, true
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
	return typ + "/" + slug(normName)
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

type request struct {
	ID     json.RawMessage `json:"id,omitempty"`
	Method string          `json:"method"`
	Params json.RawMessage `json:"params,omitempty"`
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

func writeResult(w http.ResponseWriter, id json.RawMessage, result any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0",
		"id":      idOrNull(id),
		"result":  result,
	})
}

func writeError(w http.ResponseWriter, id json.RawMessage, code int, msg string) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"jsonrpc": "2.0",
		"id":      idOrNull(id),
		"error":   map[string]any{"code": code, "message": msg},
	})
}

func idOrNull(id json.RawMessage) any {
	if len(id) == 0 {
		return nil
	}
	return id
}

func toolText(text string) map[string]any {
	return map[string]any{"content": []map[string]string{{"type": "text", "text": text}}}
}

func writeJSONTextResult(w http.ResponseWriter, id json.RawMessage, value any) {
	b, err := json.Marshal(value)
	if err != nil {
		writeResult(w, id, toolError(err.Error()))
		return
	}
	writeResult(w, id, toolText(string(b)))
}

func toolError(text string) map[string]any {
	return map[string]any{
		"isError": true,
		"content": []map[string]string{{"type": "text", "text": text}},
	}
}
