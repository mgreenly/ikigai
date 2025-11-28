#!/usr/bin/env -S deno run --allow-read

/**
 * Retrieve and sort task files from a task directory
 * Returns tasks in proper decimal numerical order (1 → 1.1 → 1.1.1 → 1.2 → 2)
 */

interface Task {
  number: string;
  name: string;
  filename: string;
  path: string;
}

interface TasksResult {
  success: boolean;
  data?: {
    tasks: Task[];
    total_count: number;
    returned_count: number;
    has_more: boolean;
    start_after?: string;
  };
  error?: string;
  code?: string;
}

/**
 * Parse task filename to extract number and name
 * Format: {number}-{name}.md
 * Example: "1.2.3-database-setup.md" → { number: "1.2.3", name: "database-setup" }
 */
function parseTaskFilename(filename: string): { number: string; name: string } | null {
  const match = filename.match(/^([\d.]+)-(.+)\.md$/);
  if (!match) return null;
  return {
    number: match[1],
    name: match[2],
  };
}

/**
 * Compare two decimal task numbers for sorting
 * Handles proper decimal ordering: 1 < 1.1 < 1.1.1 < 1.2 < 2
 */
function compareTaskNumbers(a: string, b: string): number {
  const aParts = a.split(".").map(Number);
  const bParts = b.split(".").map(Number);

  const maxLength = Math.max(aParts.length, bParts.length);

  for (let i = 0; i < maxLength; i++) {
    const aVal = i < aParts.length ? aParts[i] : 0;
    const bVal = i < bParts.length ? bParts[i] : 0;

    if (aVal !== bVal) {
      return aVal - bVal;
    }
  }

  return 0;
}

async function getTasks(
  taskDir: string,
  count: number,
  startAfter?: string
): Promise<TasksResult> {
  try {
    // Check if directory exists
    let dirInfo;
    try {
      dirInfo = await Deno.stat(taskDir);
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) {
        return {
          success: false,
          error: `Task directory not found: ${taskDir}`,
          code: "DIR_NOT_FOUND",
        };
      }
      throw error;
    }

    if (!dirInfo.isDirectory) {
      return {
        success: false,
        error: `Path is not a directory: ${taskDir}`,
        code: "NOT_A_DIRECTORY",
      };
    }

    // Read all .md files from directory
    const tasks: Task[] = [];
    for await (const entry of Deno.readDir(taskDir)) {
      if (!entry.isFile || !entry.name.endsWith(".md")) continue;

      // Skip README.md
      if (entry.name === "README.md") continue;

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
        error: `No task files found in directory: ${taskDir}`,
        code: "NO_TASKS_FOUND",
      };
    }

    // Sort tasks by decimal number
    tasks.sort((a, b) => compareTaskNumbers(a.number, b.number));

    // Find starting position if startAfter is specified
    let startIndex = 0;
    if (startAfter) {
      const afterIndex = tasks.findIndex(t => t.number === startAfter);
      if (afterIndex === -1) {
        return {
          success: false,
          error: `Task number not found: ${startAfter}`,
          code: "START_TASK_NOT_FOUND",
        };
      }
      startIndex = afterIndex + 1;
    }

    // Slice the requested number of tasks
    const selectedTasks = tasks.slice(startIndex, startIndex + count);
    const hasMore = startIndex + count < tasks.length;

    return {
      success: true,
      data: {
        tasks: selectedTasks,
        total_count: tasks.length,
        returned_count: selectedTasks.length,
        has_more: hasMore,
        ...(startAfter && { start_after: startAfter }),
      },
    };
  } catch (error) {
    return {
      success: false,
      error: `Failed to read tasks: ${error.message}`,
      code: "READ_ERROR",
    };
  }
}

// Main execution
if (import.meta.main) {
  const args = Deno.args;

  if (args.length === 0 || args.includes("--help") || args.includes("-h")) {
    console.error("Usage: deno run --allow-read run.ts <task-dir> [count] [start-after]");
    console.error("\nArguments:");
    console.error("  task-dir      Path to task directory (required)");
    console.error("  count         Number of tasks to return (default: 5)");
    console.error("  start-after   Task number to start after (optional)");
    console.error("\nExamples:");
    console.error("  deno run --allow-read run.ts ./tasks");
    console.error("  deno run --allow-read run.ts ./tasks 10");
    console.error("  deno run --allow-read run.ts ./tasks 5 1.2");
    Deno.exit(args.length === 0 ? 1 : 0);
  }

  const taskDir = args[0];
  const count = args[1] ? parseInt(args[1], 10) : 5;
  const startAfter = args[2];

  if (isNaN(count) || count <= 0) {
    console.error("Error: count must be a positive number");
    Deno.exit(1);
  }

  const result = await getTasks(taskDir, count, startAfter);
  console.log(JSON.stringify(result, null, 2));

  Deno.exit(result.success ? 0 : 1);
}
