package repos

import (
	"context"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"

	"eventplane/consumer"
)

const (
	defaultGitHubHook = "github"
	defaultBotLogin   = "ikibot[bot]"
)

// Intake accepts GitHub webhook facts delivered through the event plane.
type Intake struct {
	store    *Store
	svc      *Service
	botLogin string
	log      *slog.Logger
}

// NewIntake constructs a GitHub-fact intake handler. An empty bot login uses
// the service default.
func NewIntake(store *Store, svc *Service, botLogin string, log *slog.Logger) *Intake {
	if botLogin == "" {
		botLogin = defaultBotLogin
	}
	if log == nil {
		log = slog.Default()
	}
	return &Intake{store: store, svc: svc, botLogin: botLogin, log: log}
}

// Subscriptions declares the single webhooks feed signal consumed by intake.
func Subscriptions(hookName string) []consumer.Subscription {
	if hookName == "" {
		hookName = defaultGitHubHook
	}
	return []consumer.Subscription{{
		Source:      "webhooks",
		Filter:      "webhooks:received/" + hookName,
		Description: "GitHub webhook deliveries that can trigger repository sessions.",
	}}
}

type webhookEnvelope struct {
	Name        string            `json:"name"`
	Owner       string            `json:"owner"`
	ReceivedAt  string            `json:"received_at"`
	ContentType string            `json:"content_type"`
	Body        string            `json:"body"`
	Headers     map[string]string `json:"headers"`
}

type ghDelivery struct {
	Action string `json:"action"`
	Issue  struct {
		Number int    `json:"number"`
		State  string `json:"state"`
	} `json:"issue"`
	Label struct {
		Name string `json:"name"`
	} `json:"label"`
	Sender struct {
		Login string `json:"login"`
	} `json:"sender"`
	Repository struct {
		Name          string `json:"name"`
		CloneURL      string `json:"clone_url"`
		DefaultBranch string `json:"default_branch"`
	} `json:"repository"`

	owner string
}

var ghHandlers = map[string]func(*Intake, context.Context, ghDelivery) error{
	"issues": (*Intake).handleIssues,
}

// Handle decodes and dispatches one webhooks received event.
func (in *Intake) Handle(ctx context.Context, ev consumer.Event) error {
	var envelope webhookEnvelope
	if err := json.Unmarshal(ev.Payload, &envelope); err != nil {
		return in.skip("decode webhooks payload", err)
	}
	eventName := envelope.Headers["x-github-event"]
	if eventName == "" {
		return in.skip("decode webhooks payload", errors.New("missing x-github-event header"))
	}
	body, err := base64.StdEncoding.DecodeString(envelope.Body)
	if err != nil {
		return in.skip("decode GitHub body", err)
	}
	var delivery ghDelivery
	if err := json.Unmarshal(body, &delivery); err != nil {
		return in.skip("decode GitHub delivery", err)
	}
	handler, ok := ghHandlers[eventName]
	if !ok {
		return nil
	}
	if delivery.Sender.Login == in.botLogin {
		return nil
	}
	delivery.owner = envelope.Owner
	return handler(in, ctx, delivery)
}

func (in *Intake) handleIssues(ctx context.Context, delivery ghDelivery) error {
	if delivery.Action != "labeled" || delivery.Label.Name != "execute" {
		return nil
	}
	if delivery.Issue.State != "open" {
		in.log.Info("ignoring execute label on closed issue", "repo", delivery.Repository.Name, "issue", delivery.Issue.Number)
		return nil
	}
	if _, err := in.store.ActiveSessionForIssue(ctx, delivery.Repository.Name, delivery.Issue.Number); err == nil {
		return nil
	} else if !errors.Is(err, ErrNotFound) {
		return fmt.Errorf("check active issue session: %w", err)
	}
	facts := RepoFacts{
		Name:          delivery.Repository.Name,
		CloneURL:      delivery.Repository.CloneURL,
		DefaultBranch: delivery.Repository.DefaultBranch,
		Owner:         delivery.owner,
	}
	if err := in.svc.EnsureRepo(ctx, facts); err != nil {
		return fmt.Errorf("provision delivery repository: %w", err)
	}
	attempt, err := in.store.MaxAttempt(ctx, facts.Name, delivery.Issue.Number)
	if err != nil {
		return fmt.Errorf("find issue attempt: %w", err)
	}
	id, err := intakeSessionID()
	if err != nil {
		return err
	}
	issue := delivery.Issue.Number
	session := Session{
		ID:           id,
		RepoName:     facts.Name,
		OwnerEmail:   delivery.owner,
		IssueNumber:  &issue,
		Attempt:      attempt + 1,
		Branch:       fmt.Sprintf("ikibot/issue-%d-%d", issue, attempt+1),
		Instructions: fmt.Sprintf("Resolve GitHub issue #%d.", issue),
		Status:       StatusQueued,
		CreatedAt:    in.svc.clock.Now(),
	}
	if err := in.store.InsertSession(ctx, session); err != nil {
		return fmt.Errorf("enqueue issue session: %w", err)
	}
	return nil
}

func intakeSessionID() (string, error) {
	var bytes [12]byte
	if _, err := rand.Read(bytes[:]); err != nil {
		return "", fmt.Errorf("create intake session id: %w", err)
	}
	return hex.EncodeToString(bytes[:]), nil
}

func (in *Intake) skip(action string, err error) error {
	in.log.Error(action, "error", err)
	return fmt.Errorf("%s: %v: %w", action, err, consumer.ErrSkip)
}
