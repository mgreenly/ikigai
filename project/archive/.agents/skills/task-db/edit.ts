#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Edit a task's content
 *
 * Usage:
 *   deno run ... edit.ts <name> --file=<path> [--cleanup]
 *   deno run ... edit.ts <name> < content.md
 *
 * Options:
 *   --file=<path>  Read new content from file
 *   --cleanup      Delete the file after reading
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import { success, error, output, iso, getCurrentBranch, type Task } from "./mod.ts";
import { parseArgs } from "jsr:@std/cli@1/parse-args";

async function readStdin(): Promise<string> {
  const decoder = new TextDecoder();
  const chunks: Uint8Array[] = [];
  for await (const chunk of Deno.stdin.readable) {
    chunks.push(chunk);
  }
  const total = chunks.reduce((acc, c) => acc + c.length, 0);
  const result = new Uint8Array(total);
  let offset = 0;
  for (const chunk of chunks) {
    result.set(chunk, offset);
    offset += chunk.length;
  }
  return decoder.decode(result);
}

async function main() {
  const args = parseArgs(Deno.args, {
    string: ["file"],
    boolean: ["cleanup"],
  });

  const name = args._[0];
  if (!name || typeof name !== "string") {
    output(error("Usage: edit.ts <name> --file=<path> [--cleanup]", "INVALID_ARGS"));
    return;
  }

  // Get content from file or stdin
  let content: string;
  try {
    if (args.file) {
      content = await Deno.readTextFile(args.file);
      if (args.cleanup) {
        await Deno.remove(args.file);
      }
    } else {
      content = await readStdin();
    }
  } catch (e) {
    output(error(
      `Failed to read content: ${e instanceof Error ? e.message : String(e)}`,
      "READ_ERROR"
    ));
    return;
  }

  if (!content.trim()) {
    output(error("Content cannot be empty", "EMPTY_CONTENT"));
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

    db.prepare(`
      UPDATE tasks SET content = ?, updated_at = ? WHERE id = ?
    `).run(content, iso(), existing.id);

    const task = db.prepare("SELECT * FROM tasks WHERE id = ?").get<Task>(existing.id);
    closeDb();

    output(success({
      id: task!.id,
      name: task!.name,
      updated_at: task!.updated_at,
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
