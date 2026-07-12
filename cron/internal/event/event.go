// Package event owns cron's event contract: kind tick with a /<schedule name>
// subject, the shared {name, scheduled_for, fired_at} payload shape, and the
// LIVE published-family provider (Spec.Publishes) that reads the crontab at
// reflection time.
//
// cron is the dynamic producer the P3 Publishes seam was built for: its
// emittable subjects are derived from the current crontab rows. Append-time
// validation is therefore NONE (an empty Spec.Events registry); the subject is
// valid by construction because the tick worker emits /+name from a
// charset-CHECKed row, so the boundary guarantee lives at the DB CHECK.
package event

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"cron/internal/crontab"

	"eventplane/outbox"
)

// Kind is cron's one fact class: a schedule fired for a minute slot.
const Kind = "tick"

// timeFormat is the canonical wire timestamp shape (UTC RFC3339), matching the
// repo's other event payloads (crm/ledger render times this way).
const timeFormat = time.RFC3339

// Subject renders a schedule name as its routing subject.
func Subject(name string) string { return "/" + name }

// Payload is the tick event payload. Field names and the
// UTC-RFC3339 time format are the wire contract; ScheduledFor is the matched
// minute slot, FiredAt the actual emit time (the two diverge after a restart
// blink — the staleness signal a consumer uses).
type Payload struct {
	Name         string `json:"name"`
	ScheduledFor string `json:"scheduled_for"`
	FiredAt      string `json:"fired_at"`
}

// Build marshals a tick event for one fire: name from the crontab row,
// scheduledFor the matched slot, firedAt the emit time. Both times are rendered
// in the canonical UTC-RFC3339 wire format.
func Build(name string, scheduledFor, firedAt time.Time) (outbox.Event, error) {
	raw, err := json.Marshal(Payload{
		Name:         name,
		ScheduledFor: scheduledFor.UTC().Format(timeFormat),
		FiredAt:      firedAt.UTC().Format(timeFormat),
	})
	if err != nil {
		return outbox.Event{}, fmt.Errorf("marshal tick payload for %s: %w", name, err)
	}
	return outbox.Event{Kind: Kind, Subject: Subject(name), Payload: raw}, nil
}

// sample is the reflection example/schema source for cron's one live family.
var sample = Payload{
	Name:         "nightly",
	ScheduledFor: "2026-06-06T03:00:00Z",
	FiredAt:      "2026-06-06T03:00:00Z",
}

// Publishes returns a LIVE provider for Spec.Publishes: it reads the crontab at
// reflection time and returns one family. Its description enumerates the live
// schedule names while its subject remains open. A read failure or empty
// crontab degrades to an empty registry; the crontab list MCP tool remains the
// authoritative instance view.
func Publishes(store *crontab.Store) func() outbox.Registry {
	return func() outbox.Registry {
		entries, err := store.List(context.Background())
		if err != nil {
			return outbox.Registry{}
		}
		if len(entries) == 0 {
			return outbox.Registry{}
		}
		names := make([]string, 0, len(entries))
		for _, e := range entries {
			names = append(names, e.Name)
		}
		return outbox.Registry{{
			Kind:        Kind,
			Subject:     "/<schedule name>",
			Description: fmt.Sprintf("A schedule fired for a matching minute. Live schedules: %s.", strings.Join(names, ", ")),
			Sample:      sample,
		}}
	}
}
