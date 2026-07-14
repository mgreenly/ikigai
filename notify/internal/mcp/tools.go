package mcp

import (
	"context"
	"encoding/json"
	"fmt"
	"net/url"
	"strings"

	appkitmcp "appkit/mcp"
	"appkit/server"

	"notify/internal/push"
)

// toolPrefix brands every MCP tool name. It is currently empty: HTTP route
// paths are not branded, and the wire tool names remain the committed notify
// verbs.
const toolPrefix = ""

func tool(verb string) string { return toolPrefix + verb }

type toolHandlers struct {
	push *push.Client
}

// Tools returns notify's service-owned MCP tool declarations. The shared appkit
// MCP transport appends the chassis health and reflection tools.
func Tools(client *push.Client) []appkitmcp.Tool {
	if client == nil {
		panic("mcp: push client is required")
	}
	h := &toolHandlers{push: client}
	return []appkitmcp.Tool{
		{
			Name: tool("send"),
			Description: "Push a notification to the owner's device. 'message' (required) is the body. " +
				"Optional: 'title' (a short headline); 'priority' (one of min|low|default|high|urgent; " +
				"drives device alerting, default 'default'); 'tags' (an array of strings; known emoji " +
				"shortcodes like \"warning\" or \"white_check_mark\" render as leading emoji, others as " +
				"text labels); 'click' (an absolute URL opened when the owner taps the notification). " +
				"The topic is fixed server-side. Returns {ok:true} once ntfy accepts the push; delivery " +
				"to the device is not guaranteed and is not reported.",
			InputSchema: obj(map[string]any{
				"message":  descTyp("string", "the notification body; required and non-empty"),
				"title":    descTyp("string", "optional short headline"),
				"priority": enumTyp("string", "min", "low", "default", "high", "urgent"),
				"tags":     map[string]any{"type": "array", "items": typ("string"), "description": "optional ntfy tags; emoji shortcodes render as icons, others as labels"},
				"click":    descTyp("string", "optional absolute URL opened when the owner taps the notification"),
			}, "message"),
			OutputSchema: sendOutputSchema,
			Handler: func(ctx context.Context, args json.RawMessage, _ server.Identity) (map[string]any, error) {
				return h.toolSend(ctx, args)
			},
		},
	}
}

func obj(props map[string]any, required ...string) map[string]any {
	o := map[string]any{"type": "object", "properties": props}
	if len(required) > 0 {
		o["required"] = required
	}
	return o
}

func typ(t string) map[string]any { return map[string]any{"type": t} }

func descTyp(t, description string) map[string]any {
	return map[string]any{"type": t, "description": description}
}

func enumTyp(t string, vals ...string) map[string]any {
	return map[string]any{"type": t, "enum": vals}
}

// sendOutputSchema mirrors the {"ok": true} success object send returns.
var sendOutputSchema = obj(map[string]any{
	"ok": typ("boolean"),
}, "ok")

// toolSend is notify's one write verb: it publishes a single notification to the
// owner's fixed ntfy topic and reports the real outcome synchronously. Caller
// errors render with the shared validation code so the agent can self-correct;
// an ntfy rejection or unreachable server renders as source_unavailable and
// never leaks the topic or token.
func (h *toolHandlers) toolSend(ctx context.Context, raw json.RawMessage) (map[string]any, error) {
	var a struct {
		Message  string   `json:"message"`
		Title    string   `json:"title,omitempty"`
		Priority string   `json:"priority,omitempty"`
		Tags     []string `json:"tags,omitempty"`
		Click    string   `json:"click,omitempty"`
	}
	if len(raw) > 0 {
		if err := json.Unmarshal(raw, &a); err != nil {
			return nil, err
		}
	}
	if strings.TrimSpace(a.Message) == "" {
		return appkitmcp.ErrorResult(appkitmcp.ErrValidation, "message is required and must be non-empty"), nil
	}
	prio, err := mapPriority(a.Priority)
	if err != nil {
		return appkitmcp.ErrorResult(appkitmcp.ErrValidation, err.Error()), nil
	}
	if a.Click != "" {
		if err := validateClick(a.Click); err != nil {
			return appkitmcp.ErrorResult(appkitmcp.ErrValidation, err.Error()), nil
		}
	}
	if err := h.push.Publish(ctx, push.Notification{
		Message:  a.Message,
		Title:    a.Title,
		Priority: prio,
		Tags:     a.Tags,
		Click:    a.Click,
	}); err != nil {
		return appkitmcp.ErrorResult(appkitmcp.ErrSourceUnavailable, "the notification service rejected the request or was unreachable"), nil
	}
	return appkitmcp.StructuredResult(map[string]any{"ok": true})
}

// mapPriority translates the send verb's string enum to ntfy's numeric priority.
// An empty value is "unset" and Publish maps it to an omitted header.
func mapPriority(s string) (int, error) {
	switch s {
	case "":
		return 0, nil
	case "min":
		return 1, nil
	case "low":
		return 2, nil
	case "default":
		return 3, nil
	case "high":
		return 4, nil
	case "urgent":
		return 5, nil
	default:
		return 0, fmt.Errorf("priority must be one of min, low, default, high, urgent")
	}
}

// validateClick enforces a well-formed absolute URL. It is intentionally light:
// reject only what is clearly not a URL and otherwise pass it through.
func validateClick(s string) error {
	u, err := url.Parse(s)
	if err != nil || !u.IsAbs() {
		return fmt.Errorf("click must be a well-formed absolute URL")
	}
	return nil
}
