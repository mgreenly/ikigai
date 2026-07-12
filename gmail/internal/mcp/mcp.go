// Package mcp declares gmail's MCP tool surface for the shared appkit transport.
//
// gmail is a connector + event-plane producer (gmail-connector-decisions §1).
// This is the P4 surface: the full normal-mailbox tool set over the P2 Gmail
// client — list/read/thread/send/draft/labels/label/unlabel/trash/delete — plus
// the two chassis tools, health (the end-to-end auth proof +
// health envelope) and reflection (self-describes the three
// mail.* events the producer emits). The producer that actually emits those
// events lives in P3 (internal/gmail). Read-only tools (list/read/thread/labels)
// only fetch; mutating tools (send/draft/label/unlabel/trash/delete) change the
// mailbox — trash and delete are the full-scope destructive verbs.
//
// The JSON-RPC transport, standard health tool, and event reflection tool live
// in appkit/mcp. This package contributes only gmail's mailbox tools and the
// mail.* published-event registry. It carries NO token logic: nginx introspects
// every request via auth_request and appkit/server's RequireIdentity gate makes
// the caller identity available to the shared transport.
package mcp

import (
	"context"
	"fmt"
	"net/http"

	"appkit"
	appkitmcp "appkit/mcp"

	gm "gmail/internal/gmail"
)

const Instructions = "Read and send Gmail and manage labels. Start with list to find messages, then read or thread."

// Client is the slice of the P2 Gmail REST client the MCP tool surface drives.
// It is an interface (not the concrete *gmail.Client) so the tool handlers can
// be unit-tested against a fake without any network. The concrete *gmail.Client
// satisfies it directly.
type Client interface {
	MessagesList(ctx context.Context, q, pageToken string) (gm.MessagesListResult, error)
	MessageGet(ctx context.Context, id, format string) (gm.Message, error)
	ThreadGet(ctx context.Context, id string) (gm.Thread, error)
	MessagesSend(ctx context.Context, raw string) (gm.Message, error)
	DraftCreate(ctx context.Context, raw string) (gm.Draft, error)
	LabelsList(ctx context.Context) (gm.LabelsListResult, error)
	MessageModify(ctx context.Context, id string, add, remove []string) (gm.Message, error)
	MessageTrash(ctx context.Context, id string) (gm.Message, error)
	MessageDelete(ctx context.Context, id string) error
}

// NewHandler builds the shared appkit MCP handler around gmail's mailbox tool
// table. client is the P2 Gmail client backing the full mailbox surface; a nil
// client is a wiring error and panics at this seam rather than deferring a nil
// dereference to first request.
func NewHandler(client Client, contentBase string, rt *appkit.Router) (http.Handler, error) {
	if client == nil {
		panic("mcp: gmail client is required")
	}
	if contentBase == "" {
		panic("mcp: content base is required")
	}
	if rt == nil {
		return nil, fmt.Errorf("mcp: router is required")
	}
	return appkitmcp.New(appkitmcp.Options{
		Service:       rt.Service(),
		Version:       rt.Version(),
		Instructions:  Instructions,
		Tools:         Tools(client, contentBase),
		Health:        rt.Health(),
		Events:        rt.Events(),
		Publishes:     rt.Publishes(),
		Subscriptions: rt.Subscriptions(),
	})
}
