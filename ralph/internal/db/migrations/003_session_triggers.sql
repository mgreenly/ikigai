-- 003_session_triggers.sql — ralph's event-trigger linkage.
-- A session may declare AT MOST ONE event trigger (1:1 with the session): when
-- the named cron event fires, ralph starts a run for the session. The linkage
-- lives HERE, not in cron's payload — cron is subscriber-blind. session_id is
-- the PRIMARY KEY (one trigger per session for now; the PK relaxes to a
-- composite later if a session ever fans in from multiple events).
--
-- trigger_event is the full event type the session listens for, e.g.
-- "cron.nightly". It is indexed for the event→sessions fan-out the consumer
-- runs on every cron.<name> arrival. max_staleness_secs bounds how old an
-- occurrence (now - scheduled_for) may be before the handler skips it;
-- max_attempts caps the in-memory fixed-delay retry of a failed fire.

CREATE TABLE session_triggers (
    session_id         TEXT PRIMARY KEY REFERENCES sessions(id) ON DELETE CASCADE,
    trigger_event      TEXT NOT NULL,            -- e.g. "cron.nightly"
    max_staleness_secs INTEGER NOT NULL,         -- skip occurrence if now-scheduled_for exceeds this
    max_attempts       INTEGER NOT NULL,         -- in-memory fixed-delay retry cap
    created_at         TEXT NOT NULL,
    updated_at         TEXT NOT NULL
);

CREATE INDEX idx_session_triggers_event ON session_triggers(trigger_event);
