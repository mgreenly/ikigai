-- 003_prompt_triggers.sql — prompts' event-trigger linkage (multi-source).
-- A trigger is ONE (prompt, source, event_filter) binding: when an event of a
-- given type arrives from a given upstream producer, prompts starts a run for
-- the bound prompt. A prompt may hold MANY such bindings — one per upstream
-- event it reacts to — across N sources (cron, crm, ledger, dropbox, scripts,
-- and, fast-follow, prompts itself). This is the mirror of scripts'
-- script_triggers: the two services are symmetric event-plane peers.
--
-- The linkage lives HERE, not in the producer's payload — producers are
-- subscriber-blind. The composite PRIMARY KEY (prompt_id, source, event_filter)
-- gives N triggers per prompt across N sources. There is NO cascade: a tombstone
-- delete removes a prompt's bindings EXPLICITLY (the service calls
-- DeleteTriggers). The old cron-only knobs (max_staleness_secs, max_attempts,
-- updated_at) are GONE — fire-and-forget, symmetric with scripts.
--
-- event_filter is the event type/glob the prompt listens for, e.g.
-- "cron.nightly", "file.created", "scripts.succeeded". The (source, event_filter)
-- index serves the event→prompts fan-out the consumer runs on every arrival.

CREATE TABLE prompt_triggers (
    prompt_id    TEXT NOT NULL,            -- NO cascade: tombstone delete removes a prompt's bindings explicitly
    source       TEXT NOT NULL,            -- producer source id: cron|crm|ledger|dropbox|scripts|prompts
    event_filter TEXT NOT NULL,            -- the event type/glob this prompt listens for, e.g. "cron.nightly"
    created_at   TEXT NOT NULL,
    PRIMARY KEY (prompt_id, source, event_filter)   -- N triggers per prompt, across N sources
);

CREATE INDEX idx_prompt_triggers_lookup ON prompt_triggers(source, event_filter);
