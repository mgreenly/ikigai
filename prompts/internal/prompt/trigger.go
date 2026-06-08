package prompt

import (
	"context"
	"fmt"
	"path"
	"sort"
)

// trigger.go holds the multi-source trigger STORE methods, the static
// known-producer registry, and the SetTrigger validation helpers (A5/A12). A
// trigger is one (prompt, source, event_filter) binding keyed by the composite
// PK; a prompt may hold many across N sources. The cron-only knobs are gone —
// fire-and-forget, symmetric with scripts.

// --- store methods (multi-source: keyed by (prompt_id, source, event_filter)) ---

// SetTrigger upserts one (prompt, source, event_filter) binding. A second call
// for the same composite key refreshes created_at; distinct (source,
// event_filter) values insert distinct rows (N triggers per prompt). Not
// owner-scoped: ownership is enforced by the service before this is called.
func (s *Store) SetTrigger(ctx context.Context, t Trigger) error {
	created := t.CreatedAt
	if created == "" {
		created = s.nowStr()
	}
	_, err := s.db.ExecContext(ctx,
		`INSERT INTO prompt_triggers (prompt_id, source, event_filter, created_at)
		 VALUES (?, ?, ?, ?)
		 ON CONFLICT(prompt_id, source, event_filter) DO UPDATE SET created_at = excluded.created_at`,
		t.PromptID, t.Source, t.EventFilter, created,
	)
	if err != nil {
		return fmt.Errorf("prompt: set trigger: %w", err)
	}
	return nil
}

// ClearTrigger removes one (prompt, source, event_filter) binding. A no-match
// returns ErrNotFound so the caller can distinguish "had no such binding" from a
// successful clear.
func (s *Store) ClearTrigger(ctx context.Context, promptID, source, eventFilter string) error {
	res, err := s.db.ExecContext(ctx,
		`DELETE FROM prompt_triggers WHERE prompt_id = ? AND source = ? AND event_filter = ?`,
		promptID, source, eventFilter,
	)
	if err != nil {
		return fmt.Errorf("prompt: clear trigger: %w", err)
	}
	return requireOne(res, "clear trigger")
}

// DeleteTriggers removes ALL of a prompt's bindings, returning no error when the
// prompt has none. Used by the tombstone Delete (which removes the prompt's
// bindings explicitly, since there is no FK cascade).
func (s *Store) DeleteTriggers(ctx context.Context, promptID string) error {
	if _, err := s.db.ExecContext(ctx,
		`DELETE FROM prompt_triggers WHERE prompt_id = ?`, promptID,
	); err != nil {
		return fmt.Errorf("prompt: delete triggers: %w", err)
	}
	return nil
}

// ListTriggers returns all of a prompt's bindings, ordered by (source,
// event_filter).
func (s *Store) ListTriggers(ctx context.Context, promptID string) ([]Trigger, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT prompt_id, source, event_filter, created_at
		   FROM prompt_triggers WHERE prompt_id = ?
		  ORDER BY source, event_filter`,
		promptID,
	)
	if err != nil {
		return nil, fmt.Errorf("prompt: list triggers: %w", err)
	}
	defer rows.Close()
	var out []Trigger
	for rows.Next() {
		var t Trigger
		if err := rows.Scan(&t.PromptID, &t.Source, &t.EventFilter, &t.CreatedAt); err != nil {
			return nil, fmt.Errorf("prompt: scan trigger row: %w", err)
		}
		out = append(out, t)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("prompt: list triggers rows: %w", err)
	}
	return out, nil
}

// PromptsForEvent returns the distinct prompt_ids whose binding source matches
// and whose event_filter glob-matches evType — the event→prompts fan-out the
// consumer runs on each arrival. NOT owner-scoped (the consumer has no caller
// identity); the box is single-owner. Uses the (source, event_filter) index.
func (s *Store) PromptsForEvent(ctx context.Context, source, evType string) ([]string, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT prompt_id, event_filter FROM prompt_triggers WHERE source = ?`,
		source,
	)
	if err != nil {
		return nil, fmt.Errorf("prompt: prompts for event: %w", err)
	}
	defer rows.Close()
	seen := map[string]bool{}
	var out []string
	for rows.Next() {
		var id, filter string
		if err := rows.Scan(&id, &filter); err != nil {
			return nil, fmt.Errorf("prompt: prompts for event scan: %w", err)
		}
		if seen[id] {
			continue
		}
		if globMatch(filter, evType) {
			seen[id] = true
			out = append(out, id)
		}
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("prompt: prompts for event rows: %w", err)
	}
	return out, nil
}

// --- known-producer registry (SetTrigger validation, A12) ---

// knownProducers is the static map of consumable source -> the concrete event
// types that source publishes. cron is DYNAMIC: any "cron.*" filter is accepted
// (names come from the live crontab, not a compile-time set). prompts appears
// for self-chaining (A12 — prompts' OWN feed).
var knownProducers = map[string][]string{
	"cron":    nil, // dynamic: accept any event_filter matching "cron.*"
	"crm":     {"contact.created", "contact.updated", "contact.tagged", "contact.untagged"},
	"ledger":  {"transaction.recorded"},
	"dropbox": {"file.created", "file.modified", "file.deleted"},
	"scripts": {"scripts.succeeded", "scripts.failed"},
	"prompts": {"run.succeeded", "run.failed"}, // self-chaining (A12); the consumer loop is a fast-follow TODO
}

// cronSource is the one DYNAMIC producer: its event types come from the live
// crontab, not a compile-time set, so any "cron.*" filter is accepted.
const cronSource = "cron"

// triggerSources returns the static set of consumable source ids (set_trigger
// validation + Service.TriggerSources), sorted for stable output.
func triggerSources() []string {
	out := make([]string, 0, len(knownProducers))
	for src := range knownProducers {
		out = append(out, src)
	}
	sort.Strings(out)
	return out
}

// validateTrigger checks source is a known producer and event_filter (a glob)
// can match at least one of that producer's published types (cron accepts any
// "cron.*"). Returns ErrValidation on an unsatisfiable binding.
func validateTrigger(source, eventFilter string) error {
	types, ok := knownProducers[source]
	if !ok {
		return fmt.Errorf("%w: unknown trigger source %q (known: %v)", ErrValidation, source, triggerSources())
	}
	if eventFilter == "" {
		return fmt.Errorf("%w: empty event_filter for source %q", ErrValidation, source)
	}
	// cron is dynamic: its event types are computed at runtime from the live
	// crontab, so accept any filter that can match the "cron.*" namespace.
	if source == cronSource {
		if globMatch(eventFilter, "cron.x") || globMatch("cron.*", eventFilter) {
			return nil
		}
		return fmt.Errorf("%w: event_filter %q cannot match any cron.* event", ErrValidation, eventFilter)
	}
	for _, t := range types {
		if globMatch(eventFilter, t) {
			return nil
		}
	}
	return fmt.Errorf("%w: event_filter %q matches no %s event type (known: %v)", ErrValidation, eventFilter, source, types)
}

// globMatch reports whether a glob pattern (e.g. "contact.*") matches a concrete
// event type. Uses path.Match semantics with "." as an ordinary character, so
// "contact.*" matches "contact.created" and "*" matches anything.
func globMatch(pattern, eventType string) bool {
	ok, err := path.Match(pattern, eventType)
	if err != nil {
		// A malformed pattern matches nothing.
		return false
	}
	return ok
}
