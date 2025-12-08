#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Mark a task as done in order.json
 *
 * Usage: deno run --allow-read --allow-write scripts/tasks/done.ts <order.json> <task-name>
 *
 * Moves the task from todo[] to done[].
 *
 * Returns:
 *   {success: true, data: {remaining: number}} - task marked done
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
  data: { remaining: number };
}

interface ErrorResponse {
  success: false;
  error: string;
  code: string;
}

type Response = SuccessResponse | ErrorResponse;

function main(): Response {
  const args = Deno.args;

  if (args.length !== 2) {
    return {
      success: false,
      error: "Usage: done.ts <order.json> <task-name>",
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

  if (!Array.isArray(order.todo) || !Array.isArray(order.done)) {
    return {
      success: false,
      error: "order.json missing 'todo' or 'done' array",
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

  // Move task from todo to done
  const [task] = order.todo.splice(taskIndex, 1);
  order.done.push(task);

  try {
    Deno.writeTextFileSync(orderPath, JSON.stringify(order, null, 2) + "\n");
  } catch (e) {
    return {
      success: false,
      error: `Cannot write ${orderPath}: ${e instanceof Error ? e.message : String(e)}`,
      code: "WRITE_ERROR",
    };
  }

  return { success: true, data: { remaining: order.todo.length } };
}

console.log(JSON.stringify(main()));
