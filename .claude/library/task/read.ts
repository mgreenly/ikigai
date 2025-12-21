#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Read a task's content and metadata
 *
 * Usage: deno run ... read.ts <name>
 *
 * Returns full task including content.
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, getCurrentBranch, type Task } from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  if (!name) {
    output(error("Usage: read.ts <name>", "INVALID_ARGS"));
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

  try {
    initSchema();
    const db = getDb();

    const task = db.prepare(`
      SELECT * FROM tasks WHERE branch = ? AND name = ?
    `).get<Task>(branch, name);

    closeDb();

    if (!task) {
      output(error(`Task '${name}' not found on branch '${branch}'`, "NOT_FOUND"));
      return;
    }

    output(success(task));
  } catch (e) {
    closeDb();
    output(error(
      `Database error: ${e instanceof Error ? e.message : String(e)}`,
      "DB_ERROR"
    ));
  }
}

main();
