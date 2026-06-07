-- sites domain schema (sites-plan P1). Greenfield; once this migration has
-- shipped it is FROZEN — every later schema change is a new, higher-numbered,
-- additive migration (see the mono-repo CLAUDE.md migration-immutability note).

-- sites: one row per hosted static website. name is the slug primary key,
-- app-validated against ^[a-z0-9][a-z0-9-]{0,62}$. tier is one of '' / 'public'
-- / 'private'. published toggles whether the site is live; published_at records
-- the last publish time and is null until first published. STRICT pins column
-- affinity so the app's type contract is enforced by SQLite itself.
CREATE TABLE sites (
    name         TEXT    PRIMARY KEY,
    tier         TEXT    NOT NULL DEFAULT '',
    published    INTEGER NOT NULL DEFAULT 0,
    published_at TEXT,
    created_at   TEXT    NOT NULL,
    updated_at   TEXT    NOT NULL
) STRICT;
