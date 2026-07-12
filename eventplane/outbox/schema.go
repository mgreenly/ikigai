package outbox

// SchemaSQL is the canonical outbox-table DDL (event-protocol.md §4.5). The
// library OWNS this DDL so every producer's outbox is byte-identical; a consumer
// applies it through its own migration runner (single migration authority per DB
// file) and is expected to assert its migration matches this constant.
//
// `seq INTEGER PRIMARY KEY AUTOINCREMENT` is load-bearing (§4.5): AUTOINCREMENT
// makes seq a persistent, monotonically climbing high-water mark, so retention
// emptying the table can never let SQLite restart rowids at 1 and silently
// strand a cursored consumer. It is not decoration.
//
// The idx_outbox_created_at index supports time-horizon retention trims
// (§11.3) without scanning the table on the background timer.
const SchemaSQL = `CREATE TABLE outbox (
  seq        INTEGER PRIMARY KEY AUTOINCREMENT,
  event_id   TEXT    NOT NULL,
  kind       TEXT    NOT NULL,
  subject    TEXT    NOT NULL DEFAULT '',
  payload    TEXT    NOT NULL,
  created_at TEXT    NOT NULL
);
CREATE INDEX idx_outbox_created_at ON outbox(created_at);
`
