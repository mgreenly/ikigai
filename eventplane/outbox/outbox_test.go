package outbox

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"

	_ "modernc.org/sqlite"
)

// newMemOutbox returns an Outbox backed by a fresh single-connection in-memory
// SQLite DB with the outbox schema applied. SetMaxOpenConns(1) is the
// single-writer discipline the ordering invariant (§5) depends on.
func newMemOutbox(t *testing.T, mut ...func(*Options)) (*Outbox, *sql.DB) {
	t.Helper()
	db, err := sql.Open("sqlite", "file::memory:?_pragma=busy_timeout(5000)")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	db.SetMaxOpenConns(1)
	t.Cleanup(func() { db.Close() })
	if _, err := db.Exec(SchemaSQL); err != nil {
		t.Fatalf("apply schema: %v", err)
	}
	opts := Options{Source: "crm"}
	for _, m := range mut {
		m(&opts)
	}
	o, err := New(db, opts)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	return o, db
}

// appendOne commits a single event of the given type with a trivial payload.
func appendOne(t *testing.T, o *Outbox, db *sql.DB, typ string) {
	t.Helper()
	tx, err := db.BeginTx(context.Background(), nil)
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	if err := o.Append(tx, Event{Type: typ, Payload: json.RawMessage(`{"k":"v"}`)}); err != nil {
		t.Fatalf("append: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit: %v", err)
	}
	o.Ring()
}

func TestAppendAssignsMonotonicSeqAndStableIdentity(t *testing.T) {
	o, db := newMemOutbox(t)
	for i := 0; i < 3; i++ {
		appendOne(t, o, db, "contact.created")
	}
	rows, last, err := o.fetch(context.Background(), 0, 10)
	if err != nil {
		t.Fatalf("fetch: %v", err)
	}
	if len(rows) != 3 {
		t.Fatalf("rows: got %d want 3", len(rows))
	}
	if rows[0].seq != 1 || rows[1].seq != 2 || rows[2].seq != 3 {
		t.Fatalf("seqs not 1,2,3: %+v", rows)
	}
	if last != 3 {
		t.Fatalf("last: got %d want 3", last)
	}
	// event_id is minted once and stored — a re-fetch is byte-identical (§4.5).
	again, _, _ := o.fetch(context.Background(), 0, 10)
	for i := range rows {
		if rows[i].eventID != again[i].eventID || rows[i].createdAt != again[i].createdAt {
			t.Fatalf("identity not stable across replay at %d", i)
		}
	}
}

// TestConcurrencyStress is the §5.3 executable spec / primary net: N goroutines
// concurrently Append+commit while a reader consumes through the real fetch. The
// reader MUST observe a strictly increasing sequence and MUST never see a row
// appear behind its cursor.
func TestConcurrencyStress(t *testing.T) {
	o, db := newMemOutbox(t)
	const writers, perWriter = 8, 50
	const total = writers * perWriter

	var wg sync.WaitGroup
	stop := make(chan struct{})

	// Reader: continuously advance a cursor, asserting strict monotonicity.
	readerErr := make(chan error, 1)
	go func() {
		var cursor int64
		seen := 0
		for {
			rows, last, err := o.fetch(context.Background(), cursor, 100)
			if err != nil {
				readerErr <- err
				return
			}
			for _, r := range rows {
				if r.seq <= cursor {
					readerErr <- fmt.Errorf("row seq %d not ahead of cursor %d", r.seq, cursor)
					return
				}
				cursor = r.seq
				seen++
			}
			_ = last
			if seen >= total {
				readerErr <- nil
				return
			}
			select {
			case <-stop:
				readerErr <- nil
				return
			default:
			}
		}
	}()

	for w := 0; w < writers; w++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for i := 0; i < perWriter; i++ {
				tx, err := db.BeginTx(context.Background(), nil)
				if err != nil {
					t.Errorf("begin: %v", err)
					return
				}
				if err := o.Append(tx, Event{Type: "contact.created", Payload: json.RawMessage(`{}`)}); err != nil {
					t.Errorf("append: %v", err)
					tx.Rollback()
					return
				}
				if err := tx.Commit(); err != nil {
					t.Errorf("commit: %v", err)
					return
				}
				o.Ring()
			}
		}()
	}
	wg.Wait()
	close(stop)
	if err := <-readerErr; err != nil {
		t.Fatalf("reader observed an ordering violation: %v", err)
	}

	// Final invariant: exactly `total` rows, contiguous and strictly increasing.
	rows, _, err := o.fetch(context.Background(), 0, total+10)
	if err != nil {
		t.Fatalf("final fetch: %v", err)
	}
	if len(rows) != total {
		t.Fatalf("final rows: got %d want %d", len(rows), total)
	}
	for i := 1; i < len(rows); i++ {
		if rows[i].seq <= rows[i-1].seq {
			t.Fatalf("not strictly increasing at %d: %d then %d", i, rows[i-1].seq, rows[i].seq)
		}
	}
}

// TestStartupProbe_RefusesSecondWriter is the §5.3 startup behavioural probe:
// against a real file DB, a second concurrent BEGIN IMMEDIATE must be refused,
// so the probe passes (returns nil). This proves the single-writer guarantee the
// ordering invariant rests on, behaviourally rather than by trusting config.
func TestStartupProbe_RefusesSecondWriter(t *testing.T) {
	dbPath := filepath.Join(t.TempDir(), "probe.db")
	// Materialise the file with the schema so the probe opens a real database.
	db, err := sql.Open("sqlite", "file:"+dbPath+"?_pragma=journal_mode(WAL)")
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	if _, err := db.Exec(SchemaSQL); err != nil {
		t.Fatalf("schema: %v", err)
	}
	db.Close()

	if err := runStartupProbe(dbPath); err != nil {
		t.Fatalf("startup probe should pass on single-writer SQLite, got: %v", err)
	}
}

// TestStartupProbe_MechanismHolds is the executable proof behind the probe:
// while one connection holds a write transaction, a second connection's BEGIN
// IMMEDIATE is rejected (busy_timeout 0). If this ever stopped holding, the
// probe — and the ordering guarantee — would be meaningless.
func TestStartupProbe_MechanismHolds(t *testing.T) {
	dbPath := filepath.Join(t.TempDir(), "mech.db")
	dsn := "file:" + dbPath + "?_pragma=journal_mode(WAL)&_pragma=busy_timeout(0)"
	ctx := context.Background()

	a, err := sql.Open("sqlite", dsn)
	if err != nil {
		t.Fatal(err)
	}
	defer a.Close()
	a.SetMaxOpenConns(1)
	b, err := sql.Open("sqlite", dsn)
	if err != nil {
		t.Fatal(err)
	}
	defer b.Close()
	b.SetMaxOpenConns(1)

	ca, err := a.Conn(ctx)
	if err != nil {
		t.Fatal(err)
	}
	defer ca.Close()
	cb, err := b.Conn(ctx)
	if err != nil {
		t.Fatal(err)
	}
	defer cb.Close()

	if _, err := ca.ExecContext(ctx, "BEGIN IMMEDIATE"); err != nil {
		t.Fatalf("first BEGIN IMMEDIATE failed: %v", err)
	}
	if _, err := cb.ExecContext(ctx, "BEGIN IMMEDIATE"); err == nil {
		cb.ExecContext(ctx, "ROLLBACK")
		t.Fatal("second BEGIN IMMEDIATE was admitted — single-writer ordering not guaranteed")
	}
	ca.ExecContext(ctx, "ROLLBACK")
}

func TestNew_RequiresSourceAndDB(t *testing.T) {
	if _, err := New(nil, Options{Source: "crm"}); err == nil {
		t.Fatal("expected error for nil db")
	}
	db, _ := sql.Open("sqlite", ":memory:")
	defer db.Close()
	if _, err := New(db, Options{}); err == nil {
		t.Fatal("expected error for empty Source")
	}
}

func TestCursorRoundTrip(t *testing.T) {
	c := makeCursor("GEN123", 42)
	g, s, ok := parseCursor(c)
	if !ok || g != "GEN123" || s != 42 {
		t.Fatalf("round trip: %q -> %q,%d,%v", c, g, s, ok)
	}
	for _, bad := range []string{"", "nodot", "GEN.", ".5", "GEN.x", "GEN.-1"} {
		if _, _, ok := parseCursor(bad); ok {
			t.Errorf("parseCursor(%q) should be unintelligible", bad)
		}
	}
}

// R-4BT8-D54J
func TestLoadOrMintGenerationMintsNewEpochWhenSidecarAbsentAfterRestore(t *testing.T) {
	genPath := filepath.Join(t.TempDir(), "outbox.generation")

	before, err := loadOrMintGeneration(genPath)
	if err != nil {
		t.Fatalf("initial loadOrMintGeneration: %v", err)
	}
	if before == "" {
		t.Fatal("initial generation is empty")
	}
	if err := os.Remove(genPath); err != nil {
		t.Fatalf("remove generation sidecar to simulate restore: %v", err)
	}

	after, err := loadOrMintGeneration(genPath)
	if err != nil {
		t.Fatalf("post-restore loadOrMintGeneration: %v", err)
	}
	if after == "" {
		t.Fatal("post-restore generation is empty")
	}
	if after == before {
		t.Fatalf("post-restore generation reused old epoch %q", before)
	}
	b, err := os.ReadFile(genPath)
	if err != nil {
		t.Fatalf("read re-minted generation sidecar: %v", err)
	}
	if got := strings.TrimSpace(string(b)); got != after {
		t.Fatalf("sidecar content = %q, want re-minted generation %q", got, after)
	}
}
