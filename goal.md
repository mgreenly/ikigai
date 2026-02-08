Story: #0

## Objective

Update the orchestrator (`.claude/harness/orchestrator/run`) so that:

1. **FIFO ordering** — queued goals are dispatched oldest-first (lowest issue number first).
2. **Fairness** — all queued goals get at least one attempt before any failed goal is retried.

## Current Behavior

The orchestrator calls `goal-list queued` and fills slots from whatever order the API returns (typically newest-first). Failed goals are immediately re-queued with the same priority as untried goals, so a repeatedly-failing goal can starve others.

## Required Changes

### 1. Sort by issue number ascending

After fetching queued goals, sort the list by `number` ascending before selecting which goal to dispatch.

### 2. In-memory attempted-set for fairness

- Maintain a Ruby `Set` of goal numbers that have been attempted (spawned at least once) in this orchestrator session.
- When selecting the next goal to dispatch, partition queued goals into two groups:
  - **Untried**: not in the attempted set
  - **Retried**: in the attempted set
- Always prefer untried goals (oldest first). Only dispatch retried goals (oldest first) when no untried goals remain.
- Add a goal's number to the attempted set when it is spawned.
- The set is purely in-memory — lost on orchestrator restart, which is acceptable.

### 3. No other changes

- Do not change retry logic, label transitions, dependency checking, or any other behavior.
- Do not change CLI arguments or output format.

## Acceptance Criteria

- Queued goals are dispatched in ascending issue-number order.
- A goal that fails and re-queues is not retried until all other queued goals have been attempted at least once.
- Existing retry count / MAX_RETRIES / stuck logic is unchanged.
- Existing dependency checking is unchanged.
