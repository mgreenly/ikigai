package db

import (
	"context"
	"database/sql"
	"fmt"
)

const readPoolSize = 8

// OpenRead opens a pooled, query-only handle to the same SQLite file as the
// single writer.
func OpenRead(dbPath string) (*sql.DB, error) {
	dsn := fmt.Sprintf(
		"file:%s?_pragma=journal_mode(WAL)&_pragma=foreign_keys(ON)&_pragma=busy_timeout(5000)&_pragma=query_only(ON)",
		dbPath,
	)
	conn, err := sql.Open("sqlite", dsn)
	if err != nil {
		return nil, fmt.Errorf("open read sqlite %s: %w", dbPath, err)
	}
	conn.SetMaxOpenConns(readPoolSize)
	conn.SetMaxIdleConns(readPoolSize)
	if err := conn.Ping(); err != nil {
		conn.Close()
		return nil, fmt.Errorf("ping read sqlite %s: %w", dbPath, err)
	}
	return conn, nil
}

// Path reports the main database path behind an open SQLite handle.
func Path(ctx context.Context, conn *sql.DB) (string, error) {
	if conn == nil {
		return "", fmt.Errorf("sqlite path: nil DB")
	}
	rows, err := conn.QueryContext(ctx, `PRAGMA database_list`)
	if err != nil {
		return "", fmt.Errorf("sqlite path: database_list: %w", err)
	}
	defer rows.Close()
	for rows.Next() {
		var seq int
		var name, file string
		if err := rows.Scan(&seq, &name, &file); err != nil {
			return "", fmt.Errorf("sqlite path: scan database_list: %w", err)
		}
		if name == "main" && file != "" {
			return file, nil
		}
	}
	if err := rows.Err(); err != nil {
		return "", fmt.Errorf("sqlite path: database_list rows: %w", err)
	}
	return "", fmt.Errorf("sqlite path: main database file not found")
}
