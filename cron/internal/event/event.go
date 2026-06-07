// Package event owns cron's event contract: the cron.<name> type namespace, the
// shared {name, scheduled_for, fired_at} payload shape, and the LIVE published-
// type provider (Spec.Publishes) that reads the crontab at reflection time
// (event-triggering decisions §2).
//
// cron is the dynamic producer the P3 Publishes seam was built for: its
// emittable types are not compile-time payload structs (as crm/ledger's are) but
// "cron."+<name> derived from the current crontab rows. Append-time validation
// is therefore NONE (an empty Spec.Events registry, so the outbox imposes no
// per-emit registry guard); the type is valid by construction — the tick worker
// emits "cron."+name from a charset-CHECKed row, so the boundary guarantee lives
// at the DB CHECK, not a registry.
package event

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"cron/internal/crontab"

	"eventplane/outbox"
)

// TypePrefix is the namespace cron owns. The emitted type is TypePrefix + the
// crontab row's name (decisions §2 — "cron owns the cron.* type namespace").
const TypePrefix = "cron."

// timeFormat is the canonical wire timestamp shape (UTC RFC3339), matching the
// repo's other event payloads (crm/ledger render times this way).
const timeFormat = time.RFC3339

// Type returns the wire event type for a crontab entry name: "cron."+name.
func Type(name string) string { return TypePrefix + name }

// Payload is the cron.<name> event payload (decisions §2). Field names and the
// UTC-RFC3339 time format are the wire contract; ScheduledFor is the matched
// minute slot, FiredAt the actual emit time (the two diverge after a restart
// blink — the staleness signal a consumer uses).
type Payload struct {
	Name         string `json:"name"`
	ScheduledFor string `json:"scheduled_for"`
	FiredAt      string `json:"fired_at"`
}

// Build marshals the cron.<name> event for one fire: name from the crontab row,
// scheduledFor the matched slot, firedAt the emit time. Both times are rendered
// in the canonical UTC-RFC3339 wire format.
func Build(name string, scheduledFor, firedAt time.Time) (outbox.Event, error) {
	raw, err := json.Marshal(Payload{
		Name:         name,
		ScheduledFor: scheduledFor.UTC().Format(timeFormat),
		FiredAt:      firedAt.UTC().Format(timeFormat),
	})
	if err != nil {
		return outbox.Event{}, fmt.Errorf("marshal cron.%s payload: %w", name, err)
	}
	return outbox.Event{Type: Type(name), Payload: raw}, nil
}

// sample is the reflection example/schema source for every live cron.<name>
// type — they all share this one payload shape.
var sample = Payload{
	Name:         "nightly",
	ScheduledFor: "2026-06-06T03:00:00Z",
	FiredAt:      "2026-06-06T03:00:00Z",
}

// Publishes returns a LIVE provider for Spec.Publishes: it reads the crontab at
// reflection time and returns one EventType per row, so reflection reports the
// live cron.foo, cron.bar, … (decisions §2). Each entry carries the one shared
// payload Sample, so every type reflects the same schema/example. A read failure
// yields an empty registry (reflection degrades to "nothing published" rather
// than erroring the tool); the crontab list MCP tool is the authoritative
// instance view regardless.
func Publishes(store *crontab.Store) func() outbox.Registry {
	return func() outbox.Registry {
		entries, err := store.List(context.Background())
		if err != nil {
			return outbox.Registry{}
		}
		reg := make(outbox.Registry, 0, len(entries))
		for _, e := range entries {
			reg = append(reg, outbox.EventType{
				Type:        Type(e.Name),
				Description: fmt.Sprintf("Fires on each minute matching the %q schedule (expr %q). Payload carries the matched slot and emit time.", e.Name, e.Expr),
				Sample:      sample,
			})
		}
		return reg
	}
}
