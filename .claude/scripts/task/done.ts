#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Mark a task as done
 *
 * Usage: deno run --allow-read --allow-write done.ts <name>
 *
 * Moves task from in_progress/ to completed/ and logs to history.
 */

import {
  success,
  error,
  output,
  iso,
  findTaskLocation,
  moveTask,
  appendHistory,
  countByStatus,
  getHistoryPath,
  formatElapsed,
  getAverageTaskTime,
  countTasksToNextStop,
} from "./mod.ts";

// Get elapsed time from history
async function getElapsedSeconds(taskName: string, completedAt: Date): Promise<number> {
  try {
    const historyContent = await Deno.readTextFile(getHistoryPath());
    const lines = historyContent.split("\n").filter(l => l.trim());

    // Find the most recent "start" event for this task
    for (let i = lines.length - 1; i >= 0; i--) {
      const event = JSON.parse(lines[i]);
      if (event.task === taskName && event.action === "start") {
        const startedAt = new Date(event.timestamp);
        return Math.floor((completedAt.getTime() - startedAt.getTime()) / 1000);
      }
    }
  } catch {
    // If we can't read history, return 0
  }
  return 0;
}

async function main() {
  const name = Deno.args[0];
  if (!name) {
    output(error("Usage: done.ts <name>", "INVALID_ARGS"));
    return;
  }

  try {
    const location = await findTaskLocation(name);

    if (!location) {
      output(error(`Task '${name}' not found`, "NOT_FOUND"));
      return;
    }

    if (location !== "in_progress") {
      output(error(
        `Task '${name}' is '${location}', expected 'in_progress'`,
        "INVALID_STATUS"
      ));
      return;
    }

    const now = iso();
    const nowDate = new Date(now);
    const elapsedSeconds = await getElapsedSeconds(name, nowDate);

    // Move task file
    await moveTask(name, "in_progress", "completed");

    // Log to history
    await appendHistory({
      timestamp: now,
      action: "done",
      task: name,
      from: "in_progress",
      to: "completed",
      elapsed_seconds: elapsedSeconds,
    });

    // Get remaining count and calculate estimates
    const counts = await countByStatus();
    const avgTaskTime = await getAverageTaskTime();
    const tasksToStop = await countTasksToNextStop(name);
    const estimatedSecondsToStop = tasksToStop * avgTaskTime;
    const estimatedSecondsToCompletion = counts.pending * avgTaskTime;

    output(success({
      name,
      elapsed: formatElapsed(elapsedSeconds),
      remaining_to_stop: tasksToStop,
      eta_to_stop: formatElapsed(estimatedSecondsToStop),
      remaining_total: counts.pending,
      eta_total: formatElapsed(estimatedSecondsToCompletion),
    }));
  } catch (e) {
    output(error(
      `Failed to complete task: ${e instanceof Error ? e.message : String(e)}`,
      "DONE_ERROR"
    ));
  }
}

main();
