// Package mcpclient is a minimal outbound MCP-over-HTTP client for the suite
// discovery path.
//
// It speaks JSON-RPC 2.0 over plain HTTP POST, sending Content-Type:
// application/json and reading a single JSON response per call. It carries no
// token logic: headers passed to New are injected on every request.
package mcpclient

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"time"
)

const defaultTimeout = 30 * time.Second

// Client is an outbound MCP-over-HTTP JSON-RPC client bound to one endpoint. It
// is safe for concurrent use.
type Client struct {
	endpoint   string
	httpClient *http.Client
	headers    map[string]string
}

// New builds a Client for the given /mcp endpoint. The headers map is copied
// and injected on every outgoing request. A zero timeout selects a bounded
// default timeout.
func New(endpoint string, headers map[string]string, timeout time.Duration) *Client {
	if timeout <= 0 {
		timeout = defaultTimeout
	}
	h := make(map[string]string, len(headers))
	for k, v := range headers {
		h[k] = v
	}
	return &Client{
		endpoint:   endpoint,
		httpClient: &http.Client{Timeout: timeout},
		headers:    h,
	}
}

// Tool is this package's minimal MCP tool descriptor. InputSchema stays raw so
// callers can forward it unchanged.
type Tool struct {
	Name        string
	Description string
	InputSchema json.RawMessage
}

// ListTools calls JSON-RPC tools/list and parses result tools into descriptors.
func (c *Client) ListTools(ctx context.Context) ([]Tool, error) {
	var out struct {
		Tools []struct {
			Name        string          `json:"name"`
			Description string          `json:"description"`
			InputSchema json.RawMessage `json:"inputSchema"`
		} `json:"tools"`
	}
	if err := c.call(ctx, "tools/list", nil, &out); err != nil {
		return nil, err
	}
	tools := make([]Tool, len(out.Tools))
	for i, t := range out.Tools {
		tools[i] = Tool{Name: t.Name, Description: t.Description, InputSchema: t.InputSchema}
	}
	return tools, nil
}

// CallTool calls JSON-RPC tools/call with params {name, arguments}. It flattens
// text content blocks and returns the MCP isError flag separately from transport
// or JSON-RPC errors.
func (c *Client) CallTool(ctx context.Context, name string, args json.RawMessage) (text string, isError bool, err error) {
	params := struct {
		Name      string          `json:"name"`
		Arguments json.RawMessage `json:"arguments,omitempty"`
	}{Name: name, Arguments: args}

	var out struct {
		Content []struct {
			Type string `json:"type"`
			Text string `json:"text"`
		} `json:"content"`
		IsError bool `json:"isError"`
	}
	if err := c.call(ctx, "tools/call", params, &out); err != nil {
		return "", false, err
	}
	var buf bytes.Buffer
	for _, blk := range out.Content {
		if blk.Type != "" && blk.Type != "text" {
			continue
		}
		buf.WriteString(blk.Text)
	}
	return buf.String(), out.IsError, nil
}

type jsonRPCResponse struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id"`
	Result  json.RawMessage `json:"result"`
	Error   *rpcError       `json:"error"`
}

type rpcError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

func (e *rpcError) Error() string {
	return fmt.Sprintf("jsonrpc error %d: %s", e.Code, e.Message)
}

func (c *Client) call(ctx context.Context, method string, params any, out any) error {
	reqBody := map[string]any{
		"jsonrpc": "2.0",
		"id":      1,
		"method":  method,
	}
	if params != nil {
		reqBody["params"] = params
	}
	body, err := json.Marshal(reqBody)
	if err != nil {
		return fmt.Errorf("mcpclient: marshal request: %w", err)
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, c.endpoint, bytes.NewReader(body))
	if err != nil {
		return fmt.Errorf("mcpclient: build request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")
	for k, v := range c.headers {
		req.Header.Set(k, v)
	}

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("mcpclient: %s: %w", method, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("mcpclient: %s: unexpected status %d", method, resp.StatusCode)
	}

	var rpcResp jsonRPCResponse
	if err := json.NewDecoder(resp.Body).Decode(&rpcResp); err != nil {
		return fmt.Errorf("mcpclient: %s: decode response: %w", method, err)
	}
	if rpcResp.Error != nil {
		return fmt.Errorf("mcpclient: %s: %w", method, rpcResp.Error)
	}
	if out != nil {
		if err := json.Unmarshal(rpcResp.Result, out); err != nil {
			return fmt.Errorf("mcpclient: %s: decode result: %w", method, err)
		}
	}
	return nil
}
