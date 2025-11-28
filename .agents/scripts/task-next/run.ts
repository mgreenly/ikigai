#!/usr/bin/env -S deno run --allow-read

/**
 * Get the next task to execute based on state file
 * Returns null if waiting for verification or no more tasks
 */

interface State {
  current: string | null;
  status: "pending" | "in_progress" | "done";
}

interface Task {
  number: string;
  name: string;
  filename: string;
  path: string;
}

interface GetNextResult {
  success: boolean;
  data?: {
    task: Task | null;
    reason?: string;
  };
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

function parseTaskFilename(filename: string): { number: string; name: string } | null {
  const match = filename.match(/^([\d.]+)-(.+)\.md$/);
  if (!match) return null;
  return { number: match[1], name: match[2] };
}

async function getNextTask(taskDir: string): Promise<GetNextResult> {
  try {
    const stateFile = `${taskDir}/state.json`;

    // Read state file
    let state: State;
    try {
      const stateContent = await Deno.readTextFile(stateFile);
      state = JSON.parse(stateContent);
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) {
        return {
          success: false,
          error: `State file not found: ${stateFile}`,
          code: "STATE_FILE_NOT_FOUND",
        };
      }
      return {
        success: false,
        error: `Failed to parse state file: ${error.message}`,
        code: "STATE_PARSE_ERROR",
      };
    }

    // Check if waiting for verification
    if (state.status === "done") {
      return {
        success: true,
        data: {
          task: null,
          reason: `Waiting for verification of task ${state.current}`,
        },
      };
    }

    // If in_progress, return current task
    if (state.status === "in_progress" && state.current) {
      // Find the current task file
      const tasks: Task[] = [];
      for await (const entry of Deno.readDir(taskDir)) {
        if (!entry.isFile || !entry.name.endsWith(".md") || entry.name === "README.md") continue;
        const parsed = parseTaskFilename(entry.name);
        if (!parsed) continue;
        if (parsed.number === state.current) {
          return {
            success: true,
            data: {
              task: {
                number: parsed.number,
                name: parsed.name,
                filename: entry.name,
                path: `${taskDir}/${entry.name}`,
              },
            },
          };
        }
      }
      return {
        success: false,
        error: `Current task ${state.current} not found in directory`,
        code: "CURRENT_TASK_NOT_FOUND",
      };
    }

    // Find next task (first task if current is null, otherwise first task after current)
    const tasks: Task[] = [];
    for await (const entry of Deno.readDir(taskDir)) {
      if (!entry.isFile || !entry.name.endsWith(".md") || entry.name === "README.md") continue;
      const parsed = parseTaskFilename(entry.name);
      if (!parsed) continue;
      tasks.push({
        number: parsed.number,
        name: parsed.name,
        filename: entry.name,
        path: `${taskDir}/${entry.name}`,
      });
    }

    if (tasks.length === 0) {
      return {
        success: false,
        error: "No task files found in directory",
        code: "NO_TASKS_FOUND",
      };
    }

    tasks.sort((a, b) => compareTaskNumbers(a.number, b.number));

    // Find first task if current is null, otherwise find first task after current
    const nextTask = state.current
      ? tasks.find(t => compareTaskNumbers(t.number, state.current!) > 0)
      : tasks[0];

    if (!nextTask) {
      return {
        success: true,
        data: {
          task: null,
          reason: "All tasks completed",
        },
      };
    }

    return {
      success: true,
      data: { task: nextTask },
    };
  } catch (error) {
    return {
      success: false,
      error: `Failed to get next task: ${error.message}`,
      code: "READ_ERROR",
    };
  }
}

if (import.meta.main) {
  const args = Deno.args;

  if (args.includes("--help") || args.includes("-h")) {
    console.error("Usage: deno run --allow-read get-next-task.ts [task-dir]");
    console.error("\nArguments:");
    console.error("  task-dir  Path to task directory (default: .tasks)");
    console.error("\nExamples:");
    console.error("  deno run --allow-read get-next-task.ts");
    console.error("  deno run --allow-read get-next-task.ts ./custom-tasks");
    Deno.exit(0);
  }

  const taskDir = args[0] || ".tasks";
  const result = await getNextTask(taskDir);
  console.log(JSON.stringify(result, null, 2));
  Deno.exit(result.success ? 0 : 1);
}
