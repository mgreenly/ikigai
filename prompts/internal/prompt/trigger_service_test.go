package prompt

import (
	"context"
	"errors"
	"testing"
)

// TestService_SetTrigger_ValidationAndOwnership covers the Service layer:
// validation rejects an unknown source and an implausible event_filter, a
// foreign-owned prompt is ErrNotFound (ownership enforced before any write), and
// a valid binding upserts and is listed.
func TestService_SetTrigger_ValidationAndOwnership(t *testing.T) {
	ctx := context.Background()
	svc, store, _, _ := newTestService(t)
	p := seedPrompt(t, store, "owner@example.com")

	// Valid binding on a real source.
	trig, err := svc.SetTrigger(ctx, "owner@example.com", p.ID, "dropbox", "file.created")
	if err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}
	if trig.Source != "dropbox" || trig.EventFilter != "file.created" || trig.CreatedAt == "" {
		t.Fatalf("unexpected trigger: %+v", trig)
	}

	// A second valid binding on a different source is additive (multi-source).
	if _, err := svc.SetTrigger(ctx, "owner@example.com", p.ID, "scripts", "scripts.succeeded"); err != nil {
		t.Fatalf("second SetTrigger: %v", err)
	}
	got, err := store.ListTriggers(ctx, p.ID)
	if err != nil {
		t.Fatalf("ListTriggers: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("expected 2 bindings, got %d: %+v", len(got), got)
	}

	// Unknown source rejected with ErrValidation.
	if _, err := svc.SetTrigger(ctx, "owner@example.com", p.ID, "nope", "x"); !errors.Is(err, ErrValidation) {
		t.Fatalf("expected ErrValidation for unknown source, got %v", err)
	}
	// An event_filter the producer never publishes is rejected.
	if _, err := svc.SetTrigger(ctx, "owner@example.com", p.ID, "ledger", "file.created"); !errors.Is(err, ErrValidation) {
		t.Fatalf("expected ErrValidation for implausible event_filter, got %v", err)
	}
	// cron is dynamic: any cron.* filter is accepted.
	if _, err := svc.SetTrigger(ctx, "owner@example.com", p.ID, "cron", "cron.nightly"); err != nil {
		t.Fatalf("cron.nightly should be accepted, got %v", err)
	}

	// Foreign owner cannot set/clear.
	if _, err := svc.SetTrigger(ctx, "intruder@example.com", p.ID, "cron", "cron.nightly"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound for foreign owner, got %v", err)
	}
	if err := svc.ClearTrigger(ctx, "intruder@example.com", p.ID, "cron", "cron.nightly"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound clearing foreign prompt, got %v", err)
	}

	// Owner clear of one binding works.
	if err := svc.ClearTrigger(ctx, "owner@example.com", p.ID, "dropbox", "file.created"); err != nil {
		t.Fatalf("owner ClearTrigger: %v", err)
	}
}

// TestService_TriggerSources asserts the static known-producer set is returned.
func TestService_TriggerSources(t *testing.T) {
	svc, _, _, _ := newTestService(t)
	got := svc.TriggerSources()
	want := map[string]bool{"cron": true, "crm": true, "ledger": true, "dropbox": true, "scripts": true, "prompts": true}
	if len(got) != len(want) {
		t.Fatalf("TriggerSources len = %d, want %d: %v", len(got), len(want), got)
	}
	for _, s := range got {
		if !want[s] {
			t.Fatalf("unexpected source %q in %v", s, got)
		}
	}
}

// TestService_Create_InlineTriggers asserts create-time trigger sugar: valid
// bindings are applied after insert, and an invalid binding rejects the WHOLE
// create (no orphan prompt).
func TestService_Create_InlineTriggers(t *testing.T) {
	t.Setenv("ANTHROPIC_API_KEY", "sk-test")
	ctx := context.Background()
	svc, store, _, _ := newTestService(t)

	p, err := svc.Create(ctx, "owner@example.com", CreateInput{
		UserPrompt: "do the thing",
		Config:     Config{Model: "haiku"},
		Triggers: []TriggerSpec{
			{Source: "dropbox", EventFilter: "file.created"},
			{Source: "cron", EventFilter: "cron.nightly"},
		},
	})
	if err != nil {
		t.Fatalf("Create with triggers: %v", err)
	}
	got, err := store.ListTriggers(ctx, p.ID)
	if err != nil {
		t.Fatalf("ListTriggers: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("expected 2 inline bindings, got %d: %+v", len(got), got)
	}

	// An invalid inline trigger rejects the whole create — no prompt row written.
	_, err = svc.Create(ctx, "owner@example.com", CreateInput{
		UserPrompt: "bad",
		Config:     Config{Model: "haiku"},
		Triggers:   []TriggerSpec{{Source: "ledger", EventFilter: "file.created"}},
	})
	if !errors.Is(err, ErrValidation) {
		t.Fatalf("expected ErrValidation for invalid inline trigger, got %v", err)
	}
	all, err := store.ListPrompts(ctx, "owner@example.com")
	if err != nil {
		t.Fatalf("ListPrompts: %v", err)
	}
	if len(all) != 1 {
		t.Fatalf("expected exactly 1 prompt (the rejected create left no orphan), got %d", len(all))
	}
}

// TestService_RunByEvent_PopulatesTriggerContext asserts the event-triggered run
// path starts a run without owner scoping, is ALWAYS accepted (full concurrency),
// and the run row carries the trigger context (source/type/event_id).
func TestService_RunByEvent_PopulatesTriggerContext(t *testing.T) {
	ctx := context.Background()
	svc, store, _, _ := newTestService(t)
	p := seedPrompt(t, store, "owner@example.com")

	run, err := svc.RunByEvent(ctx, p.ID, "dropbox", "file.created", "ev-123", []byte(`{"path":"/x"}`))
	if err != nil {
		t.Fatalf("RunByEvent: %v", err)
	}
	if run.PromptID != p.ID || run.Status != RunRunning {
		t.Fatalf("unexpected run: %+v", run)
	}
	if run.TriggerSource != "dropbox" || run.TriggerType != "file.created" || run.TriggerEventID != "ev-123" {
		t.Fatalf("trigger context not carried on run: %+v", run)
	}

	// Persisted: re-read the run row and confirm the columns landed.
	got, err := store.GetRun(ctx, run.ID)
	if err != nil {
		t.Fatalf("GetRun: %v", err)
	}
	if got.TriggerSource != "dropbox" || got.TriggerType != "file.created" || got.TriggerEventID != "ev-123" {
		t.Fatalf("trigger context not persisted: %+v", got)
	}

	// Full concurrency: a second event run of the same prompt is accepted and
	// yields a distinct run.
	run2, err := svc.RunByEvent(ctx, p.ID, "", "", "", nil)
	if err != nil {
		t.Fatalf("second RunByEvent: want success, got %v", err)
	}
	if run2.ID == run.ID {
		t.Fatalf("second RunByEvent shares a run_id: %s", run2.ID)
	}

	// Unknown prompt → ErrNotFound.
	if _, err := svc.RunByEvent(ctx, "nope", "", "", "", nil); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound for unknown prompt, got %v", err)
	}
}
