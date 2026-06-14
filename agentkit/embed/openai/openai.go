// Package openai implements [embed.Embedder] against the first-party OpenAI
// embeddings endpoint (POST /v1/embeddings). Unlike the chat backend this is
// a unary JSON POST, not SSE: one request per Embed call over the whole
// texts slice, one row in the response per input in order.
//
// Authentication reuses the chat backend's pattern: OPENAI_API_KEY as a
// bearer credential. One key covers chat and embeddings (design §9.3); there
// is no separate embeddings key.
package openai

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/url"
	"time"

	"agentkit/accounting"
	"agentkit/embed"
	mdl "agentkit/model"
	"agentkit/provider"
)

const (
	defaultBaseURL = "https://api.openai.com"
	embeddingsPath = "/v1/embeddings"
)

// Client is the OpenAI embeddings backend. It satisfies [embed.Embedder].
type Client struct {
	apiKey  string
	baseURL string
	http    *http.Client
	// P0c: optional accounting sink; nil means no usage/cost record.
	logger *slog.Logger
}

// SetLogger attaches an accounting sink. Subsequent Embed calls emit one
// usage/cost slog record per call (P0c). The caller pre-binds call-site
// attribution onto logger; a nil logger is a no-op.
func (c *Client) SetLogger(l *slog.Logger) {
	c.logger = l
}

// New constructs a [Client]. apiKey must be the value of OPENAI_API_KEY; an
// empty key is rejected here so an absent credential surfaces at construction
// (the embedder is simply not built and the vector lane falls back to
// lexical-only — design §9.3) rather than as a 401 from the API. This mirrors
// the anthropic.New / chat openai.New refusal pattern.
func New(apiKey string) (*Client, error) {
	if apiKey == "" {
		return nil, fmt.Errorf("embed/openai: OPENAI_API_KEY is required")
	}
	return &Client{
		apiKey:  apiKey,
		baseURL: defaultBaseURL,
		http:    &http.Client{},
	}, nil
}

// SetBaseURL overrides the default API base URL. Used in tests to redirect to
// a local httptest server.
func (c *Client) SetBaseURL(u string) {
	c.baseURL = u
}

// embeddingsRequest is the JSON body of a /v1/embeddings call. dimensions is
// passed through verbatim; the provider rejects an illegal value.
type embeddingsRequest struct {
	Model          string   `json:"model"`
	Input          []string `json:"input"`
	Dimensions     int      `json:"dimensions,omitempty"`
	EncodingFormat string   `json:"encoding_format"`
}

// embeddingsResponse is the JSON body of a successful /v1/embeddings reply.
// Each data row carries its index so we can restore input order regardless of
// the server's emission order.
type embeddingsResponse struct {
	Data []struct {
		Index     int       `json:"index"`
		Embedding []float32 `json:"embedding"`
	} `json:"data"`
	Usage struct {
		PromptTokens int `json:"prompt_tokens"`
	} `json:"usage"`
}

// Embed issues one /v1/embeddings POST over the whole texts slice and returns
// one vector per input in order plus the reported input-token count. An empty
// texts slice is a no-op that returns an empty result without a network call.
func (c *Client) Embed(ctx context.Context, model string, dims int, texts []string) (embed.Result, error) {
	if len(texts) == 0 {
		// No API call is made, so no accounting record is emitted.
		return embed.Result{Vectors: [][]float32{}}, nil
	}

	start := time.Now()
	body, err := json.Marshal(embeddingsRequest{
		Model:          model,
		Input:          texts,
		Dimensions:     dims,
		EncodingFormat: "float",
	})
	if err != nil {
		return embed.Result{}, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "marshal request"}
	}

	endpoint, err := url.JoinPath(c.baseURL, embeddingsPath)
	if err != nil {
		return embed.Result{}, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "build endpoint"}
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, endpoint, bytes.NewReader(body))
	if err != nil {
		return embed.Result{}, &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "build request"}
	}
	httpReq.Header.Set("Authorization", "Bearer "+c.apiKey)
	httpReq.Header.Set("Content-Type", "application/json")

	resp, err := c.http.Do(httpReq)
	if err != nil {
		return embed.Result{}, mapTransportError(err)
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return embed.Result{}, &provider.Error{Kind: provider.ErrServer, Msg: "read response body"}
	}

	if resp.StatusCode != http.StatusOK {
		return embed.Result{}, mapErrorBody(resp.StatusCode, respBody)
	}

	var parsed embeddingsResponse
	if err := json.Unmarshal(respBody, &parsed); err != nil {
		return embed.Result{}, &provider.Error{Kind: provider.ErrServer, Msg: "decode response body"}
	}

	vectors := make([][]float32, len(texts))
	for _, row := range parsed.Data {
		if row.Index < 0 || row.Index >= len(vectors) {
			return embed.Result{}, &provider.Error{Kind: provider.ErrServer, Msg: "response row index out of range"}
		}
		vectors[row.Index] = row.Embedding
	}
	for i := range vectors {
		if vectors[i] == nil {
			return embed.Result{}, &provider.Error{Kind: provider.ErrServer, Msg: "response missing a vector"}
		}
	}

	// P0c: one accounting record per Embed (one API call). Embeddings bill
	// only on input tokens; there is no output/cache tier and no stop reason.
	// An unregistered model yields a zero rate (cost 0) rather than blocking
	// the record — pricing-presence enforcement is the composition root's job.
	pricing, _ := mdl.EmbeddingPricing(model)
	accounting.Log(c.logger, accounting.Record{
		Provider:    string(mdl.ProviderOpenAI),
		Model:       model,
		InputTokens: parsed.Usage.PromptTokens,
		CostUSD:     pricing.ComputeCost(parsed.Usage.PromptTokens),
		DurationMS:  accounting.DurationSince(start),
	})

	return embed.Result{
		Vectors:     vectors,
		InputTokens: parsed.Usage.PromptTokens,
	}, nil
}

// mapErrorBody reads an HTTP error response body (JSON) and returns a typed
// [provider.Error], reusing the chat backend's error.type mapping.
func mapErrorBody(statusCode int, body []byte) *provider.Error {
	var resp struct {
		Error struct {
			Type string `json:"type"`
		} `json:"error"`
	}
	if len(body) > 0 {
		_ = json.Unmarshal(body, &resp)
	}
	switch resp.Error.Type {
	case "authentication_error":
		return &provider.Error{Kind: provider.ErrAuth, Msg: "openai rejected credentials"}
	case "invalid_request_error":
		return &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "openai rejected the request"}
	case "rate_limit_error":
		return &provider.Error{Kind: provider.ErrRateLimit, Msg: "openai rate-limited the request"}
	case "server_error", "overloaded_error":
		return &provider.Error{Kind: provider.ErrServer, Msg: "openai server error"}
	}
	switch {
	case statusCode == http.StatusUnauthorized || statusCode == http.StatusForbidden:
		return &provider.Error{Kind: provider.ErrAuth, Msg: "openai rejected credentials"}
	case statusCode == http.StatusTooManyRequests:
		return &provider.Error{Kind: provider.ErrRateLimit, Msg: "openai rate-limited the request"}
	case statusCode >= 500:
		return &provider.Error{Kind: provider.ErrServer, Msg: "openai server error"}
	case statusCode >= 400:
		return &provider.Error{Kind: provider.ErrInvalidRequest, Msg: "openai rejected the request"}
	default:
		return &provider.Error{Kind: provider.ErrUnknown, Msg: "openai error"}
	}
}

// mapTransportError converts a Go transport error into a typed
// [provider.Error], mirroring the chat backend.
func mapTransportError(err error) *provider.Error {
	if errors.Is(err, context.DeadlineExceeded) {
		return &provider.Error{Kind: provider.ErrTimeout, Msg: "openai request deadline exceeded"}
	}
	if errors.Is(err, context.Canceled) {
		return &provider.Error{Kind: provider.ErrUnknown, Msg: "openai request canceled"}
	}
	var netErr net.Error
	if errors.As(err, &netErr) && netErr.Timeout() {
		return &provider.Error{Kind: provider.ErrTimeout, Msg: "openai request timed out"}
	}
	return &provider.Error{Kind: provider.ErrUnknown, Msg: "openai transport failure"}
}
