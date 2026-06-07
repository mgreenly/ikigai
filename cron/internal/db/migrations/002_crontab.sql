-- cron domain schema (event-triggering decisions §2 "cron implementation
-- detail"). One table: a programmable crontab of named 5-field schedules. Each
-- row's `name` IS the identity and the suffix of the emitted `cron.<name>` event
-- type, so it is the PRIMARY KEY and is constrained by a DB CHECK to the
-- event-type-safe charset — the CHECK is the validation boundary for the event
-- type (the tick worker emits "cron."+name with no per-append registry guard, so
-- correctness of the type rides on this constraint, decisions §2).
--
-- NOTE: the plan's literal `name GLOB '[a-z0-9-]*'` only constrains the FIRST
-- character (GLOB `*` then matches any trailing chars, including spaces/dots), so
-- it does not actually enforce the charset. We use `name NOT GLOB '*[^a-z0-9-]*'`
-- instead — true to the plan's intent (event-type-safe charset) and correct: it
-- rejects any string containing a char outside [a-z0-9-].
--
-- `expr` is the raw 5-field cron string (minute hour day-of-month month
-- day-of-week); the MCP layer (P5) parses + validates it on create/update and
-- stores the raw text here. `last_slot` (minute-truncated RFC3339, UTC) is the
-- per-schedule double-emit guard the tick worker (P5) maintains — it is NOT
-- scheduling state and is NOT cleared on an `expr` update.
CREATE TABLE crontab (
    name       TEXT PRIMARY KEY
        CHECK (name <> '' AND name NOT GLOB '*[^a-z0-9-]*'),
    expr       TEXT NOT NULL
        CHECK (expr <> ''),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    last_slot  TEXT NULL
);
