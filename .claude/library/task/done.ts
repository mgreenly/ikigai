#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Mark a task as done and log session completion
 *
 * Usage: deno run ... done.ts <name>
 *
 * Sets status to 'done', records completed_at, logs session event,
 * and returns elapsed time for this task.
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, iso, getCurrentBranch, formatElapsed } from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  if (!name) {
    output(error("Usage: done.ts <name>", "INVALID_ARGS"));
    return;
  }

  let branch: string;
  try {
    branch = await getCurrentBranch();
  } catch (e) {
    output(error(
      `Failed to get git branch: ${e instanceof Error ? e.message : String(e)}`,
      "GIT_ERROR"
    ));
    return;
  }

  try {
    initSchema();
    const db = getDb();
    const now = iso();

    const task = db.prepare(`
      SELECT id, status, started_at FROM tasks WHERE branch = ? AND name = ?
    `).get<{ id: number; status: string; started_at: string | null }>(branch, name);

    if (!task) {
      closeDb();
      output(error(`Task '${name}' not found on branch '${branch}'`, "NOT_FOUND"));
      return;
    }

    if (task.status !== "in_progress") {
      closeDb();
      output(error(
        `Task '${name}' is '${task.status}', expected 'in_progress'`,
        "INVALID_STATUS"
      ));
      return;
    }

    // Update task status
    db.prepare(`
      UPDATE tasks SET status = 'done', completed_at = ?, updated_at = ?
      WHERE id = ?
    `).run(now, now, task.id);

    // Log session event
    db.prepare(`
      INSERT INTO sessions (task_id, event, timestamp) VALUES (?, 'done', ?)
    `).run(task.id, now);

    // Calculate elapsed time for this task
    let elapsedSeconds = 0;
    if (task.started_at) {
      elapsedSeconds = Math.floor(
        (new Date(now).getTime() - new Date(task.started_at).getTime()) / 1000
      );
    }

    // Get remaining count
    const remaining = db.prepare(`
      SELECT COUNT(*) as count FROM tasks WHERE branch = ? AND status = 'pending'
    `).get<{ count: number }>(branch);

    closeDb();

    output(success({
      name,
      status: "done",
      completed_at: now,
      elapsed_seconds: elapsedSeconds,
      elapsed_human: formatElapsed(elapsedSeconds),
      remaining: remaining?.count || 0,
    }));
  } catch (e) {
    closeDb();
    output(error(
      `Database error: ${e instanceof Error ? e.message : String(e)}`,
      "DB_ERROR"
    ));
  }
}

main();
