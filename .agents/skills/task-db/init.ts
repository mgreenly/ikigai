#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net

/**
 * Initialize the task database
 *
 * Usage: deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net init.ts
 *
 * Creates the database and schema if they don't exist.
 */

import { initSchema, closeDb } from "./db.ts";
import { success, error, output } from "./mod.ts";

function main() {
  try {
    initSchema();
    closeDb();
    output(success({ initialized: true }));
  } catch (e) {
    output(error(
      `Failed to initialize database: ${e instanceof Error ? e.message : String(e)}`,
      "INIT_ERROR"
    ));
  }
}

main();
