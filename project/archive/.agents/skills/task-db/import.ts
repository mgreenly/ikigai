#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Import tasks from existing order.json + task files
 *
 * Usage: deno run ... import.ts <tasks-directory>
 *
 * Reads order.json from the directory and imports all tasks (both todo and done)
 * along with their markdown content from the same directory.
 *
 * Example: deno run ... import.ts docs/rel-07/tasks
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, iso, getCurrentBranch } from "./mod.ts";
import { join } from "jsr:@std/path@1";

interface TaskEntry {
  task: string;
  group: string;
  model: string;
  thinking: string;
}

interface OrderFile {
  todo: TaskEntry[];
  done: TaskEntry[];
}

async function main() {
  const tasksDir = Deno.args[0];
  if (!tasksDir) {
    output(error("Usage: import.ts <tasks-directory>", "INVALID_ARGS"));
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

  if (!Array.isArray(order.todo) || !Array.isArray(order.done)) {
    output(error("order.json must have 'todo' and 'done' arrays", "INVALID_FORMAT"));
    return;
  }

  try {
    initSchema();
    const db = getDb();
    const now = iso();

    const imported: { name: string; status: string }[] = [];
    const errors: { name: string; error: string }[] = [];

    // Import todo tasks
    for (const entry of order.todo) {
      const taskPath = join(tasksDir, entry.task);
      try {
        const content = await Deno.readTextFile(taskPath);
        db.prepare(`
          INSERT INTO tasks (branch, name, content, task_group, model, thinking, status, created_at, updated_at)
          VALUES (?, ?, ?, ?, ?, ?, 'pending', ?, ?)
        `).run(branch, entry.task, content, entry.group || null, entry.model, entry.thinking, now, now);
        imported.push({ name: entry.task, status: "pending" });
      } catch (e) {
        if (e instanceof Error && e.message.includes("UNIQUE constraint")) {
          errors.push({ name: entry.task, error: "Already exists" });
        } else {
          errors.push({
            name: entry.task,
            error: e instanceof Error ? e.message : String(e),
          });
        }
      }
    }

    // Import done tasks
    for (const entry of order.done) {
      const taskPath = join(tasksDir, entry.task);
      try {
        const content = await Deno.readTextFile(taskPath);
        db.prepare(`
          INSERT INTO tasks (branch, name, content, task_group, model, thinking, status, created_at, updated_at, completed_at)
          VALUES (?, ?, ?, ?, ?, ?, 'done', ?, ?, ?)
        `).run(branch, entry.task, content, entry.group || null, entry.model, entry.thinking, now, now, now);
        imported.push({ name: entry.task, status: "done" });
      } catch (e) {
        if (e instanceof Error && e.message.includes("UNIQUE constraint")) {
          errors.push({ name: entry.task, error: "Already exists" });
        } else {
          errors.push({
            name: entry.task,
            error: e instanceof Error ? e.message : String(e),
          });
        }
      }
    }

    closeDb();

    output(success({
      branch,
      imported_count: imported.length,
      error_count: errors.length,
      imported,
      errors: errors.length > 0 ? errors : undefined,
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
