# Phase 4 — Consumer surface: routing fields and keyed delivery

*Realizes design Decision 4 (consumer surface). Depends on Phase 2.*

Revise package `consumer` to the new addressing: `Event` gains `Kind` and
`Subject` (from the envelope JSON) and a `Key()` method (via `routing.Key`);
the `Type` field is deleted; `parseEvent` treats an empty decoded `kind` as an
unparseable envelope (engine skip: no handler, cursor advances).
`Subscription` is cut over with it: its `Filter` doc contract becomes a
canonical-key glob in the `routing` dialect, its `Match` method is deleted
(sole suite caller is notify's handler, which moves to
`routing.Match(sub.Filter, ev.Key())` in its own spec), and the package's
`path` import goes with it. Engine delivery semantics — handler-return cursor gate, resync, backoff,
structural-vs-transport, control-frame dispatch — are untouched; the existing
end-to-end consumer tests (real `FeedHandler` + `httptest` + `consumer.Run`)
are updated to the new shapes and must stay green. End state: a consuming
service filters in its handler with `routing.Match(pattern, ev.Key())`, proven
end-to-end.

**Done when:**

- R-3VDM-JK38 — end-to-end delivery populates `Kind`/`Subject`/`ID`/`Source`/
  `Time`/`Payload`; no `Type` field exists on `Event`.
- R-3WLI-XBTX — `Event.Key()` equals the frame's `event:` line for
  subject-ful and subjectless events.
- R-3XTF-B3KM — control frames (`caught-up`, `status`, keepalive, `resync`)
  never reach the handler and never corrupt the cursor under keyed frames.
- R-4098-2N20 — an empty-kind envelope is engine-skipped: handler not
  invoked, cursor advances, next event delivered.
- R-3Z1B-OVBB — a handler filtering with
  `routing.Match("dropbox:create/bills/**/*.pdf", ev.Key())` acts on exactly
  the matching event of three, cursor past all three.
- R-95KP-1QIO — filtering through a declared `Subscription` (handler gating
  on `routing.Match(sub.Filter, ev.Key())`) acts end-to-end on exactly the
  events its `Filter` selects; `Subscription` has no `Match` method.

Each id is covered by a test citing it (end-to-end ids on the real
FeedHandler/httptest/consumer.Run substrate); `go test ./...` and
`go vet ./...` from `eventplane/` exit 0; run from `eventplane/`, each of
`grep -n 'json:"type"' consumer/*.go`,
`grep -n 'func (s Subscription) Match' consumer/subscription.go`, and
`grep -n '"path"' consumer/subscription.go` prints nothing.
