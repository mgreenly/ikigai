#!/usr/bin/env -S deno run --allow-read

/**
 * Get the next task from order.json
 *
 * Usage: deno run --allow-read scripts/tasks/next.ts <order.json>
 *
 * Returns:
 *   {success: true, data: {task, group, model, thinking}} - next task
 *   {success: true, data: null} - no more tasks
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

interface SuccessResponse {
  success: true;
  data: TaskEntry | null;
}

interface ErrorResponse {
  success: false;
  error: string;
  code: string;
}

type Response = SuccessResponse | ErrorResponse;

function main(): Response {
  const args = Deno.args;

  if (args.length !== 1) {
    return {
      success: false,
      error: "Usage: next.ts <order.json>",
      code: "INVALID_ARGS",
    };
  }

  const orderPath = args[0];

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

  if (order.todo.length === 0) {
    return { success: true, data: null };
  }

  return { success: true, data: order.todo[0] };
}

console.log(JSON.stringify(main()));
