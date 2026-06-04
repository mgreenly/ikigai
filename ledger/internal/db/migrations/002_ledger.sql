-- Double-entry bookkeeping domain (PLAN.md §5). The journal is immutable and
-- append-only: corrections are posted as linked reversals, never edits or
-- deletes. The only mutation of an existing row is a posting's reconciliation
-- status (ledger_reconcile). Accounts are emergent — there is no accounts table;
-- an account exists exactly when a posting references its colon-path, and its
-- type is inferred from the root segment at query time.

CREATE TABLE transactions (
    id              TEXT PRIMARY KEY,          -- ULID
    date            TEXT NOT NULL,             -- bare YYYY-MM-DD calendar day (lexically sortable)
    description     TEXT NOT NULL,             -- payee / memo
    created_at      TEXT NOT NULL,             -- RFC3339Nano insertion instant
    reverses_id     TEXT NULL REFERENCES transactions(id),  -- set on a reversal mirror -> the original
    reversed_by_id  TEXT NULL REFERENCES transactions(id)   -- set on an original -> its mirror (blocks double-reversal)
);
CREATE INDEX idx_transactions_date ON transactions(date);

CREATE TABLE postings (
    id            TEXT PRIMARY KEY,            -- ULID
    txn_id        TEXT NOT NULL REFERENCES transactions(id),
    account       TEXT NOT NULL,               -- canonical colon-path; root is one of the five known types
    amount_cents  INTEGER NOT NULL,            -- signed minor units (debit +, credit -); stored raw, no normalization
    status        TEXT NOT NULL DEFAULT 'pending',  -- pending | cleared | reconciled
    ord           INTEGER NOT NULL             -- 0,1,2… in the order the postings were supplied
);
CREATE INDEX idx_postings_account ON postings(account);
CREATE INDEX idx_postings_txn ON postings(txn_id);
CREATE INDEX idx_postings_status ON postings(status);
