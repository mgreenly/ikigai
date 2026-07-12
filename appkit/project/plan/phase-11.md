# Phase 11 — Event-routing conformance (family registry, kind-keyed reflection)

*Realizes design Decision 11 (routing conformance) and Decision 9 as rewritten
(family-based `reflection`, kind-keyed detail). Depends on Phase 09 (the
standard tools) and Phase 10 (the consumer table whose fixtures convert).*

> ⛔ **External ordering — do not start until the revised eventplane is
> built.** This phase compiles against the revised eventplane API
> (`outbox.Event{Kind, Subject}`, `outbox.Family`/`Registry`,
> `Registry.Detail(kind)`/`UnknownKindError`, `routing.Key`,
> `consumer.Event{Kind, Subject}`), specified in `eventplane/project/design/`
> D1–D4 and built by **eventplane plan phases 01–04**. Until those are ✅ in
> `eventplane/project/plan/STATUS.md`, this phase cannot build — appkit will
> not compile against the old eventplane once started, nor against the new one
> until this phase lands. In suite build order this phase follows eventplane
> phases 01–04 and **precedes every service's own conformance phase** (appkit
> is the hinge: services compile against appkit).

Observable end state:

- appkit builds and its whole suite is green against the revised eventplane:
  every `outbox.Registry` value is family-shaped
  (`Family{Kind, Subject, Description, Sample}`), no code or test references
  `outbox.EventType` or `outbox.UnknownEventTypeError`, and doc comments at
  the pass-through seams (`appkit.go` `Spec.Events`/`Publishes`,
  `feed/feed.go` `Options.Registry`, `server/server.go`) speak kinds/families.
- The chassis `reflection` tool takes an optional `kind` argument (no
  `event_type` anywhere): the no-arg index renders
  `{kind, subject, description}` entries via `Registry.Index()` (with the
  `Publishes` live-provider preference intact) plus the unchanged
  `subscribes` list; `kind` detail renders
  `{kind, subject, description, schema, example}` via `Registry.Detail(kind)`;
  an undeclared kind returns an `isError` naming the declared kinds via
  `errors.As` on `*outbox.UnknownKindError`; the tool descriptor's
  `inputSchema` advertises `kind`.
- Test fixtures conform: `mcp/mcp_test.go` and `server/server_test.go`
  registry literals are families; `consumers_test.go` frames
  `kind`/`subject` envelopes with canonical-key `event:` lines (no `"type"`
  key — under the revised consumer contract a typed envelope is
  engine-skipped and would hang the consumer tests).
- appkit gains no outbox migration and no DDL guard (it is a library with no
  service database; `internal/testmigrations` stays outbox-free) — services
  re-apply the revised `outbox.SchemaSQL` in their own specs.

**Done when:** the suite is green — `cd appkit && go build ./...`,
`go vet ./...`, `gofmt -l .` (no output), and `go test ./...` all succeed with
zero failures — and:

- R-7EK6-8030, R-7FS2-LRTP, R-7GZY-ZJKE, R-7I7V-DBB3 (D9, revised) are covered
  by clearly-named tests through the D8 `ServeHTTP` seam with real
  family-shaped registries;
- R-7JFR-R31S and R-7LVK-IMJ6 (D11) are covered by clearly-named tests driving
  `feed.Start` over a real `t.TempDir()` SQLite database, R-7LVK-IMJ6 through
  the returned `Producer.Handler` over a real `httptest` connection;
- R-ML2U-CBQM's existing `health` test still passes unchanged (the health tool
  is untouched by the cutover);
- `grep -rn "EventType\|UnknownEventTypeError\|event_type" --include="*.go" .`
  run from `appkit/` prints nothing (the `project/` docs are not `.go` files,
  so the check is not self-referential).
