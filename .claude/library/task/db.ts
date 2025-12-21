/**
 * Task DB - Database module
 *
 * Uses @db/sqlite for Deno-native SQLite with FFI
 */

import { Database } from "jsr:@db/sqlite@0.12";
import { dirname, fromFileUrl, join } from "jsr:@std/path@1";

const SCHEMA_VERSION = 1;

// Get database path relative to this module
// Module is at .claude/library/task-db/db.ts
// Database is at .claude/data/tasks.db
function getDbPath(): string {
  const moduleDir = dirname(fromFileUrl(import.meta.url));
  return join(moduleDir, "..", "..", "data", "tasks.db");
}

let _db: Database | null = null;

export function getDb(): Database {
  if (!_db) {
    const dbPath = getDbPath();
    _db = new Database(dbPath);
    _db.exec("PRAGMA foreign_keys = ON");
    _db.exec("PRAGMA journal_mode = WAL");
  }
  return _db;
}

export function closeDb(): void {
  if (_db) {
    _db.close();
    _db = null;
  }
}

export function initSchema(): void {
  const db = getDb();

  db.exec(`
    CREATE TABLE IF NOT EXISTS schema_version (
      version INTEGER PRIMARY KEY
    );

    CREATE TABLE IF NOT EXISTS tasks (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      branch TEXT NOT NULL,
      name TEXT NOT NULL,
      content TEXT NOT NULL,
      task_group TEXT,
      model TEXT NOT NULL DEFAULT 'sonnet',
      thinking TEXT NOT NULL DEFAULT 'thinking',
      status TEXT NOT NULL DEFAULT 'pending',
      created_at TEXT NOT NULL,
      updated_at TEXT NOT NULL,
      started_at TEXT,
      completed_at TEXT,
      UNIQUE(branch, name)
    );

    CREATE TABLE IF NOT EXISTS escalations (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      task_id INTEGER NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
      from_model TEXT NOT NULL,
      from_thinking TEXT NOT NULL,
      to_model TEXT NOT NULL,
      to_thinking TEXT NOT NULL,
      reason TEXT,
      escalated_at TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS sessions (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      task_id INTEGER NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
      event TEXT NOT NULL,
      timestamp TEXT NOT NULL
    );

    CREATE INDEX IF NOT EXISTS idx_tasks_branch_status ON tasks(branch, status);
    CREATE INDEX IF NOT EXISTS idx_sessions_task ON sessions(task_id);
    CREATE INDEX IF NOT EXISTS idx_escalations_task ON escalations(task_id);
  `);

  // Set schema version if not exists
  const version = db.prepare("SELECT version FROM schema_version").value<[number]>();
  if (!version) {
    db.prepare("INSERT INTO schema_version (version) VALUES (?)").run(SCHEMA_VERSION);
  }
}

export { Database };
