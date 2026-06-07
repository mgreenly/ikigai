package session

import (
	"context"
	"errors"
	"testing"
)

// TestService_SetTrigger_DefaultsAndOwnership covers the Service layer: defaults
// fill when knobs are <=0, an empty trigger_event is a ValidationError, and a
// foreign-owned session is ErrNotFound (ownership enforced before any write).
func TestService_SetTrigger_DefaultsAndOwnership(t *testing.T) {
	ctx := context.Background()
	svc, store, _, _ := newTestService(t)
	sess := seedSession(t, store, "owner@example.com", StatusIdle)

	// Defaults applied.
	trig, err := svc.SetTrigger(ctx, "owner@example.com", sess.ID, SetTriggerInput{TriggerEvent: "cron.nightly"})
	if err != nil {
		t.Fatalf("SetTrigger: %v", err)
	}
	if trig.MaxStalenessSecs != DefaultMaxStalenessSecs || trig.MaxAttempts != DefaultMaxAttempts {
		t.Fatalf("defaults not applied: %+v", trig)
	}

	// Empty trigger_event rejected.
	if _, err := svc.SetTrigger(ctx, "owner@example.com", sess.ID, SetTriggerInput{}); err == nil {
		t.Fatalf("expected validation error for empty trigger_event")
	} else {
		var ve *ValidationError
		if !errors.As(err, &ve) {
			t.Fatalf("expected ValidationError, got %T: %v", err, err)
		}
	}

	// Foreign owner cannot set/clear.
	if _, err := svc.SetTrigger(ctx, "intruder@example.com", sess.ID, SetTriggerInput{TriggerEvent: "cron.x"}); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound for foreign owner, got %v", err)
	}
	if err := svc.ClearTrigger(ctx, "intruder@example.com", sess.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound clearing foreign session, got %v", err)
	}

	// Owner clear works.
	if err := svc.ClearTrigger(ctx, "owner@example.com", sess.ID); err != nil {
		t.Fatalf("owner ClearTrigger: %v", err)
	}
}

// TestService_RunByID_NoOwnerScopeAndSingleFlight asserts the triggered run path
// starts a run without owner scoping and is the single-flight gate (ErrBusy when
// the session is already running — the serialization mechanism).
func TestService_RunByID_NoOwnerScopeAndSingleFlight(t *testing.T) {
	ctx := context.Background()
	svc, store, _, _ := newTestService(t)
	sess := seedSession(t, store, "owner@example.com", StatusIdle)

	run, err := svc.RunByID(ctx, sess.ID)
	if err != nil {
		t.Fatalf("RunByID: %v", err)
	}
	if run.SessionID != sess.ID || run.Status != RunRunning {
		t.Fatalf("unexpected run: %+v", run)
	}

	// fakeRunner does not auto-complete, so the session is still running →
	// second RunByID is ErrBusy.
	if _, err := svc.RunByID(ctx, sess.ID); !errors.Is(err, ErrBusy) {
		t.Fatalf("expected ErrBusy on second RunByID, got %v", err)
	}

	// Unknown session → ErrNotFound.
	if _, err := svc.RunByID(ctx, "nope"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound for unknown session, got %v", err)
	}
}
