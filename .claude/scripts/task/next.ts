#!/usr/bin/env -S deno run --allow-read --allow-write --allow-env

/**
 * Get the next pending task
 *
 * Usage: deno run --allow-read --allow-write next.ts
 *
 * Returns the first pending task from order.json, or null if none remain.
 */

import {
  success,
  error,
  output,
  readOrder,
  findTaskLocation,
  countByStatus,
  isTaskEntry,
  isStopEntry,
  isStopPassed,
  appendHistory,
  iso,
} from "./mod.ts";

async function main() {
  try {
    const order = await readOrder();
    const counts = await countByStatus();

    // Find first task or stop in order.json that hasn't been completed/passed
    for (const entry of order.tasks) {
      if (isStopEntry(entry)) {
        // Check if this stop has been passed
        const passed = await isStopPassed(entry.stop);
        if (!passed) {
          // Log that we reached this stop
          await appendHistory({
            timestamp: iso(),
            action: "stop_reached",
            task: entry.stop,
            stop: entry.stop,
            message: entry.message,
          });

          output(success({
            type: "stop",
            stop: entry.stop,
            message: entry.message || "Manual verification checkpoint",
            counts,
          }));
          return;
        }
        // Stop already passed, continue to next entry
      } else if (isTaskEntry(entry)) {
        const location = await findTaskLocation(entry.task);
        if (location === "pending") {
          output(success({
            type: "task",
            task: entry,
            counts,
          }));
          return;
        }
      }
    }

    // No pending tasks or stops found
    output(success({
      type: "task",
      task: null,
      counts,
    }));
  } catch (e) {
    output(error(
      `Failed to get next task: ${e instanceof Error ? e.message : String(e)}`,
      "READ_ERROR"
    ));
  }
}

main();
