#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Delete tasks from the database
 *
 * Usage:
 *   delete.ts <name> [name2] [name3] ...   - Delete specific tasks by name
 *   delete.ts --from-dir <path>            - Delete all tasks matching files in directory
 *
 * Deletes tasks and their associated escalations/sessions (via CASCADE).
 * Only deletes tasks on the current branch.
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, getCurrentBranch } from "./mod.ts";

interface DeleteResult {
  deleted: string[];
  not_found: string[];
  total_deleted: number;
}

async function getTaskNamesFromDir(dirPath: string): Promise<string[]> {
  const names: string[] = [];
  try {
    for await (const entry of Deno.readDir(dirPath)) {
      if (entry.isFile && entry.name.endsWith(".md")) {
        names.push(entry.name);
      }
    }
  } catch (e) {
    throw new Error(`Failed to read directory '${dirPath}': ${e instanceof Error ? e.message : String(e)}`);
  }
  return names;
}

async function main() {
  const args = Deno.args;

  if (args.length === 0) {
    output(error("Usage: delete.ts <name> [...] or delete.ts --from-dir <path>", "INVALID_ARGS"));
    return;
  }

  let taskNames: string[];

  // Check for --from-dir flag
  if (args[0] === "--from-dir") {
    if (args.length < 2) {
      output(error("Usage: delete.ts --from-dir <path>", "INVALID_ARGS"));
      return;
    }
    try {
      taskNames = await getTaskNamesFromDir(args[1]);
    } catch (e) {
      output(error(e instanceof Error ? e.message : String(e), "DIR_ERROR"));
      return;
    }
    if (taskNames.length === 0) {
      output(success<DeleteResult>({
        deleted: [],
        not_found: [],
        total_deleted: 0,
      }));
      return;
    }
  } else {
    taskNames = args;
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

    const deleted: string[] = [];
    const notFound: string[] = [];

    for (const name of taskNames) {
      const task = db.prepare(`
        SELECT id FROM tasks WHERE branch = ? AND name = ?
      `).get<{ id: number }>(branch, name);

      if (!task) {
        notFound.push(name);
        continue;
      }

      // Delete task (escalations and sessions cascade automatically)
      db.prepare(`DELETE FROM tasks WHERE id = ?`).run(task.id);
      deleted.push(name);
    }

    closeDb();

    output(success<DeleteResult>({
      deleted,
      not_found: notFound,
      total_deleted: deleted.length,
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
