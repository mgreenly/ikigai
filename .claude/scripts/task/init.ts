#!/usr/bin/env -S deno run --allow-read --allow-write --allow-env

/**
 * Initialize task directory structure
 *
 * Usage: CDD_DIR=/path/to/workspace deno run --allow-read --allow-write --allow-env init.ts
 *
 * Creates the $CDD_DIR/tasks directory structure:
 * - pending/, in_progress/, completed/, failed/ directories
 * - order.json (empty task list)
 * - history.jsonl (empty file)
 *
 * Requires $CDD_DIR environment variable to be set.
 */

import {
  success,
  error,
  output,
  getTasksDir,
  getOrderPath,
  getHistoryPath,
  getStatusDir,
  type TaskStatus,
} from "./mod.ts";

async function main() {
  try {
    const tasksDir = getTasksDir();

    // Create main tasks directory
    await Deno.mkdir(tasksDir, { recursive: true });

    // Create status directories
    const statuses: TaskStatus[] = ["pending", "in_progress", "completed", "failed"];
    for (const status of statuses) {
      await Deno.mkdir(getStatusDir(status), { recursive: true });
    }

    // Create empty order.json
    const orderPath = getOrderPath();
    try {
      await Deno.stat(orderPath);
      output(error("order.json already exists", "ALREADY_EXISTS"));
      return;
    } catch {
      // File doesn't exist, create it
      await Deno.writeTextFile(
        orderPath,
        JSON.stringify({ tasks: [] }, null, 2) + "\n"
      );
    }

    // Create empty history.jsonl
    const historyPath = getHistoryPath();
    try {
      await Deno.stat(historyPath);
    } catch {
      // File doesn't exist, create it
      await Deno.writeTextFile(historyPath, "");
    }

    output(success({
      initialized: true,
      tasks_dir: tasksDir,
      directories_created: statuses.length,
    }));
  } catch (e) {
    output(error(
      `Failed to initialize: ${e instanceof Error ? e.message : String(e)}`,
      "INIT_ERROR"
    ));
  }
}

main();
