#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Delete a task from the queue
 *
 * Usage: deno run ... delete.ts <name>
 *
 * Removes task and all associated escalation/session records.
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, getCurrentBranch } from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  if (!name) {
    output(error("Usage: delete.ts <name>", "INVALID_ARGS"));
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

    const existing = db.prepare(`
      SELECT id FROM tasks WHERE branch = ? AND name = ?
    `).get<{ id: number }>(branch, name);

    if (!existing) {
      closeDb();
      output(error(`Task '${name}' not found on branch '${branch}'`, "NOT_FOUND"));
      return;
    }

    // CASCADE will handle escalations and sessions
    db.prepare("DELETE FROM tasks WHERE id = ?").run(existing.id);
    closeDb();

    output(success({ deleted: name, branch }));
  } catch (e) {
    closeDb();
    output(error(
      `Database error: ${e instanceof Error ? e.message : String(e)}`,
      "DB_ERROR"
    ));
  }
}

main();
