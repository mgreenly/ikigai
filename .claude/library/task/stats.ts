#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Generate metrics report for the current branch
 *
 * Usage: deno run ... stats.ts
 *
 * Returns:
 *   - Task counts by status
 *   - Total runtime (sum of all task durations)
 *   - Average time per task
 *   - Escalation summary
 *   - Slowest tasks
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, getCurrentBranch, formatElapsed } from "./mod.ts";

interface TaskTiming {
  name: string;
  elapsed_seconds: number;
  escalation_count: number;
}

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

    // Task counts
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

    // Calculate total runtime from completed tasks
    const completedTasks = db.prepare(`
      SELECT
        t.name,
        t.started_at,
        t.completed_at,
        (SELECT COUNT(*) FROM escalations e WHERE e.task_id = t.id) as escalation_count
      FROM tasks t
      WHERE t.branch = ? AND t.status IN ('done', 'failed') AND t.started_at IS NOT NULL AND t.completed_at IS NOT NULL
      ORDER BY t.completed_at DESC
    `).all<{
      name: string;
      started_at: string;
      completed_at: string;
      escalation_count: number;
    }>(branch);

    let totalSeconds = 0;
    const taskTimings: TaskTiming[] = [];

    for (const task of completedTasks) {
      const elapsed = Math.floor(
        (new Date(task.completed_at).getTime() - new Date(task.started_at).getTime()) / 1000
      );
      totalSeconds += elapsed;
      taskTimings.push({
        name: task.name,
        elapsed_seconds: elapsed,
        escalation_count: task.escalation_count,
      });
    }

    // Sort by elapsed time descending for slowest tasks
    taskTimings.sort((a, b) => b.elapsed_seconds - a.elapsed_seconds);
    const slowestTasks = taskTimings.slice(0, 5).map((t) => ({
      name: t.name,
      elapsed: formatElapsed(t.elapsed_seconds),
      elapsed_seconds: t.elapsed_seconds,
      escalations: t.escalation_count,
    }));

    // Escalation summary
    const escalations = db.prepare(`
      SELECT
        e.from_model || '/' || e.from_thinking as from_level,
        e.to_model || '/' || e.to_thinking as to_level,
        COUNT(*) as count
      FROM escalations e
      JOIN tasks t ON e.task_id = t.id
      WHERE t.branch = ?
      GROUP BY from_level, to_level
      ORDER BY count DESC
    `).all<{ from_level: string; to_level: string; count: number }>(branch);

    const totalEscalations = escalations.reduce((sum, e) => sum + e.count, 0);

    // Average time per completed task
    const completedCount = (counts?.done || 0) + (counts?.failed || 0);
    const avgSeconds = completedCount > 0 ? Math.floor(totalSeconds / completedCount) : 0;

    closeDb();

    output(success({
      branch,
      counts: counts || { pending: 0, in_progress: 0, done: 0, failed: 0, total: 0 },
      runtime: {
        total_seconds: totalSeconds,
        total_human: formatElapsed(totalSeconds),
        average_seconds: avgSeconds,
        average_human: formatElapsed(avgSeconds),
        completed_tasks: completedCount,
      },
      escalations: {
        total: totalEscalations,
        breakdown: escalations,
      },
      slowest_tasks: slowestTasks,
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
