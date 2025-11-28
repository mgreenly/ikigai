#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Start a task - mark it as in_progress
 */

interface State {
  current: string | null;
  status: "pending" | "in_progress" | "done";
}

interface UpdateResult {
  success: boolean;
  data?: State;
  error?: string;
  code?: string;
}

async function startTask(
  taskDir: string,
  taskNumber: string
): Promise<UpdateResult> {
  try {
    const stateFile = `${taskDir}/state.json`;

    // Read current state
    let state: State;
    try {
      const stateContent = await Deno.readTextFile(stateFile);
      state = JSON.parse(stateContent);
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) {
        // Initialize new state
        state = {
          current: null,
          status: "pending",
        };
      } else {
        return {
          success: false,
          error: `Failed to parse state file: ${error.message}`,
          code: "STATE_PARSE_ERROR",
        };
      }
    }

    // Check if waiting for verification
    if (state.status === "done") {
      return {
        success: false,
        error: `Cannot start new task while waiting for verification of task ${state.current}`,
        code: "AWAITING_VERIFICATION",
      };
    }

    // Update state to in_progress
    state.current = taskNumber;
    state.status = "in_progress";

    // Write updated state
    await Deno.writeTextFile(stateFile, JSON.stringify(state, null, 2));

    return {
      success: true,
      data: state,
    };
  } catch (error) {
    return {
      success: false,
      error: `Failed to start task: ${error.message}`,
      code: "WRITE_ERROR",
    };
  }
}

if (import.meta.main) {
  const args = Deno.args;

  if (args.length < 1 || args.includes("--help") || args.includes("-h")) {
    console.error("Usage: deno run --allow-read --allow-write task-start/run.ts [task-dir] <task-number>");
    console.error("\nArguments:");
    console.error("  task-dir      Path to task directory (default: .tasks)");
    console.error("  task-number   Task number to start (required)");
    console.error("\nExamples:");
    console.error("  deno run --allow-read --allow-write .agents/scripts/task-start/run.ts 1");
    console.error("  deno run --allow-read --allow-write .agents/scripts/task-start/run.ts ./custom-tasks 1.2");
    Deno.exit(args.length < 1 ? 1 : 0);
  }

  // Parse arguments - support both styles
  let taskDir: string;
  let taskNumber: string;

  if (args.length === 1) {
    // Single arg: task number, use default .tasks
    taskDir = ".tasks";
    taskNumber = args[0];
  } else {
    // Two args: task-dir and task number
    taskDir = args[0];
    taskNumber = args[1];
  }

  const result = await startTask(taskDir, taskNumber);
  console.log(JSON.stringify(result, null, 2));
  Deno.exit(result.success ? 0 : 1);
}
