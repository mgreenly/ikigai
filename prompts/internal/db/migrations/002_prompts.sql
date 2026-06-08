-- 002_prompts.sql — prompts' domain schema: prompts and their runs.
-- A prompt is a persistent prompt + config owned by an email; a run is one
-- execution of that prompt's agent loop and is the on-disk unit
-- (data/runs/<run_id>/, holding output.jsonl + a per-run sandbox/).

CREATE TABLE prompts (
    id            TEXT PRIMARY KEY,        -- ULID
    owner_email   TEXT NOT NULL,           -- from X-Owner-Email at create
    name          TEXT,
    user_prompt   TEXT NOT NULL,
    system_prompt TEXT,
    config_json   TEXT NOT NULL,           -- normalized {provider, model, effort?, max_tokens?, temperature?}
    created_at    TEXT NOT NULL,
    updated_at    TEXT NOT NULL
);

CREATE TABLE runs (
    id            TEXT PRIMARY KEY,          -- ULID (first-class & addressable)
    prompt_id     TEXT NOT NULL,             -- NO foreign key / NO cascade: a run may dangle after its prompt is tombstone-deleted
    owner_email   TEXT NOT NULL,             -- denormalized from the prompt: a run stays owner-addressable after its prompt is gone
    prompt_name   TEXT,                      -- captured at run start, for the outcome event
    status        TEXT NOT NULL,             -- 'running' | 'succeeded' | 'failed' | 'cancelled'
    started_at    TEXT NOT NULL,
    ended_at      TEXT,
    usage_json    TEXT,                      -- token/usage totals from the engine
    error         TEXT,
    trigger_source   TEXT,                   -- '' for a manual run; else producer source id (cron|crm|ledger|dropbox|scripts|prompts)
    trigger_type     TEXT,                   -- the fired event type, e.g. "cron.nightly", "file.created", "scripts.succeeded"
    trigger_event_id TEXT,                   -- the upstream event id that fired this run
    log_path      TEXT NOT NULL              -- data/runs/<run_id>/output.jsonl
);

CREATE INDEX idx_runs_prompt ON runs(prompt_id, started_at);
CREATE INDEX idx_runs_status ON runs(status);
