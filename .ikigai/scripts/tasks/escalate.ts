#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Escalate a task's model/thinking to the next level
 *
 * Usage: deno run --allow-read --allow-write scripts/tasks/escalate.ts <order.json> <task-name>
 *
 * Escalation ladder:
 *   Level 1: sonnet + thinking (default)
 *   Level 2: sonnet + extended
 *   Level 3: opus + extended
 *   Level 4: opus + ultrathink
 *
 * Returns:
 *   {success: true, data: {model, thinking, level, max_level}} - escalated
 *   {success: true, data: null} - already at max level
 *   {success: false, error: string, code: string} - error
 */

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

interface EscalationLevel {
  model: string;
  thinking: string;
}

interface SuccessResponse {
  success: true;
  data: { model: string; thinking: string; level: number; max_level: number } | null;
}

interface ErrorResponse {
  success: false;
  error: string;
  code: string;
}

type Response = SuccessResponse | ErrorResponse;

const ESCALATION_LADDER: EscalationLevel[] = [
  { model: "sonnet", thinking: "thinking" },
  { model: "sonnet", thinking: "extended" },
  { model: "opus", thinking: "extended" },
  { model: "opus", thinking: "ultrathink" },
];

function findCurrentLevel(model: string, thinking: string): number {
  const index = ESCALATION_LADDER.findIndex(
    (level) => level.model === model && level.thinking === thinking
  );
  // If not found on ladder, assume level 0 (will escalate to level 1)
  return index === -1 ? 0 : index + 1;
}

function main(): Response {
  const args = Deno.args;

  if (args.length !== 2) {
    return {
      success: false,
      error: "Usage: escalate.ts <order.json> <task-name>",
      code: "INVALID_ARGS",
    };
  }

  const [orderPath, taskName] = args;

  let content: string;
  try {
    content = Deno.readTextFileSync(orderPath);
  } catch (e) {
    return {
      success: false,
      error: `Cannot read ${orderPath}: ${e instanceof Error ? e.message : String(e)}`,
      code: "FILE_NOT_FOUND",
    };
  }

  let order: OrderFile;
  try {
    order = JSON.parse(content);
  } catch (e) {
    return {
      success: false,
      error: `Invalid JSON in ${orderPath}: ${e instanceof Error ? e.message : String(e)}`,
      code: "INVALID_JSON",
    };
  }

  if (!Array.isArray(order.todo)) {
    return {
      success: false,
      error: "order.json missing 'todo' array",
      code: "INVALID_FORMAT",
    };
  }

  const taskIndex = order.todo.findIndex((t) => t.task === taskName);
  if (taskIndex === -1) {
    return {
      success: false,
      error: `Task '${taskName}' not found in todo list`,
      code: "TASK_NOT_FOUND",
    };
  }

  const task = order.todo[taskIndex];
  const currentLevel = findCurrentLevel(task.model, task.thinking);
  const maxLevel = ESCALATION_LADDER.length;

  // Already at max level
  if (currentLevel >= maxLevel) {
    return { success: true, data: null };
  }

  // Escalate to next level
  const nextLevel = ESCALATION_LADDER[currentLevel]; // currentLevel is 1-indexed, array is 0-indexed
  task.model = nextLevel.model;
  task.thinking = nextLevel.thinking;

  try {
    Deno.writeTextFileSync(orderPath, JSON.stringify(order, null, 2) + "\n");
  } catch (e) {
    return {
      success: false,
      error: `Cannot write ${orderPath}: ${e instanceof Error ? e.message : String(e)}`,
      code: "WRITE_ERROR",
    };
  }

  return {
    success: true,
    data: {
      model: nextLevel.model,
      thinking: nextLevel.thinking,
      level: currentLevel + 1,
      max_level: maxLevel,
    },
  };
}

console.log(JSON.stringify(main()));
