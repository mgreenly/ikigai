#!/usr/bin/env -S deno run --allow-read --allow-write --allow-env

/**
 * Mark a task as in_progress
 *
 * Usage: deno run --allow-read --allow-write start.ts <name>
 *
 * Moves task from pending/ to in_progress/ and logs to history.
 */

import {
  success,
  error,
  output,
  iso,
  findTaskLocation,
  moveTask,
  appendHistory,
} from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  if (!name) {
    output(error("Usage: start.ts <name>", "INVALID_ARGS"));
    return;
  }

  try {
    const location = await findTaskLocation(name);

    if (!location) {
      output(error(`Task '${name}' not found`, "NOT_FOUND"));
      return;
    }

    if (location !== "pending") {
      output(error(
        `Task '${name}' is '${location}', expected 'pending'`,
        "INVALID_STATUS"
      ));
      return;
    }

    const now = iso();

    // Move task file
    await moveTask(name, "pending", "in_progress");

    // Log to history
    await appendHistory({
      timestamp: now,
      action: "start",
      task: name,
      from: "pending",
      to: "in_progress",
    });

    output(success({
      name,
      status: "in_progress",
      started_at: now,
    }));
  } catch (e) {
    output(error(
      `Failed to start task: ${e instanceof Error ? e.message : String(e)}`,
      "START_ERROR"
    ));
  }
}

main();
