#!/usr/bin/env -S deno run --allow-read --allow-write --allow-env

/**
 * Escalate a task to the next model/thinking level
 *
 * Usage: deno run --allow-read --allow-write escalate.ts <name> [reason]
 *
 * Bumps the task to the next escalation level in order.json, resets status to pending,
 * and logs to history. Returns null data if already at max level.
 *
 * Escalation ladder:
 *   Level 1: sonnet + thinking (default)
 *   Level 2: sonnet + extended
 *   Level 3: opus + extended
 *   Level 4: opus + ultrathink
 */

import {
  success,
  error,
  output,
  iso,
  findTaskLocation,
  moveTask,
  appendHistory,
  readOrder,
  writeOrder,
  getTaskMetadata,
  updateTaskMetadata,
  getNextEscalation,
  ESCALATION_LADDER,
} from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  const reason = Deno.args.slice(1).join(" ") || undefined;

  if (!name) {
    output(error("Usage: escalate.ts <name> [reason]", "INVALID_ARGS"));
    return;
  }

  try {
    const location = await findTaskLocation(name);

    if (!location) {
      output(error(`Task '${name}' not found`, "NOT_FOUND"));
      return;
    }

    // Read current task metadata
    const order = await readOrder();
    const taskMeta = getTaskMetadata(order, name);

    if (!taskMeta) {
      output(error(`Task '${name}' not found in order.json`, "NOT_FOUND"));
      return;
    }

    // Get next escalation level
    const nextLevel = getNextEscalation(taskMeta.model, taskMeta.thinking);

    if (!nextLevel) {
      output(success({
        escalated: false,
        at_max_level: true,
        current_level: ESCALATION_LADDER.length,
        max_level: ESCALATION_LADDER.length,
      }));
      return;
    }

    const now = iso();

    // Update order.json with new model/thinking
    const updatedOrder = updateTaskMetadata(order, name, {
      model: nextLevel.model,
      thinking: nextLevel.thinking,
    });
    await writeOrder(updatedOrder);

    // Log escalation to history
    await appendHistory({
      timestamp: now,
      action: "escalate",
      task: name,
      from_model: taskMeta.model,
      from_thinking: taskMeta.thinking,
      to_model: nextLevel.model,
      to_thinking: nextLevel.thinking,
      reason,
    });

    // If task is in_progress or failed, move back to pending
    if (location === "in_progress" || location === "failed") {
      await moveTask(name, location, "pending");

      await appendHistory({
        timestamp: now,
        action: "reset",
        task: name,
        from: location,
        to: "pending",
      });
    }

    output(success({
      escalated: true,
      name,
      from: { model: taskMeta.model, thinking: taskMeta.thinking },
      to: { model: nextLevel.model, thinking: nextLevel.thinking },
      level: nextLevel.level,
      max_level: ESCALATION_LADDER.length,
    }));
  } catch (e) {
    output(error(
      `Failed to escalate task: ${e instanceof Error ? e.message : String(e)}`,
      "ESCALATE_ERROR"
    ));
  }
}

main();
