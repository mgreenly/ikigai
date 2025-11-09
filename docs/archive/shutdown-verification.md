# Shutdown Mechanism Verification

[See also: [Test Implementation](shutdown-test-impl.md) | [Test Execution](shutdown-test-exec.md) | [Decision Matrix](shutdown-decision-matrix.md)]

**🎯 Purpose**: Verify that libulfius properly coordinates shutdown with active WebSocket callbacks.

**👤 Intended User**: An AI agent or developer implementing Phase 1, who needs to validate the shutdown mechanism before writing production code.

**📋 Expected Outcome**: Empirical evidence of how libulfius behaves during shutdown, followed by a decision on implementation changes (if any).

**⏱️ Estimated Time**: 4-8 hours (includes writing tests, running with various tools, analyzing results)

---

## Quick Start

If you're about to implement Phase 1, **do this test first**:

1. **Read "Why This Test Exists"** (below) - Understand what assumptions we're validating
2. **Implement the test program** from [Test Implementation](shutdown-test-impl.md)
3. **Run all 4 test cases** documented in the test implementation
4. **Run with valgrind and AddressSanitizer** to catch memory issues (see [Test Execution](shutdown-test-exec.md))
5. **Determine which scenario** (A/B/C) matches reality in [Decision Matrix](shutdown-decision-matrix.md)
6. **Update the implementation** based on the decision matrix
7. **Update docs/archive/phase-1-details.md** with verified behavior

**Do not skip this test.** The entire Phase 1 safety model depends on these results.

---

## Why This Test Exists

**Critical Assumption Being Tested**: The Phase 1 implementation uses a fire-and-forget pattern where WebSocket callbacks enqueue tasks and return immediately, while worker threads process requests and send responses. We need to verify that libulfius safely handles WebSocket operations from worker threads during shutdown.

### The Safety Dependency Chain

The entire Phase 1 design relies on this sequence being safe:

```
1. Handler receives WebSocket message
2. Handler enqueues task and RETURNS IMMEDIATELY (callback completes quickly)
3. Worker thread processes task (calls OpenAI, sends chunks via websocket handle)
4. [SIGINT arrives, sets shutdown flag]
5. httpd_run() calls ulfius_stop_framework()
6. Worker sees abort flag on next poll, aborts OpenAI request
7. [What happens here? We don't know!]
8. ulfius_clean_instance() frees memory
9. Program exits
```

**If libulfius doesn't coordinate properly, we have**:
- **Use-after-free**: Worker thread uses websocket handle after it's freed by ulfius_clean_instance()
- **Memory corruption**: WebSocket handle freed while worker is sending a chunk
- **Segfault**: `ulfius_websocket_send_message()` called from worker on invalid handle
- **Race condition**: Close callback frees connection state while worker accesses it

### What We're Assuming vs. What We Know

**Assumptions in phase-1-details.md** (unverified):
> "Worker thread sends responses directly using task's websocket handle (libulfius handles locking)"

> "The websocket handle remains valid until close callback completes and joins the worker thread"

**What we actually know**:
- libulfius creates one thread per WebSocket connection (verified by source inspection)
- `ulfius_websocket_send_message()` has internal locking (documented as thread-safe)
- Nothing else about shutdown behavior or handle lifetime has been verified

### Why Manual Testing Isn't Enough

You cannot discover race conditions by manually starting the server and pressing Ctrl+C a few times:
- Race conditions are **probabilistic** - they happen based on timing
- A crash might only occur 1 in 100 shutdowns
- valgrind and AddressSanitizer can detect issues that don't immediately crash
- We need **reproducible test cases** that deliberately trigger edge cases

### The Cost of Getting This Wrong

If we implement Phase 1 based on unverified assumptions:
1. ❌ Random crashes during shutdown (hard to debug)
2. ❌ Memory leaks that grow over time
3. ❌ Corrupt responses sent to clients
4. ❌ Need to refactor shutdown mechanism later (wasted effort)
5. ❌ Loss of confidence in the codebase

**This test validates the foundation**. If these assumptions are wrong, we need to know NOW, before implementing the other modules.

---

## What This Test Does

### The Testing Approach

We isolate libulfius shutdown behavior by:

1. **Creating a minimal WebSocket server** - No OpenAI, no ikigai modules, just libulfius + callbacks
2. **Simulating long-running operations** - Callbacks that sleep/loop for several seconds
3. **Triggering shutdown mid-operation** - Send SIGINT while callbacks are still running
4. **Observing behavior** - Log everything, measure timing, check for crashes
5. **Using memory safety tools** - valgrind and AddressSanitizer to catch issues that don't immediately crash

### What We're Measuring

For each test case, we record:

- ✅ **Timing**: How long does `ulfius_stop_framework()` block?
- ✅ **Callback completion**: Do callbacks run to completion or get cancelled?
- ✅ **Memory safety**: Any use-after-free, double-free, or leaks?
- ✅ **API behavior**: What does `ulfius_websocket_send_message()` return during/after shutdown?
- ✅ **Close callback timing**: When does it execute relative to message callback?

### The Key Questions to Answer

After testing, we need to know:

| Question | Why It Matters |
|----------|----------------|
| **Q1: WebSocket handle lifetime** | When does the handle become invalid? After `ulfius_stop_framework()`? After close callback? |
| **Q2: Worker thread safety** | Can worker threads safely call `ulfius_websocket_send_message()` after shutdown starts? |
| **Q3: Close callback timing** | Does close callback wait for worker to finish, or can it run concurrently? |
| **Q4: Cleanup ordering** | Can we safely `pthread_join()` worker from close callback without deadlock? |

**Answers to these questions determine if fire-and-forget pattern is safe.**

---

## Problem Statement

The Phase 1 design uses a fire-and-forget pattern where WebSocket message callbacks return immediately after enqueueing tasks, and worker threads handle all processing and send responses. We need to empirically test what happens when:

1. Worker thread is processing a task (calling OpenAI, sending chunks)
2. SIGINT/SIGTERM arrives and sets `g_httpd_shutdown = 1`
3. `ulfius_stop_framework()` is called while worker is active
4. Worker sees shutdown flag and aborts
5. Close callback runs and tries to join worker thread

**Specific unknowns we must answer**:
- Is the websocket handle valid for worker after `ulfius_stop_framework()` is called?
- Can worker safely call `ulfius_websocket_send_message()` during shutdown?
- When does the close callback execute? Can it run while worker is active?
- Can we safely `pthread_join(worker)` from close callback without deadlock?
- What happens to `ulfius_websocket_send_message()` after connection closes?
- How long does it take for worker to abort and cleanup to complete?

---

## When You Can Skip This Test

**Never.** This is a prerequisite for Phase 1 implementation.

If you're tempted to skip it because:
- ❌ "It's probably fine" → Race conditions don't show up in manual testing
- ❌ "We can fix it later" → Refactoring concurrency is expensive and error-prone
- ❌ "Other projects use libulfius" → Their usage pattern might be different
- ❌ "The docs say it's thread-safe" → Thread-safe ≠ shutdown-safe

**Do the test.** It takes 4-8 hours. Fixing a race condition in production takes weeks.

---

## Next Steps

1. **Implement the tests**: See [Test Implementation](shutdown-test-impl.md)
2. **Run the tests**: See [Test Execution](shutdown-test-exec.md)
3. **Analyze results**: See [Decision Matrix](shutdown-decision-matrix.md)
4. **Update documentation** with verified behavior
