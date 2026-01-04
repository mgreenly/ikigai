#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Mark a stop as passed and continue execution
 *
 * Usage: deno run --allow-read --allow-write continue.ts <stop-id>
 *
 * Records a stop_continue event in history, allowing the orchestrator to proceed.
 */

import {
  success,
  error,
  output,
  iso,
  appendHistory,
  readOrder,
  isStopEntry,
  isStopPassed,
} from "./mod.ts";

async function main() {
  const stopId = Deno.args[0];
  if (!stopId) {
    output(error("Usage: continue.ts <stop-id>", "INVALID_ARGS"));
    return;
  }

  try {
    // Check if stop exists in order.json
    const order = await readOrder();
    const stopEntry = order.tasks.find(
      entry => isStopEntry(entry) && entry.stop === stopId
    );

    if (!stopEntry) {
      output(error(`Stop '${stopId}' not found in order.json`, "NOT_FOUND"));
      return;
    }

    // Check if already passed
    const passed = await isStopPassed(stopId);
    if (passed) {
      output(error(`Stop '${stopId}' already passed`, "ALREADY_PASSED"));
      return;
    }

    const now = iso();

    // Log stop_continue event
    await appendHistory({
      timestamp: now,
      action: "stop_continue",
      task: stopId,
      stop: stopId,
      message: isStopEntry(stopEntry) ? stopEntry.message : undefined,
    });

    output(success({
      stop: stopId,
      continued_at: now,
      message: "Stop marked as passed, orchestrator can continue",
    }));
  } catch (e) {
    output(error(
      `Failed to continue: ${e instanceof Error ? e.message : String(e)}`,
      "CONTINUE_ERROR"
    ));
  }
}

main();
