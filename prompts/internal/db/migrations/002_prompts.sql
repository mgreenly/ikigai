-- 002_prompts.sql — prompts' domain schema: sessions and their runs.
-- A session is a persistent prompt + config + sandbox folder owned by an
-- email; a run is one execution of that session's agent loop.

CREATE TABLE sessions (
    id            TEXT PRIMARY KEY,        -- ULID
    owner_email   TEXT NOT NULL,           -- from X-Owner-Email at create
    name          TEXT,
    prompt        TEXT NOT NULL,
    system_prompt TEXT,
    config_json   TEXT NOT NULL,           -- normalized {provider, model, effort?, max_tokens?, temperature?}
    status        TEXT NOT NULL,           -- 'idle' | 'running'
    created_at    TEXT NOT NULL,
    updated_at    TEXT NOT NULL
);

CREATE TABLE runs (
    id          TEXT PRIMARY KEY,          -- ULID
    session_id  TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    status      TEXT NOT NULL,             -- 'running' | 'succeeded' | 'failed' | 'cancelled'
    started_at  TEXT NOT NULL,
    ended_at    TEXT,
    usage_json  TEXT,                      -- token/usage totals from the engine
    error       TEXT,
    log_path    TEXT NOT NULL              -- data/runs/<session_id>/<run_id>.jsonl
);

CREATE INDEX idx_runs_session ON runs(session_id, started_at);
