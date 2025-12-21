#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Mark a task as failed (max escalation reached)
 *
 * Usage: deno run ... fail.ts <name> [reason]
 *
 * Sets status to 'failed' and logs completion.
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, iso, getCurrentBranch, formatElapsed } from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  const reason = Deno.args.slice(1).join(" ") || "Max escalation reached";

  if (!name) {
    output(error("Usage: fail.ts <name> [reason]", "INVALID_ARGS"));
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
      UPDATE tasks SET status = 'failed', completed_at = ?, updated_at = ?
      WHERE id = ?
    `).run(now, now, task.id);

    // Log session event
    db.prepare(`
      INSERT INTO sessions (task_id, event, timestamp) VALUES (?, 'done', ?)
    `).run(task.id, now);

    // Calculate elapsed time
    let elapsedSeconds = 0;
    if (task.started_at) {
      elapsedSeconds = Math.floor(
        (new Date(now).getTime() - new Date(task.started_at).getTime()) / 1000
      );
    }

    closeDb();

    output(success({
      name,
      status: "failed",
      reason,
      completed_at: now,
      elapsed_seconds: elapsedSeconds,
      elapsed_human: formatElapsed(elapsedSeconds),
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
