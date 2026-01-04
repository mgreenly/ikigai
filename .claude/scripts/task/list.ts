#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * List tasks from order.json and filesystem
 *
 * Usage:
 *   deno run --allow-read --allow-write list.ts              # all tasks
 *   deno run --allow-read --allow-write list.ts pending      # filter by status
 *   deno run --allow-read --allow-write list.ts completed
 *   deno run --allow-read --allow-write list.ts in_progress
 *   deno run --allow-read --allow-write list.ts failed
 *
 * Returns task summaries with status from filesystem.
 */

import {
  success,
  error,
  output,
  readOrder,
  findTaskLocation,
  countByStatus,
  type TaskStatus,
} from "./mod.ts";

const VALID_STATUSES = ["pending", "in_progress", "completed", "failed"];

async function main() {
  const statusFilter = Deno.args[0] as TaskStatus | undefined;

  if (statusFilter && !VALID_STATUSES.includes(statusFilter)) {
    output(error(
      `Invalid status '${statusFilter}'. Valid: ${VALID_STATUSES.join(", ")}`,
      "INVALID_STATUS"
    ));
    return;
  }

  try {
    const order = await readOrder();
    const counts = await countByStatus();

    // Build task list with current status from filesystem
    const tasks = [];
    for (const taskEntry of order.tasks) {
      const location = await findTaskLocation(taskEntry.task);

      if (!statusFilter || location === statusFilter) {
        tasks.push({
          name: taskEntry.task,
          group: taskEntry.group,
          model: taskEntry.model,
          thinking: taskEntry.thinking,
          status: location || "missing",
        });
      }
    }

    output(success({
      filter: statusFilter || "all",
      tasks,
      counts,
    }));
  } catch (e) {
    output(error(
      `Failed to list tasks: ${e instanceof Error ? e.message : String(e)}`,
      "LIST_ERROR"
    ));
  }
}

main();
