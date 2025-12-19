#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Mark a task as in_progress and log session start
 *
 * Usage: deno run ... start.ts <name>
 *
 * Sets status to 'in_progress', records started_at, and logs session event.
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, iso, getCurrentBranch } from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  if (!name) {
    output(error("Usage: start.ts <name>", "INVALID_ARGS"));
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
      SELECT id, status FROM tasks WHERE branch = ? AND name = ?
    `).get<{ id: number; status: string }>(branch, name);

    if (!task) {
      closeDb();
      output(error(`Task '${name}' not found on branch '${branch}'`, "NOT_FOUND"));
      return;
    }

    if (task.status !== "pending") {
      closeDb();
      output(error(
        `Task '${name}' is '${task.status}', expected 'pending'`,
        "INVALID_STATUS"
      ));
      return;
    }

    // Update task status
    db.prepare(`
      UPDATE tasks SET status = 'in_progress', started_at = ?, updated_at = ?
      WHERE id = ?
    `).run(now, now, task.id);

    // Log session event
    db.prepare(`
      INSERT INTO sessions (task_id, event, timestamp) VALUES (?, 'start', ?)
    `).run(task.id, now);

    closeDb();

    output(success({
      name,
      status: "in_progress",
      started_at: now,
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
