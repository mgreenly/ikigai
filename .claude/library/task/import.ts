#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Import tasks from a source order.json + task files
 *
 * Usage: deno run --allow-read --allow-write import.ts [tasks-directory]
 *
 * Defaults to release/tasks if no directory specified.
 * Reads source order.json, copies it to release/tasks/order.json,
 * and moves all task files to pending/ directory.
 */

import {
  success,
  error,
  output,
  iso,
  appendHistory,
  writeOrder,
  getTaskPath,
  type TaskEntry,
} from "./mod.ts";
import { join } from "jsr:@std/path@1";

interface OrderFile {
  tasks?: TaskEntry[];
  todo?: TaskEntry[];  // Deprecated, use 'tasks'
}

async function main() {
  const tasksDir = Deno.args[0] || "release/tasks";

  // Read order.json
  const orderPath = join(tasksDir, "order.json");
  let order: OrderFile;
  try {
    const content = await Deno.readTextFile(orderPath);
    order = JSON.parse(content);
  } catch (e) {
    output(error(
      `Failed to read ${orderPath}: ${e instanceof Error ? e.message : String(e)}`,
      "READ_ERROR"
    ));
    return;
  }

  // Accept 'tasks' (preferred) or 'todo' (deprecated)
  const entries = order.tasks || order.todo;
  if (!Array.isArray(entries)) {
    output(error("order.json must have a 'tasks' array", "INVALID_FORMAT"));
    return;
  }

  try {
    const imported: { name: string; status: string }[] = [];
    const errors: { name: string; error: string }[] = [];
    const now = iso();

    // Move each task file to pending/ and log to history
    // (Skip stop entries - they have no files)
    for (const entry of entries) {
      // Skip stop entries
      if ('stop' in entry) {
        continue;
      }

      const sourcePath = join(tasksDir, entry.task);
      const destPath = getTaskPath("pending", entry.task);

      try {
        // Check if file exists
        await Deno.stat(sourcePath);

        // Move to pending
        await Deno.rename(sourcePath, destPath);

        // Log to history
        await appendHistory({
          timestamp: now,
          action: "import",
          task: entry.task,
          to: "pending",
        });

        imported.push({ name: entry.task, status: "pending" });
      } catch (e) {
        errors.push({
          name: entry.task,
          error: e instanceof Error ? e.message : String(e),
        });
      }
    }

    // Write order.json with ordered task list
    await writeOrder({ tasks: entries });

    output(success({
      imported_count: imported.length,
      error_count: errors.length,
      imported,
      errors: errors.length > 0 ? errors : undefined,
    }));
  } catch (e) {
    output(error(
      `Import error: ${e instanceof Error ? e.message : String(e)}`,
      "IMPORT_ERROR"
    ));
  }
}

main();
