#!/usr/bin/env -S deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run

/**
 * Escalate a task to the next model/thinking level
 *
 * Usage: deno run ... escalate.ts <name> [reason]
 *
 * Bumps the task to the next escalation level and resets status to pending.
 * Returns null data if already at max level.
 *
 * Escalation ladder:
 *   Level 1: sonnet + thinking (default)
 *   Level 2: sonnet + extended
 *   Level 3: opus + extended
 *   Level 4: opus + ultrathink
 */

import { getDb, initSchema, closeDb } from "./db.ts";
import {
  success,
  error,
  output,
  iso,
  getCurrentBranch,
  getNextEscalation,
  findEscalationLevel,
  ESCALATION_LADDER,
} from "./mod.ts";

async function main() {
  const name = Deno.args[0];
  const reason = Deno.args.slice(1).join(" ") || null;

  if (!name) {
    output(error("Usage: escalate.ts <name> [reason]", "INVALID_ARGS"));
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
    const now = iso();

    const task = db.prepare(`
      SELECT id, model, thinking, status FROM tasks WHERE branch = ? AND name = ?
    `).get<{ id: number; model: string; thinking: string; status: string }>(branch, name);

    if (!task) {
      closeDb();
      output(error(`Task '${name}' not found on branch '${branch}'`, "NOT_FOUND"));
      return;
    }

    const nextLevel = getNextEscalation(task.model, task.thinking);

    if (!nextLevel) {
      closeDb();
      output(success({
        escalated: false,
        at_max_level: true,
        current_level: ESCALATION_LADDER.length,
        max_level: ESCALATION_LADDER.length,
      }));
      return;
    }

    // Record escalation
    db.prepare(`
      INSERT INTO escalations (task_id, from_model, from_thinking, to_model, to_thinking, reason, escalated_at)
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `).run(task.id, task.model, task.thinking, nextLevel.model, nextLevel.thinking, reason, now);

    // Update task - reset to pending with new model/thinking
    db.prepare(`
      UPDATE tasks
      SET model = ?, thinking = ?, status = 'pending', started_at = NULL, updated_at = ?
      WHERE id = ?
    `).run(nextLevel.model, nextLevel.thinking, now, task.id);

    // Log retry event
    db.prepare(`
      INSERT INTO sessions (task_id, event, timestamp) VALUES (?, 'retry', ?)
    `).run(task.id, now);

    closeDb();

    output(success({
      escalated: true,
      name,
      from: { model: task.model, thinking: task.thinking },
      to: { model: nextLevel.model, thinking: nextLevel.thinking },
      level: nextLevel.level,
      max_level: ESCALATION_LADDER.length,
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
