#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Add a new task to the queue
 *
 * Usage:
 *   deno run ... add.ts <name> --file=<path> [--cleanup] [--group=<group>] [--model=<model>] [--thinking=<thinking>]
 *   deno run ... add.ts <name> < content.md
 *
 * Options:
 *   --file=<path>    Read content from file
 *   --cleanup        Delete the file after reading
 *   --group=<group>  Task group label
 *   --model=<model>  Initial model (default: sonnet)
 *   --thinking=<t>   Initial thinking level (default: thinking)
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
    string: ["file", "group", "model", "thinking"],
    boolean: ["cleanup"],
    default: {
      model: "sonnet",
      thinking: "thinking",
    },
  });

  const name = args._[0];
  if (!name || typeof name !== "string") {
    output(error("Usage: add.ts <name> --file=<path> [options]", "INVALID_ARGS"));
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

  // Get current branch
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
    const now = iso();

    const stmt = db.prepare(`
      INSERT INTO tasks (branch, name, content, task_group, model, thinking, status, created_at, updated_at)
      VALUES (?, ?, ?, ?, ?, ?, 'pending', ?, ?)
    `);

    stmt.run(branch, name, content, args.group || null, args.model, args.thinking, now, now);

    const task = db.prepare("SELECT * FROM tasks WHERE id = last_insert_rowid()").get<Task>();
    closeDb();

    output(success({
      id: task!.id,
      name: task!.name,
      branch: task!.branch,
      status: task!.status,
    }));
  } catch (e) {
    closeDb();
    const message = e instanceof Error ? e.message : String(e);
    if (message.includes("UNIQUE constraint")) {
      output(error(`Task '${name}' already exists on branch '${branch}'`, "DUPLICATE"));
    } else {
      output(error(`Database error: ${message}`, "DB_ERROR"));
    }
  }
}

main();
