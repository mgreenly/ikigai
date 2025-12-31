#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Log session events and return elapsed work time
 *
 * Usage:
 *   deno run --allow-read --allow-write scripts/tasks/session.ts <session.json> start <task>
 *   deno run --allow-read --allow-write scripts/tasks/session.ts <session.json> done <task>
 *   deno run --allow-read --allow-write scripts/tasks/session.ts <session.json> retry <task>
 *
 * Events:
 *   start - Begin work on a task (starts timer)
 *   done  - Complete work on a task (stops timer)
 *   retry - Log an escalation retry (informational, doesn't affect timer)
 *
 * Returns:
 *   {success: true, data: {elapsed_seconds: number, elapsed_human: string}}
 *   {success: false, error: string, code: string}
 */

type EventType = "start" | "done" | "retry";

interface SessionEvent {
  event: EventType;
  task: string;
  time: string;
}

interface SuccessResponse {
  success: true;
  data: {
    elapsed_seconds: number;
    elapsed_human: string;
  };
}

interface ErrorResponse {
  success: false;
  error: string;
  code: string;
}

type Response = SuccessResponse | ErrorResponse;

function formatElapsed(seconds: number): string {
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

function calculateElapsed(events: SessionEvent[]): number {
  let total = 0;
  let currentStart: Date | null = null;

  for (const event of events) {
    const time = new Date(event.time);
    if (event.event === "start") {
      currentStart = time;
    } else if (event.event === "done" && currentStart !== null) {
      total += Math.floor((time.getTime() - currentStart.getTime()) / 1000);
      currentStart = null;
    }
  }

  // If there's an open start, count time until now
  if (currentStart !== null) {
    total += Math.floor((Date.now() - currentStart.getTime()) / 1000);
  }

  return total;
}

function main(): Response {
  const args = Deno.args;

  if (args.length !== 3) {
    return {
      success: false,
      error: "Usage: session.ts <session.json> <start|done|retry> <task>",
      code: "INVALID_ARGS",
    };
  }

  const [sessionPath, eventType, taskName] = args;

  if (eventType !== "start" && eventType !== "done" && eventType !== "retry") {
    return {
      success: false,
      error: "Event type must be 'start', 'done', or 'retry'",
      code: "INVALID_EVENT",
    };
  }

  let events: SessionEvent[];
  try {
    const content = Deno.readTextFileSync(sessionPath);
    events = JSON.parse(content);
    if (!Array.isArray(events)) {
      return {
        success: false,
        error: "session.json must be an array",
        code: "INVALID_FORMAT",
      };
    }
  } catch (e) {
    if (e instanceof Deno.errors.NotFound) {
      // File doesn't exist, start fresh
      events = [];
    } else if (e instanceof SyntaxError) {
      return {
        success: false,
        error: `Invalid JSON in ${sessionPath}: ${e.message}`,
        code: "INVALID_JSON",
      };
    } else {
      return {
        success: false,
        error: `Cannot read ${sessionPath}: ${e instanceof Error ? e.message : String(e)}`,
        code: "READ_ERROR",
      };
    }
  }

  // Append new event
  const newEvent: SessionEvent = {
    event: eventType,
    task: taskName,
    time: new Date().toISOString(),
  };
  events.push(newEvent);

  try {
    Deno.writeTextFileSync(sessionPath, JSON.stringify(events, null, 2) + "\n");
  } catch (e) {
    return {
      success: false,
      error: `Cannot write ${sessionPath}: ${e instanceof Error ? e.message : String(e)}`,
      code: "WRITE_ERROR",
    };
  }

  const elapsed = calculateElapsed(events);

  return {
    success: true,
    data: {
      elapsed_seconds: elapsed,
      elapsed_human: formatElapsed(elapsed),
    },
  };
}

console.log(JSON.stringify(main()));
