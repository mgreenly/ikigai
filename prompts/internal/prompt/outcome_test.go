package prompt

import (
	"context"
	"database/sql"
	"encoding/json"
	"testing"

	"eventplane/outbox"

	"prompts/internal/ids"
)

// newProducerStore stands up a migrated prompts DB (which now includes the outbox
// table via 005_outbox.sql) with a real outbox wired onto the Store, so FinishRun
// exercises the SAME-tx terminal-write + outbox.Append path. Events is passed as
// the registry so an unregistered type would be rejected at Append.
func newProducerStore(t *testing.T) (*Store, *sql.DB) {
	t.Helper()
	ctx := context.Background()
	conn := openMigratedTestDB(t, ctx)
	// DBPath empty → skip the §5.3 concurrency probe (single shared handle).
	ob, err := outbox.New(conn, outbox.Options{Source: "prompts", Registry: Events})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	store := NewStore(conn)
	store.Outbox = ob
	return store, conn
}

// seedRunningRun seeds a running session + run for a terminal-write test. The
// outcome-event fields (prompt_id, prompt_name, trigger context) now live on
// the run row — FinishRun reads them from there — so they are pinned at seed.
func seedRunningRun(t *testing.T, store *Store, name string) (Prompt, Run) {
	return seedRunningRunTrig(t, store, name, "", "", "", "")
}

func seedRunningRunTrig(t *testing.T, store *Store, name, triggerSource, triggerKind, triggerSubject, triggerEventID string) (Prompt, Run) {
	t.Helper()
	ctx := context.Background()
	now := store.nowStr()
	sess := Prompt{
		ID:         ids.NewULID(),
		OwnerEmail: "owner@example.com",
		Name:       name,
		UserPrompt: "p",
		Config:     Config{Provider: "anthropic", Model: "claude-haiku-4-5"},
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	if err := store.InsertPrompt(ctx, sess); err != nil {
		t.Fatalf("InsertPrompt: %v", err)
	}
	run := Run{
		ID:             ids.NewULID(),
		PromptID:       sess.ID,
		OwnerEmail:     sess.OwnerEmail,
		PromptName:     sess.Name,
		Status:         RunRunning,
		StartedAt:      now,
		LogPath:        "x",
		TriggerSource:  triggerSource,
		TriggerKind:    triggerKind,
		TriggerSubject: triggerSubject,
		TriggerEventID: triggerEventID,
	}
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	return sess, run
}

// outboxRows reads every (kind, payload) from the outbox table in seq order.
func outboxRows(t *testing.T, conn *sql.DB) []struct{ Kind, Payload string } {
	t.Helper()
	rows, err := conn.Query(`SELECT kind, payload FROM outbox ORDER BY seq`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	var out []struct{ Kind, Payload string }
	for rows.Next() {
		var typ, payload string
		if err := rows.Scan(&typ, &payload); err != nil {
			t.Fatalf("scan outbox: %v", err)
		}
		out = append(out, struct{ Kind, Payload string }{typ, payload})
	}
	return out
}

// decodePayload unmarshals an outcome payload into a generic map so a test can
// assert exact field presence/values.
func decodePayload(t *testing.T, raw string) map[string]any {
	t.Helper()
	var m map[string]any
	if err := json.Unmarshal([]byte(raw), &m); err != nil {
		t.Fatalf("decode payload %q: %v", raw, err)
	}
	return m
}

// TestFinishRun_SuccessEmitsRunSucceeded: the success path emits exactly one
// run.succeeded, in the SAME tx as the terminal write — the run row is succeeded
// AND the outbox holds exactly one event, committed together. trigger_source /
// trigger_type / trigger_event_id are sourced from the (cron-triggered) run;
// error is absent.
func TestFinishRun_SuccessEmitsRunSucceeded(t *testing.T) {
	store, conn := newProducerStore(t)
	ctx := context.Background()
	sess, run := seedRunningRunTrig(t, store, "nightly scan", "cron", "tick", "/nightly", "ev-001")

	if err := store.FinishRun(ctx, FinishRunInput{
		RunID:     run.ID,
		Status:    RunSucceeded,
		EndedAt:   store.nowStr(),
		UsageJSON: `{"tokens":5}`,
	}); err != nil {
		t.Fatalf("FinishRun: %v", err)
	}

	// Terminal write committed.
	got, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got.Status != RunSucceeded || got.EndedAt == "" {
		t.Fatalf("terminal write not committed: %+v", got)
	}

	// Exactly one event, the right type, committed alongside the terminal write.
	evs := outboxRows(t, conn)
	if len(evs) != 1 {
		t.Fatalf("expected exactly one outbox event, got %d: %+v", len(evs), evs)
	}
	if evs[0].Kind != EventRunSucceeded {
		t.Fatalf("kind = %q, want %q", evs[0].Kind, EventRunSucceeded)
	}
	p := decodePayload(t, evs[0].Payload)
	if p["prompt_id"] != sess.ID || p["prompt_name"] != "nightly scan" {
		t.Fatalf("identity fields wrong: %+v", p)
	}
	if p["trigger_source"] != "cron" || p["trigger_kind"] != "tick" || p["trigger_subject"] != "/nightly" || p["trigger_event_id"] != "ev-001" {
		t.Fatalf("trigger context wrong: %+v", p)
	}
	if p["run_id"] != run.ID {
		t.Fatalf("run_id wrong: got %v, want %s", p["run_id"], run.ID)
	}
	if _, ok := p["error"]; ok {
		t.Fatalf("success payload must omit error: %+v", p)
	}
}

// TestFinishRun_FailureEmitsRunFailedWithError: the failure path emits exactly
// one run.failed carrying the terminal error string in the payload.
func TestFinishRun_FailureEmitsRunFailedWithError(t *testing.T) {
	store, conn := newProducerStore(t)
	ctx := context.Background()
	_, run := seedRunningRunTrig(t, store, "nightly scan", "cron", "tick", "/nightly", "ev-001")

	if err := store.FinishRun(ctx, FinishRunInput{
		RunID:   run.ID,
		Status:  RunFailed,
		EndedAt: store.nowStr(),
		ErrMsg:  "run TTL exceeded",
	}); err != nil {
		t.Fatalf("FinishRun: %v", err)
	}

	evs := outboxRows(t, conn)
	if len(evs) != 1 || evs[0].Kind != EventRunFailed {
		t.Fatalf("expected one run.failed, got %+v", evs)
	}
	p := decodePayload(t, evs[0].Payload)
	if p["error"] != "run TTL exceeded" {
		t.Fatalf("failure payload must carry error: %+v", p)
	}
}

// TestFinishRun_ManualRunEmptyTriggerContext: a run that was NOT cron-triggered
// (a manual MCP run) emits its outcome with empty trigger_event / scheduled_for —
// the documented manual-run representation (event-triggering decisions §3).
func TestFinishRun_ManualRunEmptyTriggerContext(t *testing.T) {
	store, conn := newProducerStore(t)
	ctx := context.Background()
	_, run := seedRunningRun(t, store, "manual task") // no trigger context → manual run

	if err := store.FinishRun(ctx, FinishRunInput{
		RunID:   run.ID,
		Status:  RunSucceeded,
		EndedAt: store.nowStr(),
	}); err != nil {
		t.Fatalf("FinishRun: %v", err)
	}
	evs := outboxRows(t, conn)
	if len(evs) != 1 {
		t.Fatalf("expected one event, got %+v", evs)
	}
	p := decodePayload(t, evs[0].Payload)
	if p["trigger_source"] != "" || p["trigger_kind"] != "" || p["trigger_subject"] != "" || p["trigger_event_id"] != "" {
		t.Fatalf("manual run must carry empty trigger context: %+v", p)
	}
}

// TestFinishRun_CancelledEmitsNoEvent: a cancelled run writes its terminal state
// but emits NEITHER outcome type (cancel is operator-initiated, not an outcome
// a consumer announces).
func TestFinishRun_CancelledEmitsNoEvent(t *testing.T) {
	store, conn := newProducerStore(t)
	ctx := context.Background()
	sess, run := seedRunningRun(t, store, "cancelled task")

	if err := store.FinishRun(ctx, FinishRunInput{
		RunID:   run.ID,
		Status:  RunCancelled,
		EndedAt: store.nowStr(),
		ErrMsg:  "cancelled",
	}); err != nil {
		t.Fatalf("FinishRun: %v", err)
	}
	got, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got.Status != RunCancelled {
		t.Fatalf("terminal write not applied: %+v", got)
	}
	if evs := outboxRows(t, conn); len(evs) != 0 {
		t.Fatalf("cancelled run must emit no event, got %+v", evs)
	}
}

// TestFinishRun_AppendFailureRollsBackTerminalWrite is the atomicity invariant:
// if the outbox Append fails, the run's terminal-state write MUST roll back too
// (they share one tx). We force the Append failure with a registry that does NOT
// contain the outcome types, so Outbox.Append rejects the unregistered type. The
// run must remain 'running' — nothing committed.
func TestFinishRun_AppendFailureRollsBackTerminalWrite(t *testing.T) {
	store, conn := newProducerStore(t)
	ctx := context.Background()
	_, run := seedRunningRun(t, store, "atomic task")

	// Swap in an outbox whose registry rejects run.* — Append will fail.
	bad, err := outbox.New(conn, outbox.Options{
		Source:   "prompts",
		Registry: outbox.Registry{{Kind: "other.thing", Description: "x", Sample: map[string]string{}}},
	})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	store.Outbox = bad

	if err := store.FinishRun(ctx, FinishRunInput{
		RunID:   run.ID,
		Status:  RunSucceeded,
		EndedAt: store.nowStr(),
	}); err == nil {
		t.Fatal("FinishRun must fail when the outbox Append is rejected")
	}

	// Atomicity: the terminal write rolled back with the failed Append.
	got, err := store.GetLatestRun(ctx, run.PromptID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got.Status != RunRunning || got.EndedAt != "" {
		t.Fatalf("terminal write must have rolled back, run = %+v", got)
	}
	if evs := outboxRows(t, conn); len(evs) != 0 {
		t.Fatalf("no event must be committed on rollback, got %+v", evs)
	}
}

// TestFinishRun_NilOutboxIsPureTerminalWrite: with no producer wired (Outbox
// nil), FinishRun still writes the terminal state (the runner path before P8 and
// any non-producer build).
func TestFinishRun_NilOutboxIsPureTerminalWrite(t *testing.T) {
	store := newTestStore(t)
	ctx := context.Background()
	sess := seedPrompt(t, store, "owner@example.com")
	run := Run{ID: ids.NewULID(), PromptID: sess.ID, OwnerEmail: sess.OwnerEmail, PromptName: sess.Name, Status: RunRunning, StartedAt: store.nowStr(), LogPath: "x"}
	if err := store.InsertRun(ctx, run); err != nil {
		t.Fatalf("InsertRun: %v", err)
	}
	if err := store.FinishRun(ctx, FinishRunInput{
		RunID:   run.ID,
		Status:  RunSucceeded,
		EndedAt: store.nowStr(),
	}); err != nil {
		t.Fatalf("FinishRun (nil outbox): %v", err)
	}
	got, err := store.GetLatestRun(ctx, sess.ID)
	if err != nil {
		t.Fatalf("GetLatestRun: %v", err)
	}
	if got.Status != RunSucceeded {
		t.Fatalf("terminal write not applied: %+v", got)
	}
}
