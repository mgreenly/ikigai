/**
 * Task - Shared types and utilities for file-based task system
 */

import { join } from "jsr:@std/path@1";

// Response types for JSON output
export interface SuccessResponse<T> {
  success: true;
  data: T;
}

export interface ErrorResponse {
  success: false;
  error: string;
  code: string;
}

export type Response<T> = SuccessResponse<T> | ErrorResponse;

// Task types
export interface TaskEntry {
  task: string;      // filename (e.g., "setup-db.md")
  group: string;     // task group
  model: string;     // "sonnet" | "opus"
  thinking: string;  // "none" | "thinking" | "extended" | "ultrathink"
}

export interface StopEntry {
  stop: string;      // stop identifier (e.g., "verify-feature-a")
  message?: string;  // optional message explaining why we're stopping
}

export type OrderEntry = TaskEntry | StopEntry;

export interface OrderFile {
  tasks: OrderEntry[];
}

// Type guards
export function isTaskEntry(entry: OrderEntry): entry is TaskEntry {
  return 'task' in entry;
}

export function isStopEntry(entry: OrderEntry): entry is StopEntry {
  return 'stop' in entry;
}

export type TaskStatus = "pending" | "in_progress" | "completed" | "failed";

// History event types
export interface BaseHistoryEvent {
  timestamp: string;
  action: string;
  task: string;
}

export interface MoveEvent extends BaseHistoryEvent {
  action: "start" | "done" | "fail" | "reset" | "import";
  from?: TaskStatus;
  to: TaskStatus;
  reason?: string;
  elapsed_seconds?: number;
}

export interface EscalateEvent extends BaseHistoryEvent {
  action: "escalate";
  from_model: string;
  from_thinking: string;
  to_model: string;
  to_thinking: string;
  reason?: string;
}

export interface StopEvent extends BaseHistoryEvent {
  action: "stop_reached" | "stop_continue";
  stop: string;      // stop identifier
  message?: string;
}

export type HistoryEvent = MoveEvent | EscalateEvent | StopEvent;

// Escalation ladder
export const ESCALATION_LADDER = [
  { model: "sonnet", thinking: "thinking" },
  { model: "sonnet", thinking: "extended" },
  { model: "opus", thinking: "extended" },
  { model: "opus", thinking: "ultrathink" },
] as const;

export function findEscalationLevel(model: string, thinking: string): number {
  const index = ESCALATION_LADDER.findIndex(
    (level) => level.model === model && level.thinking === thinking
  );
  return index === -1 ? 0 : index + 1;
}

export function getNextEscalation(
  model: string,
  thinking: string
): { model: string; thinking: string; level: number } | null {
  const currentLevel = findEscalationLevel(model, thinking);
  if (currentLevel >= ESCALATION_LADDER.length) {
    return null;
  }
  const next = ESCALATION_LADDER[currentLevel];
  return {
    model: next.model,
    thinking: next.thinking,
    level: currentLevel + 1,
  };
}

// Paths
export function getTasksDir(): string {
  return "cdd/tasks";
}

export function getOrderPath(): string {
  return join(getTasksDir(), "order.json");
}

export function getHistoryPath(): string {
  return join(getTasksDir(), "history.jsonl");
}

export function getStatusDir(status: TaskStatus): string {
  return join(getTasksDir(), status);
}

export function getTaskPath(status: TaskStatus, taskName: string): string {
  return join(getStatusDir(status), taskName);
}

// Order file operations
export async function readOrder(): Promise<OrderFile> {
  const content = await Deno.readTextFile(getOrderPath());
  return JSON.parse(content);
}

export async function writeOrder(order: OrderFile): Promise<void> {
  await Deno.writeTextFile(
    getOrderPath(),
    JSON.stringify(order, null, 2) + "\n"
  );
}

// History operations
export async function appendHistory(event: HistoryEvent): Promise<void> {
  const line = JSON.stringify(event) + "\n";
  await Deno.writeTextFile(getHistoryPath(), line, { append: true });
}

// Find task in filesystem
export async function findTaskLocation(taskName: string): Promise<TaskStatus | null> {
  const statuses: TaskStatus[] = ["pending", "in_progress", "completed", "failed"];

  for (const status of statuses) {
    try {
      const path = getTaskPath(status, taskName);
      await Deno.stat(path);
      return status;
    } catch {
      // File doesn't exist in this directory
      continue;
    }
  }

  return null;
}

// Move task between directories
export async function moveTask(
  taskName: string,
  from: TaskStatus,
  to: TaskStatus
): Promise<void> {
  const fromPath = getTaskPath(from, taskName);
  const toPath = getTaskPath(to, taskName);

  await Deno.rename(fromPath, toPath);
}

// Get task metadata from order
export function getTaskMetadata(order: OrderFile, taskName: string): TaskEntry | null {
  const entry = order.tasks.find(t => isTaskEntry(t) && t.task === taskName);
  return entry && isTaskEntry(entry) ? entry : null;
}

// Update task metadata in order
export function updateTaskMetadata(
  order: OrderFile,
  taskName: string,
  updates: Partial<TaskEntry>
): OrderFile {
  return {
    tasks: order.tasks.map(entry =>
      isTaskEntry(entry) && entry.task === taskName
        ? { ...entry, ...updates }
        : entry
    ),
  };
}

// Check if a stop has been passed in history
export async function isStopPassed(stopId: string): Promise<boolean> {
  try {
    const historyContent = await Deno.readTextFile(getHistoryPath());
    const lines = historyContent.split("\n").filter(l => l.trim());

    // Look for stop_continue event for this stop
    for (const line of lines) {
      const event = JSON.parse(line) as HistoryEvent;
      if (event.action === "stop_continue" && 'stop' in event && event.stop === stopId) {
        return true;
      }
    }
    return false;
  } catch {
    // If history file doesn't exist or can't be read, stop hasn't been passed
    return false;
  }
}

// Count tasks by status
export async function countByStatus(): Promise<Record<TaskStatus, number>> {
  const counts: Record<TaskStatus, number> = {
    pending: 0,
    in_progress: 0,
    completed: 0,
    failed: 0,
  };

  const statuses: TaskStatus[] = ["pending", "in_progress", "completed", "failed"];

  for (const status of statuses) {
    try {
      const dir = getStatusDir(status);
      const entries = [];
      for await (const entry of Deno.readDir(dir)) {
        if (entry.isFile && entry.name.endsWith(".md")) {
          entries.push(entry);
        }
      }
      counts[status] = entries.length;
    } catch {
      counts[status] = 0;
    }
  }

  return counts;
}

// Utility functions
export function success<T>(data: T): SuccessResponse<T> {
  return { success: true, data };
}

export function error(message: string, code: string): ErrorResponse {
  return { success: false, error: message, code };
}

export function output<T>(response: Response<T>): void {
  console.log(JSON.stringify(response));
}

export function iso(): string {
  return new Date().toISOString();
}

export function formatElapsed(seconds: number): string {
  if (seconds < 60) {
    return `${seconds}s`;
  }
  const mins = Math.floor(seconds / 60);
  const secs = seconds % 60;
  if (mins < 60) {
    return secs > 0 ? `${mins}m ${secs}s` : `${mins}m`;
  }
  const hours = Math.floor(mins / 60);
  const remainingMins = mins % 60;
  if (remainingMins > 0) {
    return `${hours}h ${remainingMins}m`;
  }
  return `${hours}h`;
}

// Calculate average task time from history
export async function getAverageTaskTime(): Promise<number> {
  try {
    const historyContent = await Deno.readTextFile(getHistoryPath());
    const lines = historyContent.split("\n").filter(l => l.trim());

    let totalSeconds = 0;
    let count = 0;

    for (const line of lines) {
      const event = JSON.parse(line) as HistoryEvent;
      if ((event.action === "done" || event.action === "fail") && 'elapsed_seconds' in event && event.elapsed_seconds) {
        totalSeconds += event.elapsed_seconds;
        count++;
      }
    }

    return count > 0 ? Math.floor(totalSeconds / count) : 0;
  } catch {
    return 0;
  }
}

// Count tasks remaining until next stop (or end if no stops)
export async function countTasksToNextStop(currentTaskName: string): Promise<number> {
  try {
    const order = await readOrder();
    const counts = await countByStatus();

    // Find current task in order
    let foundCurrent = false;
    let tasksUntilStop = 0;

    for (const entry of order.tasks) {
      if (isTaskEntry(entry)) {
        // Check if this is current task
        if (entry.task === currentTaskName) {
          foundCurrent = true;
          continue; // Don't count the current task
        }

        // After current task, count pending tasks until we hit a stop
        if (foundCurrent) {
          const location = await findTaskLocation(entry.task);
          if (location === "pending") {
            tasksUntilStop++;
          }
        }
      } else if (isStopEntry(entry) && foundCurrent) {
        // Hit a stop after current task, break
        break;
      }
    }

    // If we didn't find a stop, return total pending count
    return tasksUntilStop > 0 ? tasksUntilStop : counts.pending;
  } catch {
    return 0;
  }
}
