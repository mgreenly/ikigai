package repos

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"path/filepath"
	"testing"
	"time"

	"eventplane/consumer"
)

func TestIntakeSubscriptionsUseConfiguredHook(t *testing.T) {
	// R-EQ4C-D8F5
	for _, test := range []struct {
		hook, filter string
	}{{"", "webhooks:received/github"}, {"alternate", "webhooks:received/alternate"}} {
		subs := Subscriptions(test.hook)
		if len(subs) != 1 || subs[0].Source != "webhooks" || subs[0].Filter != test.filter {
			t.Fatalf("Subscriptions(%q) = %#v", test.hook, subs)
		}
	}
}

func TestIntakeExecuteDeliveryProvisionsRepoAndQueuesSession(t *testing.T) {
	// R-ERC8-R05U
	fixture := newIntakeFixture(t, "")
	if err := fixture.intake.Handle(context.Background(), fixture.event(t, issueDelivery("alice", "open", "execute"))); err != nil {
		t.Fatalf("Handle: %v", err)
	}
	repo, err := fixture.store.GetRepo(context.Background(), "fixture")
	if err != nil || repo.OwnerEmail != "owner@example.com" {
		t.Fatalf("repo = %#v, %v", repo, err)
	}
	sessions, err := fixture.store.ListSessions(context.Background(), "fixture", "")
	if err != nil || len(sessions) != 1 {
		t.Fatalf("sessions = %#v, %v", sessions, err)
	}
	got := sessions[0]
	if got.Status != StatusQueued || got.OwnerEmail != "owner@example.com" || got.IssueNumber == nil || *got.IssueNumber != 42 || got.Attempt != 1 {
		t.Fatalf("queued session = %#v", got)
	}
}

func TestIntakeSuppressesConfiguredBotSender(t *testing.T) {
	// R-ESK5-4RWJ
	for _, test := range []struct {
		name, configured, sender string
	}{{"default", "", "ikibot[bot]"}, {"custom", "automation[bot]", "automation[bot]"}} {
		t.Run(test.name, func(t *testing.T) {
			fixture := newIntakeFixture(t, test.configured)
			if err := fixture.intake.Handle(context.Background(), fixture.event(t, issueDelivery(test.sender, "open", "execute"))); err != nil {
				t.Fatalf("Handle: %v", err)
			}
			if _, err := fixture.store.GetRepo(context.Background(), "fixture"); !errors.Is(err, ErrNotFound) {
				t.Fatalf("GetRepo error = %v, want ErrNotFound", err)
			}
			if sessions, err := fixture.store.ListSessions(context.Background(), "", ""); err != nil || len(sessions) != 0 {
				t.Fatalf("sessions = %#v, %v", sessions, err)
			}
		})
	}
	custom := newIntakeFixture(t, "automation[bot]")
	if err := custom.intake.Handle(context.Background(), custom.event(t, issueDelivery("ikibot[bot]", "open", "execute"))); err != nil {
		t.Fatalf("non-configured bot Handle: %v", err)
	}
	if sessions, err := custom.store.ListSessions(context.Background(), "fixture", ""); err != nil || len(sessions) != 1 {
		t.Fatalf("non-configured bot sessions = %#v, %v", sessions, err)
	}
}

func TestIntakeIgnoresClosedIssueAndOtherLabel(t *testing.T) {
	// R-ETS1-IJN8
	for _, test := range []struct{ name, state, label string }{
		{"closed", "closed", "execute"}, {"other label", "open", "executing"},
	} {
		t.Run(test.name, func(t *testing.T) {
			fixture := newIntakeFixture(t, "")
			if err := fixture.intake.Handle(context.Background(), fixture.event(t, issueDelivery("alice", test.state, test.label))); err != nil {
				t.Fatalf("Handle: %v", err)
			}
			if sessions, err := fixture.store.ListSessions(context.Background(), "", ""); err != nil || len(sessions) != 0 {
				t.Fatalf("sessions = %#v, %v", sessions, err)
			}
		})
	}
}

func TestIntakeGuardsActiveSessionAndIncrementsAfterTerminal(t *testing.T) {
	// R-EUZX-WBDX
	for _, status := range []string{StatusQueued, StatusRunning} {
		t.Run(status, func(t *testing.T) {
			fixture := newIntakeFixture(t, "")
			fixture.insertRepoAndSession(t, status, 2)
			if err := fixture.intake.Handle(context.Background(), fixture.event(t, issueDelivery("alice", "open", "execute"))); err != nil {
				t.Fatalf("Handle: %v", err)
			}
			sessions, _ := fixture.store.ListSessions(context.Background(), "fixture", "")
			if len(sessions) != 1 {
				t.Fatalf("session count = %d, want 1", len(sessions))
			}
		})
	}
	fixture := newIntakeFixture(t, "")
	fixture.insertRepoAndSession(t, StatusSucceeded, 2)
	if err := fixture.intake.Handle(context.Background(), fixture.event(t, issueDelivery("alice", "open", "execute"))); err != nil {
		t.Fatalf("terminal Handle: %v", err)
	}
	sessions, _ := fixture.store.ListSessions(context.Background(), "fixture", "")
	if len(sessions) != 2 || sessions[1].Attempt != 3 || sessions[1].Status != StatusQueued {
		t.Fatalf("sessions after terminal = %#v", sessions)
	}
}

func TestIntakeClassifiesPoisonUnknownAndDatabaseFailure(t *testing.T) {
	// R-EW7U-A34M
	fixture := newIntakeFixture(t, "")
	badBody, _ := json.Marshal(webhookEnvelope{Owner: "owner@example.com", Body: "%%%", Headers: map[string]string{"x-github-event": "issues"}})
	badJSONBody, _ := json.Marshal(webhookEnvelope{Owner: "owner@example.com", Body: base64.StdEncoding.EncodeToString([]byte(`not-json`)), Headers: map[string]string{"x-github-event": "issues"}})
	missingHeader, _ := json.Marshal(webhookEnvelope{Owner: "owner@example.com", Body: base64.StdEncoding.EncodeToString([]byte(`{}`))})
	for _, payload := range [][]byte{[]byte(`not-json`), badBody, badJSONBody, missingHeader} {
		err := fixture.intake.Handle(context.Background(), consumer.Event{Payload: payload})
		if !errors.Is(err, consumer.ErrSkip) {
			t.Errorf("poison error = %v, want ErrSkip", err)
		}
	}
	unknown := fixture.eventNamed(t, "push", issueDelivery("alice", "open", "execute"))
	if err := fixture.intake.Handle(context.Background(), unknown); err != nil {
		t.Fatalf("unknown event error = %v", err)
	}
	closed := newIntakeFixture(t, "")
	if err := closed.db.Close(); err != nil {
		t.Fatalf("close database: %v", err)
	}
	err := closed.intake.Handle(context.Background(), closed.event(t, issueDelivery("alice", "open", "execute")))
	if err == nil || errors.Is(err, consumer.ErrSkip) {
		t.Fatalf("database error = %v, want non-ErrSkip error", err)
	}
}

type intakeFixture struct {
	intake *Intake
	store  *Store
	db     interface{ Close() error }
	remote string
	clock  fixedClock
}

func newIntakeFixture(t *testing.T, botLogin string) intakeFixture {
	t.Helper()
	store, db := migratedStore(t)
	remote := newBareRemote(t)
	clock := fixedClock{time.Date(2026, 7, 15, 14, 0, 0, 0, time.UTC)}
	service := NewService(store, NewGit(filepath.Join(t.TempDir(), "repos"), &staticTokenSource{token: "fixture"}), clock, "fixture-org")
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	return intakeFixture{NewIntake(store, service, botLogin, logger), store, db, remote, clock}
}

func (f intakeFixture) event(t *testing.T, delivery map[string]any) consumer.Event {
	t.Helper()
	return f.eventNamed(t, "issues", delivery)
}

func (f intakeFixture) eventNamed(t *testing.T, name string, delivery map[string]any) consumer.Event {
	t.Helper()
	delivery["repository"] = map[string]any{"name": "fixture", "clone_url": fileURL(f.remote), "default_branch": "main"}
	body, err := json.Marshal(delivery)
	if err != nil {
		t.Fatalf("marshal delivery: %v", err)
	}
	payload, err := json.Marshal(webhookEnvelope{Owner: "owner@example.com", Body: base64.StdEncoding.EncodeToString(body), Headers: map[string]string{"x-github-event": name}})
	if err != nil {
		t.Fatalf("marshal envelope: %v", err)
	}
	return consumer.Event{Payload: payload}
}

func (f intakeFixture) insertRepoAndSession(t *testing.T, status string, attempt int) {
	t.Helper()
	ctx := context.Background()
	repo := Repo{Name: "fixture", OwnerEmail: "owner@example.com", CloneURL: fileURL(f.remote), DefaultBranch: "main", CreatedAt: f.clock.value}
	if err := f.store.InsertRepo(ctx, repo); err != nil {
		t.Fatalf("insert repo: %v", err)
	}
	issue := 42
	insertSession(t, f.store, Session{ID: "existing-" + status, RepoName: repo.Name, OwnerEmail: repo.OwnerEmail, IssueNumber: &issue,
		Attempt: attempt, Branch: "existing", Instructions: "existing", Status: status, CreatedAt: f.clock.value.Add(-time.Hour)})
}

func issueDelivery(sender, state, label string) map[string]any {
	return map[string]any{
		"action": "labeled", "issue": map[string]any{"number": 42, "state": state},
		"label": map[string]any{"name": label}, "sender": map[string]any{"login": sender},
	}
}
