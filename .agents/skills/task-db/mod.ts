/**
 * Task DB - Shared types and utilities
 */

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

// Database types
export interface Task {
  id: number;
  branch: string;
  name: string;
  content: string;
  task_group: string | null;
  model: string;
  thinking: string;
  status: TaskStatus;
  created_at: string;
  updated_at: string;
  started_at: string | null;
  completed_at: string | null;
}

export type TaskStatus = "pending" | "in_progress" | "done" | "failed";

export interface TaskSummary {
  id: number;
  name: string;
  task_group: string | null;
  model: string;
  thinking: string;
  status: TaskStatus;
}

export interface Escalation {
  id: number;
  task_id: number;
  from_model: string;
  from_thinking: string;
  to_model: string;
  to_thinking: string;
  reason: string | null;
  escalated_at: string;
}

export interface SessionEvent {
  id: number;
  task_id: number;
  event: "start" | "done" | "retry";
  timestamp: string;
}

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

export async function getCurrentBranch(): Promise<string> {
  const command = new Deno.Command("git", {
    args: ["branch", "--show-current"],
    stdout: "piped",
    stderr: "piped",
  });
  const { code, stdout } = await command.output();
  if (code !== 0) {
    throw new Error("Failed to get current git branch");
  }
  return new TextDecoder().decode(stdout).trim();
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
