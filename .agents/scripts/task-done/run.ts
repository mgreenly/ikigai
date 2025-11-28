#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Mark current task as done - awaiting verification
 */

interface State {
  last_verified: string | null;
  current: string | null;
  status: "pending" | "in_progress" | "done";
}

interface UpdateResult {
  success: boolean;
  data?: State;
  error?: string;
  code?: string;
}

async function markTaskDone(taskDir: string): Promise<UpdateResult> {
  try {
    const stateFile = `${taskDir}/state.json`;

    // Read current state
    let state: State;
    try {
      const stateContent = await Deno.readTextFile(stateFile);
      state = JSON.parse(stateContent);
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) {
        return {
          success: false,
          error: "State file not found - no task is in progress",
          code: "STATE_FILE_NOT_FOUND",
        };
      } else {
        return {
          success: false,
          error: `Failed to parse state file: ${error.message}`,
          code: "STATE_PARSE_ERROR",
        };
      }
    }

    // Check if task is in_progress
    if (state.status !== "in_progress") {
      return {
        success: false,
        error: "Can only mark task as done when in_progress",
        code: "INVALID_TRANSITION",
      };
    }

    // Update state to done
    state.status = "done";

    // Write updated state
    await Deno.writeTextFile(stateFile, JSON.stringify(state, null, 2));

    return {
      success: true,
      data: state,
    };
  } catch (error) {
    return {
      success: false,
      error: `Failed to mark task as done: ${error.message}`,
      code: "WRITE_ERROR",
    };
  }
}

if (import.meta.main) {
  const args = Deno.args;

  if (args.includes("--help") || args.includes("-h")) {
    console.error("Usage: deno run --allow-read --allow-write task-done/run.ts [task-dir]");
    console.error("\nArguments:");
    console.error("  task-dir  Path to task directory (default: .tasks)");
    console.error("\nExamples:");
    console.error("  deno run --allow-read --allow-write .agents/scripts/task-done/run.ts");
    console.error("  deno run --allow-read --allow-write .agents/scripts/task-done/run.ts ./custom-tasks");
    Deno.exit(0);
  }

  const taskDir = args[0] || ".tasks";
  const result = await markTaskDone(taskDir);
  console.log(JSON.stringify(result, null, 2));
  Deno.exit(result.success ? 0 : 1);
}
