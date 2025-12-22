#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Get the next pending task for the current branch
 *
 * Usage: deno run ... next.ts
 *
 * Returns the first pending task metadata (without content), or null if none remain.
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, getCurrentBranch, type Task } from "./mod.ts";

async function main() {
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

    const task = db.prepare(`
      SELECT * FROM tasks
      WHERE branch = ? AND status = 'pending'
      ORDER BY id ASC
      LIMIT 1
    `).get<Task>(branch);

    // Also get counts for context
    const counts = db.prepare(`
      SELECT
        SUM(CASE WHEN status = 'pending' THEN 1 ELSE 0 END) as pending,
        SUM(CASE WHEN status = 'in_progress' THEN 1 ELSE 0 END) as in_progress,
        SUM(CASE WHEN status = 'done' THEN 1 ELSE 0 END) as done,
        SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed,
        COUNT(*) as total
      FROM tasks WHERE branch = ?
    `).get<{
      pending: number;
      in_progress: number;
      done: number;
      failed: number;
      total: number;
    }>(branch);

    closeDb();

    if (!task) {
      output(success({
        task: null,
        counts: counts || { pending: 0, in_progress: 0, done: 0, failed: 0, total: 0 },
      }));
      return;
    }

    output(success({
      task: {
        id: task.id,
        name: task.name,
        task_group: task.task_group,
        model: task.model,
        thinking: task.thinking,
      },
      counts: counts || { pending: 0, in_progress: 0, done: 0, failed: 0, total: 0 },
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
