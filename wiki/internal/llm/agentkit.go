package llm

import (
	"io"
	"time"

	"appkit/logging"
	agentkit "github.com/ikigenba/agentkit"
)

// AgentKitProvider is the production provider boundary supplied at composition.
type AgentKitProvider = agentkit.Provider

// Client is the production LLM client shell shared by wiki services.
type Client struct {
	prov     AgentKitProvider
	log      io.Writer
	model    string
	recorder Recorder
	now      func() time.Time
	newID    func() string
}

// NewClient records the provider and model selected at the composition root.
func NewClient(provider AgentKitProvider, model string, recorders ...Recorder) *Client {
	c := &Client{prov: provider, model: model}
	if len(recorders) > 0 {
		c.recorder = recorders[0]
	}
	c.setDefaults()
	return c
}

// Model reports the configured model id.
func (c *Client) Model() string {
	if c == nil {
		return ""
	}
	return c.model
}

// Provider reports the configured AgentKit provider.
func (c *Client) Provider() AgentKitProvider {
	if c == nil {
		return nil
	}
	return c.prov
}

// WithRecorder installs the call-footprint recorder and returns the client.
func (c *Client) WithRecorder(rec Recorder) *Client {
	if c == nil {
		return nil
	}
	c.recorder = rec
	c.setDefaults()
	return c
}

// WithClock installs the wall-clock source used for call records.
func (c *Client) WithClock(now func() time.Time) *Client {
	if c == nil {
		return nil
	}
	c.now = now
	c.setDefaults()
	return c
}

func (c *Client) setDefaults() {
	if c == nil {
		return
	}
	if c.now == nil {
		c.now = time.Now
	}
	if c.newID == nil {
		c.newID = logging.NewULID
	}
}

// ToAgentKit converts the narrow wiki prompt shape to AgentKit messages.
func ToAgentKit(messages []Message) []agentkit.Message {
	out := make([]agentkit.Message, 0, len(messages))
	for _, msg := range messages {
		out = append(out, agentkit.Message{
			Role:   agentkit.Role(msg.Role),
			Blocks: []agentkit.Block{agentkit.TextBlock{Text: msg.Content}},
		})
	}
	return out
}
