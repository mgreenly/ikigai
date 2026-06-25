package llm

import (
	"context"
	"fmt"
	"time"

	"appkit/logging"
	agentkit "github.com/ikigenba/agentkit"
)

// recordingEmbedder wraps an AgentKit embedder and logs each call footprint.
type Embedder interface {
	Embed(ctx context.Context, texts []string, role agentkit.InputType) (*agentkit.EmbedResult, error)
}

type recordingEmbedder struct {
	inner    Embedder
	recorder Recorder
	stage    string
	provider string
	model    string
	dims     int

	now   func() time.Time
	newID func() string
}

func NewRecordingEmbedder(inner Embedder, recorder Recorder, stage string, provider agentkit.EmbeddingProvider, model string, dims int) Embedder {
	providerName := ""
	if provider != nil {
		providerName = provider.Name()
	}
	return &recordingEmbedder{
		inner:    inner,
		recorder: recorder,
		stage:    stage,
		provider: providerName,
		model:    model,
		dims:     dims,
	}
}

func (e *recordingEmbedder) Embed(ctx context.Context, texts []string, role agentkit.InputType) (*agentkit.EmbedResult, error) {
	if ctx == nil {
		ctx = context.Background()
	}
	e.setDefaults()
	startedAt := e.now()
	result, err := e.inner.Embed(ctx, texts, role)
	endedAt := e.now()
	if recErr := e.record(ctx, texts, role, result, err, startedAt, endedAt); recErr != nil {
		return nil, recErr
	}
	if err != nil {
		return nil, err
	}
	return result, nil
}

func (e *recordingEmbedder) setDefaults() {
	if e.now == nil {
		e.now = time.Now
	}
	if e.newID == nil {
		e.newID = logging.NewULID
	}
}

func (e *recordingEmbedder) record(ctx context.Context, texts []string, role agentkit.InputType, result *agentkit.EmbedResult, callErr error, startedAt, endedAt time.Time) error {
	if e.recorder == nil {
		return nil
	}
	errText := ""
	if callErr != nil {
		errText = callErr.Error()
	}
	provider := e.provider
	model := e.model
	dims := e.dims
	if agentkitEmbedder, ok := e.inner.(*agentkit.Embedder); ok {
		if model == "" {
			model = agentkitEmbedder.Model
		}
		if dims == 0 {
			dims = agentkitEmbedder.Dimensions
		}
		if provider == "" && agentkitEmbedder.Provider != nil {
			provider = agentkitEmbedder.Provider.Name()
		}
	}
	rec := CallRecord{
		ID:        e.newID(),
		Stage:     e.stage,
		JobID:     JobID(ctx),
		Attempt:   1,
		Provider:  provider,
		Model:     model,
		Params:    mustJSON(embedParams{Dimensions: dims}),
		Request:   mustJSON(embedRequest{Inputs: append([]string(nil), texts...), Role: embedRole(role)}),
		Response:  embedResponseJSON(result),
		Usage:     embedUsageJSON(result),
		Err:       errText,
		StartedAt: startedAt,
		EndedAt:   endedAt,
	}
	return e.recorder.Record(ctx, rec)
}

type embedParams struct {
	Dimensions int `json:"dimensions,omitempty"`
}

type embedRequest struct {
	Inputs []string `json:"inputs"`
	Role   string   `json:"role"`
}

type embedResponse struct {
	Vectors  int `json:"vectors"`
	Dims     int `json:"dims,omitempty"`
	Warnings int `json:"warnings,omitempty"`
}

func embedResponseJSON(result *agentkit.EmbedResult) string {
	if result == nil {
		return ""
	}
	dims := 0
	if len(result.Vectors) > 0 {
		dims = len(result.Vectors[0])
	}
	return mustJSON(embedResponse{Vectors: len(result.Vectors), Dims: dims, Warnings: len(result.Warnings)})
}

func embedUsageJSON(result *agentkit.EmbedResult) string {
	if result == nil {
		return ""
	}
	usage := result.Usage()
	if usage == (agentkit.EmbeddingUsage{}) {
		return ""
	}
	return mustJSON(usage)
}

func embedRole(role agentkit.InputType) string {
	switch role {
	case agentkit.InputQuery:
		return "query"
	case agentkit.InputDocument:
		return "document"
	case agentkit.InputUnspecified:
		return "unspecified"
	default:
		return fmt.Sprintf("unknown:%d", role)
	}
}
