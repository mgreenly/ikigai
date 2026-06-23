package wiki_test

import (
	"context"
	"database/sql"
	"testing"

	agentkit "github.com/ikigenba/agentkit"

	"wiki/internal/db"
	wikidomain "wiki/internal/wiki"
)

// migratedWikiDB opens a fresh temp-dir SQLite database and applies all
// migrations, returning the single open connection. Used by integration
// suites that drive the service through a single connection.
func migratedWikiDB(t *testing.T, ctx context.Context) *sql.DB {
	t.Helper()

	conn, err := db.Open(t.TempDir() + "/wiki.db")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if err := db.Migrate(ctx, conn); err != nil {
		conn.Close()
		t.Fatalf("Migrate: %v", err)
	}
	return conn
}

// migratedConns opens a fresh temp-dir SQLite database with separate read and
// write connections, applies all migrations, and returns the Conns pair plus a
// cleanup func. Used by suites that exercise concurrent read/write behavior.
func migratedConns(t *testing.T, ctx context.Context) (wikidomain.Conns, func()) {
	t.Helper()

	path := t.TempDir() + "/wiki.db"
	write, err := db.Open(path)
	if err != nil {
		t.Fatalf("Open writer: %v", err)
	}
	if err := db.Migrate(ctx, write); err != nil {
		write.Close()
		t.Fatalf("Migrate: %v", err)
	}
	read, err := db.OpenRead(path)
	if err != nil {
		write.Close()
		t.Fatalf("OpenRead: %v", err)
	}
	return wikidomain.Conns{Read: read, Write: write}, func() {
		read.Close()
		write.Close()
	}
}

// cloneAgentKitRequest makes a defensive copy of an agentkit request so that
// captured requests are not mutated by later round trips.
func cloneAgentKitRequest(req *agentkit.Request) agentkit.Request {
	if req == nil {
		return agentkit.Request{}
	}
	return agentkit.Request{
		Model:    req.Model,
		System:   req.System,
		Messages: append([]agentkit.Message(nil), req.Messages...),
		Tools:    append([]agentkit.Tool(nil), req.Tools...),
		Gen:      req.Gen,
	}
}
