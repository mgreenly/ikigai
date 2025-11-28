#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Verify current task and advance to next task
 */

interface State {
  current: string | null;
  status: "pending" | "in_progress" | "done";
}

interface UpdateResult {
  success: boolean;
  data?: State & { next_task?: string | null };
  error?: string;
  code?: string;
}

function compareTaskNumbers(a: string, b: string): number {
  const aParts = a.split(".").map(Number);
  const bParts = b.split(".").map(Number);
  const maxLength = Math.max(aParts.length, bParts.length);

  for (let i = 0; i < maxLength; i++) {
    const aVal = i < aParts.length ? aParts[i] : 0;
    const bVal = i < bParts.length ? bParts[i] : 0;
    if (aVal !== bVal) return aVal - bVal;
  }
  return 0;
}

function parseTaskFilename(filename: string): { number: string } | null {
  const match = filename.match(/^([\d.]+)-(.+)\.md$/);
  if (!match) return null;
  return { number: match[1] };
}

async function findNextTaskNumber(taskDir: string, afterTask: string): Promise<string | null> {
  const tasks: string[] = [];
  for await (const entry of Deno.readDir(taskDir)) {
    if (!entry.isFile || !entry.name.endsWith(".md") || entry.name === "README.md") continue;
    const parsed = parseTaskFilename(entry.name);
    if (parsed) tasks.push(parsed.number);
  }

  tasks.sort(compareTaskNumbers);
  return tasks.find(t => compareTaskNumbers(t, afterTask) > 0) || null;
}

async function verifyTask(taskDir: string): Promise<UpdateResult> {
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
          error: "State file not found",
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

    // Check if task is done
    if (state.status !== "done") {
      return {
        success: false,
        error: "Can only verify tasks that are done",
        code: "INVALID_TRANSITION",
      };
    }

    // Find next task
    const nextTask = await findNextTaskNumber(taskDir, state.current!);

    // Advance to next task
    state.current = nextTask;
    state.status = "pending";

    // Write updated state
    await Deno.writeTextFile(stateFile, JSON.stringify(state, null, 2));

    return {
      success: true,
      data: {
        ...state,
        next_task: nextTask,
      },
    };
  } catch (error) {
    return {
      success: false,
      error: `Failed to verify task: ${error.message}`,
      code: "WRITE_ERROR",
    };
  }
}

if (import.meta.main) {
  const args = Deno.args;

  if (args.includes("--help") || args.includes("-h")) {
    console.error("Usage: deno run --allow-read --allow-write task-verify/run.ts [task-dir]");
    console.error("\nArguments:");
    console.error("  task-dir  Path to task directory (default: .tasks)");
    console.error("\nExamples:");
    console.error("  deno run --allow-read --allow-write .agents/scripts/task-verify/run.ts");
    console.error("  deno run --allow-read --allow-write .agents/scripts/task-verify/run.ts ./custom-tasks");
    Deno.exit(0);
  }

  const taskDir = args[0] || ".tasks";
  const result = await verifyTask(taskDir);
  console.log(JSON.stringify(result, null, 2));
  Deno.exit(result.success ? 0 : 1);
}
