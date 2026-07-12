package prompt

import (
	"context"
	"fmt"
	"sort"
	"strings"

	"eventplane/outbox"
	"eventplane/routing"
)

// SetTrigger upserts a canonical-key filter. Source is derived by Service after
// validation and retained solely for the per-upstream lookup index.
func (s *Store) SetTrigger(ctx context.Context, t Trigger) error {
	created := t.CreatedAt
	if created == "" {
		created = s.nowStr()
	}
	_, err := s.db.ExecContext(ctx, `INSERT INTO prompt_triggers (prompt_id, source, filter, created_at)
		VALUES (?, ?, ?, ?) ON CONFLICT(prompt_id, filter) DO UPDATE SET created_at = excluded.created_at`,
		t.PromptID, t.Source, t.Filter, created)
	if err != nil {
		return fmt.Errorf("prompt: set trigger: %w", err)
	}
	return nil
}

func (s *Store) ClearTrigger(ctx context.Context, promptID, filter string) error {
	res, err := s.db.ExecContext(ctx, `DELETE FROM prompt_triggers WHERE prompt_id = ? AND filter = ?`, promptID, filter)
	if err != nil {
		return fmt.Errorf("prompt: clear trigger: %w", err)
	}
	return requireOne(res, "clear trigger")
}

func (s *Store) DeleteTriggers(ctx context.Context, promptID string) error {
	if _, err := s.db.ExecContext(ctx, `DELETE FROM prompt_triggers WHERE prompt_id = ?`, promptID); err != nil {
		return fmt.Errorf("prompt: delete triggers: %w", err)
	}
	return nil
}

func (s *Store) ListTriggers(ctx context.Context, promptID string) ([]Trigger, error) {
	rows, err := s.db.QueryContext(ctx, `SELECT prompt_id, source, filter, created_at FROM prompt_triggers WHERE prompt_id = ? ORDER BY source, filter`, promptID)
	if err != nil {
		return nil, fmt.Errorf("prompt: list triggers: %w", err)
	}
	defer rows.Close()
	var out []Trigger
	for rows.Next() {
		var t Trigger
		if err := rows.Scan(&t.PromptID, &t.Source, &t.Filter, &t.CreatedAt); err != nil {
			return nil, fmt.Errorf("prompt: scan trigger row: %w", err)
		}
		out = append(out, t)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("prompt: list triggers rows: %w", err)
	}
	return out, nil
}

func (s *Store) PromptsForEvent(ctx context.Context, source, key string) ([]string, error) {
	rows, err := s.db.QueryContext(ctx, `SELECT prompt_id, filter FROM prompt_triggers WHERE source = ?`, source)
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
		ok, err := routing.Match(filter, key)
		if err == nil && ok && !seen[id] {
			seen[id] = true
			out = append(out, id)
		}
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("prompt: prompts for event rows: %w", err)
	}
	return out, nil
}

var knownFamilies = map[string][]string{
	"cron": {"tick"}, "crm": {"contact.created", "contact.updated", "contact.tagged", "contact.untagged"},
	"ledger": {"recorded"}, "dropbox": {"create", "modify", "delete"}, "scripts": {"succeeded", "failed"}, "prompts": {"run.succeeded", "run.failed"},
}

func triggerSources() []string {
	out := make([]string, 0, len(knownFamilies))
	for src := range knownFamilies {
		out = append(out, src)
	}
	sort.Strings(out)
	return out
}

// validateTrigger enforces a literal consumed source and asks eventplane's
// family registry whether the full canonical-key filter is satisfiable.
func validateTrigger(filter string) (string, error) {
	i := strings.IndexByte(filter, ':')
	if i <= 0 {
		return "", fmt.Errorf("%w: filter must start with a literal source followed by ':'", ErrValidation)
	}
	source := filter[:i]
	if strings.ContainsAny(source, "*?[") {
		return "", fmt.Errorf("%w: trigger source %q must be literal", ErrValidation, source)
	}
	kinds, ok := knownFamilies[source]
	if !ok {
		return "", fmt.Errorf("%w: unknown trigger source %q (known: %v)", ErrValidation, source, triggerSources())
	}
	registry := make(outbox.Registry, 0, len(kinds))
	for _, kind := range kinds {
		registry = append(registry, outbox.Family{Kind: kind})
	}
	ok, err := registry.CouldMatch(source, filter)
	if err != nil {
		return "", fmt.Errorf("%w: invalid filter %q: %v", ErrValidation, filter, err)
	}
	if !ok {
		return "", fmt.Errorf("%w: filter %q matches no %s event family", ErrValidation, filter, source)
	}
	return source, nil
}
