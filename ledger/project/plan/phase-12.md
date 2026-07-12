# Phase 12 — `external_ref`: opt-in idempotency on `record`/`reverse`

*Realizes design Decision 14 (`external_ref`). Depends on Phase 09 (the
assembled `appkit/mcp` surface) and Phase 10 (the settled `internal/db`
migration-embed shape). Covers R-FP14-UYWQ, R-FQ91-8QNF, R-FRGX-MIE4,
R-FSOU-0A4T, R-FTWQ-E1VI, R-FV4M-RTM7, R-FWCJ-5LCW. **Read D14 for the
pinned edges: empty-string rejection, register/balance non-exposure, and the
reversal-never-frees-the-ref rule.***

Observable end state:

- One **new timestamped migration**, minted with
  `bin/create-migration ledger external_ref` (never a hand-picked number),
  adds `transactions.external_ref TEXT NULL` plus the partial unique index
  `idx_transactions_external_ref ON transactions(external_ref) WHERE
  external_ref IS NOT NULL`. Migrations `001`–`003` are byte-untouched.
- `internal/ledger`: `Transaction`/`RecordInput` gain `ExternalRef *string`;
  `Store.InsertTransaction` writes the column and the scan helpers read it; a
  new `ErrDuplicateRef` sentinel; `Record` and `Reverse` accept an optional
  ref, look it up inside the same write tx before inserting, and abort with a
  wrapped `ErrDuplicateRef` naming the existing transaction id (a partial-
  index constraint violation maps to the same sentinel). A rejected call
  leaves no transaction row, no postings, no outbox row.
- `internal/mcp`: `record` and `reverse` gain the optional `external_ref`
  string parameter (schema description stating the `<source>:<identifier>`
  convention, e.g. `dropbox:/bills/aws/2026-06.pdf@<content_hash>` — a
  convention, not a validated format); empty string is rejected at the
  boundary with `ledger.ErrValidation`; `translateLedgerError` gains the
  `duplicate_ref` case; `transactionJSON` includes `external_ref` when
  present (omitted when NULL, like `reverses_id`); `register`/`balance`
  output is unchanged.
- `describe` gains a static `external_refs` convention field (domain
  `DescribeReport` + the tool rendering).
- `internal/ledger/events.go`: `transactionRecordedPayload` gains
  `ExternalRef *string` (`json:"external_ref"`, null when absent) and the
  registry Sample fills it in.

**Done when:** the suite is green (design Conventions commands, from
`ledger/`, plus `bin/check-migrations ledger` per the plan done bar) and:

- R-FP14-UYWQ, R-FQ91-8QNF, R-FRGX-MIE4, R-FSOU-0A4T, R-FTWQ-E1VI,
  R-FV4M-RTM7, and R-FWCJ-5LCW are each covered by a clearly-named test
  asserting the behavior its D14 Verification line states (store/round-trip +
  two-NULL-refs-both-insert; duplicate rejection naming the first txn with
  transactions/postings/outbox row counts unchanged; the real-schema
  direct-SQL partial-index proof on a fresh migrated DB; empty-string →
  `validation`, nothing inserted; reverse's own ref + reversal never freeing
  the original's; the event payload's `external_ref` value/null and the
  mirror's own ref; the tool-surface schema/describe/`duplicate_ref` wire
  envelope through the assembled handler);
- `git diff --stat -- ledger/internal/db/migrations/001_schema_migrations.sql
  ledger/internal/db/migrations/002_ledger.sql
  ledger/internal/db/migrations/003_outbox.sql` is empty (frozen migrations
  untouched) and exactly one new `*_external_ref.sql` timestamped migration
  exists under `ledger/internal/db/migrations/`.
