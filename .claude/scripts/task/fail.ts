#!/usr/bin/env -S deno run --allow-read --allow-write --allow-env

/**
 * Mark a task as failed (max escalation reached)
 *
 * Usage: deno run --allow-read --allow-write fail.ts <name> [reason]
 *
 * Moves task from in_progress/ to failed/ and logs to history.
 */

import {
  success,
  error,
  output,
  iso,
  findTaskLocation,
  moveTask,
  appendHistory,
  getHistoryPath,
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
  const reason = Deno.args.slice(1).join(" ") || "Max escalation reached";

  if (!name) {
    output(error("Usage: fail.ts <name> [reason]", "INVALID_ARGS"));
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
    await moveTask(name, "in_progress", "failed");

    // Log to history
    await appendHistory({
      timestamp: now,
      action: "fail",
      task: name,
      from: "in_progress",
      to: "failed",
      reason,
      elapsed_seconds: elapsedSeconds,
    });

    output(success({
      name,
      status: "failed",
      reason,
      completed_at: now,
      elapsed_seconds: elapsedSeconds,
    }));
  } catch (e) {
    output(error(
      `Failed to mark task as failed: ${e instanceof Error ? e.message : String(e)}`,
      "FAIL_ERROR"
    ));
  }
}

main();
