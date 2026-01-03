#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * List tasks for the current branch
 *
 * Usage:
 *   deno run ... list.ts              # all tasks
 *   deno run ... list.ts pending      # filter by status
 *   deno run ... list.ts done
 *   deno run ... list.ts in_progress
 *   deno run ... list.ts failed
 *
 * Returns task summaries (not full content).
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, getCurrentBranch, type TaskSummary, type TaskStatus } from "./mod.ts";

const VALID_STATUSES = ["pending", "in_progress", "done", "failed"];

async function main() {
  const statusFilter = Deno.args[0] as TaskStatus | undefined;

  if (statusFilter && !VALID_STATUSES.includes(statusFilter)) {
    output(error(
      `Invalid status '${statusFilter}'. Valid: ${VALID_STATUSES.join(", ")}`,
      "INVALID_STATUS"
    ));
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

    let query = `
      SELECT id, name, task_group, model, thinking, status
      FROM tasks
      WHERE branch = ?
    `;
    const params: (string | undefined)[] = [branch];

    if (statusFilter) {
      query += " AND status = ?";
      params.push(statusFilter);
    }

    query += " ORDER BY id ASC";

    const tasks = db.prepare(query).all<TaskSummary>(...params);

    // Get counts
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

    output(success({
      branch,
      filter: statusFilter || "all",
      tasks,
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
