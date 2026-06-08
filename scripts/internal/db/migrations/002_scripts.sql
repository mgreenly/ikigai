-- 002_scripts.sql — scripts domain schema (ARCHITECTURE.md §4 / PLAN.md §A3).
--
-- Tombstone-delete model: runs are append-only history, so runs.script_id has
-- NO ON DELETE CASCADE (the label may dangle after the script row is deleted).
-- Triggers ARE live definition, so script_triggers cascades with the script.
-- There is NO status column on scripts; running_count is derived from runs.

CREATE TABLE scripts (
    id          TEXT PRIMARY KEY,        -- ULID
    owner_email TEXT NOT NULL,           -- from X-Owner-Email at create
    name        TEXT,
    body        TEXT NOT NULL,           -- the Python source text
    config_json TEXT NOT NULL,           -- normalized {interpreter?, timeout_secs?, ...}; minimal day-one
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL
);

CREATE TABLE runs (
    id          TEXT PRIMARY KEY,        -- ULID — the run/instance id
    script_id   TEXT NOT NULL REFERENCES scripts(id),  -- NO cascade: runs are append-only history (tombstone delete); script_id may dangle after the script is deleted
    status      TEXT NOT NULL,           -- 'running' | 'succeeded' | 'failed' | 'cancelled'
    exit_code   INTEGER,                 -- null while running / never-started
    started_at  TEXT NOT NULL,
    ended_at    TEXT,                    -- null while running
    error       TEXT,                    -- failure / TTL / spawn reason
    trigger_source   TEXT,               -- '' for a manual run
    trigger_type     TEXT,
    trigger_event_id TEXT,
    stdout_path TEXT NOT NULL,           -- data/runs/<run_id>/stdout.log
    stderr_path TEXT NOT NULL            -- data/runs/<run_id>/stderr.log; the run dir also persists main.py + config.json + any produced files
);
CREATE INDEX idx_runs_script ON runs(script_id, started_at);
CREATE INDEX idx_runs_status ON runs(status);

CREATE TABLE script_triggers (
    script_id    TEXT NOT NULL REFERENCES scripts(id) ON DELETE CASCADE,
    source       TEXT NOT NULL,          -- upstream producer, e.g. "crm"
    event_filter TEXT NOT NULL,          -- glob: "contact.created", "contact.*", "cron.nightly"
    created_at   TEXT NOT NULL,
    PRIMARY KEY (script_id, source, event_filter)
);
CREATE INDEX idx_script_triggers_source ON script_triggers(source);
