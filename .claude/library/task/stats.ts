#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Generate metrics report from history.jsonl
 *
 * Usage: deno run --allow-read --allow-write stats.ts
 *
 * Returns:
 *   - Task counts by status
 *   - Total runtime (sum of all task durations)
 *   - Average time per task
 *   - Escalation summary
 *   - Slowest tasks
 */

import {
  success,
  error,
  output,
  formatElapsed,
  countByStatus,
  getHistoryPath,
  type HistoryEvent,
  type MoveEvent,
  type EscalateEvent,
} from "./mod.ts";

interface TaskTiming {
  name: string;
  elapsed_seconds: number;
  escalation_count: number;
}

interface EscalationCount {
  from_level: string;
  to_level: string;
  count: number;
}

async function main() {
  try {
    const counts = await countByStatus();

    // Read history
    const historyContent = await Deno.readTextFile(getHistoryPath());
    const lines = historyContent.split("\n").filter(l => l.trim());
    const events: HistoryEvent[] = lines.map(l => JSON.parse(l));

    // Track task timings
    const taskStarts: Map<string, Date> = new Map();
    const taskTimings: TaskTiming[] = [];
    const taskEscalations: Map<string, number> = new Map();

    // Track escalations
    const escalationCounts: Map<string, number> = new Map();

    for (const event of events) {
      if (event.action === "start") {
        taskStarts.set(event.task, new Date(event.timestamp));
      } else if (event.action === "done" || event.action === "fail") {
        const moveEvent = event as MoveEvent;
        const startTime = taskStarts.get(event.task);
        if (startTime && moveEvent.elapsed_seconds !== undefined) {
          taskTimings.push({
            name: event.task,
            elapsed_seconds: moveEvent.elapsed_seconds,
            escalation_count: taskEscalations.get(event.task) || 0,
          });
        }
      } else if (event.action === "escalate") {
        const escEvent = event as EscalateEvent;
        taskEscalations.set(event.task, (taskEscalations.get(event.task) || 0) + 1);

        const key = `${escEvent.from_model}/${escEvent.from_thinking} → ${escEvent.to_model}/${escEvent.to_thinking}`;
        escalationCounts.set(key, (escalationCounts.get(key) || 0) + 1);
      }
    }

    // Calculate totals
    const totalSeconds = taskTimings.reduce((sum, t) => sum + t.elapsed_seconds, 0);
    const completedCount = counts.completed + counts.failed;
    const avgSeconds = completedCount > 0 ? Math.floor(totalSeconds / completedCount) : 0;

    // Sort by elapsed time descending for slowest tasks
    taskTimings.sort((a, b) => b.elapsed_seconds - a.elapsed_seconds);
    const slowestTasks = taskTimings.slice(0, 5).map((t) => ({
      name: t.name,
      elapsed: formatElapsed(t.elapsed_seconds),
      elapsed_seconds: t.elapsed_seconds,
      escalations: t.escalation_count,
    }));

    // Build escalation breakdown
    const escalationBreakdown: EscalationCount[] = [];
    for (const [key, count] of escalationCounts.entries()) {
      const parts = key.split(" → ");
      escalationBreakdown.push({
        from_level: parts[0],
        to_level: parts[1],
        count,
      });
    }
    escalationBreakdown.sort((a, b) => b.count - a.count);

    const totalEscalations = escalationBreakdown.reduce((sum, e) => sum + e.count, 0);

    output(success({
      counts,
      runtime: {
        total_seconds: totalSeconds,
        total_human: formatElapsed(totalSeconds),
        average_seconds: avgSeconds,
        average_human: formatElapsed(avgSeconds),
        completed_tasks: completedCount,
      },
      escalations: {
        total: totalEscalations,
        breakdown: escalationBreakdown,
      },
      slowest_tasks: slowestTasks,
    }));
  } catch (e) {
    output(error(
      `Failed to generate stats: ${e instanceof Error ? e.message : String(e)}`,
      "STATS_ERROR"
    ));
  }
}

main();
