package script

import (
	"fmt"
	"path"
	"sort"
)

// trigger.go holds the static known-producer registry and the SetTrigger
// validation helpers (PLAN.md §A8/§A12). A trigger row's persistence lives in
// store.go; this file only decides whether a requested (source, event_filter)
// is satisfiable against the producers scripts can consume.

// knownProducers is the static map of consumable source -> the concrete event
// types that source publishes. cron is DYNAMIC: any "cron.*" filter is accepted
// (names come from the live crontab, not a compile-time set). scripts appears
// for self-chaining (§A12).
var knownProducers = map[string][]string{
	"cron":    nil, // dynamic: accept any event_filter matching "cron.*"
	"crm":     {"contact.created", "contact.updated", "contact.tagged", "contact.untagged"},
	"ledger":  {"transaction.recorded"},
	"dropbox": {"file.created", "file.modified", "file.deleted"},
	"prompts": {"run.succeeded", "run.failed"},
	// TODO(self-chaining): scripts may subscribe to its OWN /feed
	// ({scripts, "scripts.succeeded"}) for chaining (§A12). Day-one keeps the
	// five external upstreams only; adding self-chaining means a sixth consumer
	// loop pointed at the local /feed. When that lands, add:
	//   "scripts": {EventSucceeded, EventFailed},
}

// cronSource is the one DYNAMIC producer: its event types come from the live
// crontab, not a compile-time set, so any "cron.*" filter is accepted.
const cronSource = "cron"

// triggerSources returns the static set of consumable source ids (set_trigger
// validation + Service.TriggerSources).
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
