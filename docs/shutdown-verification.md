# Shutdown Mechanism Verification

**🎯 Purpose**: Verify that libulfius properly coordinates shutdown with active WebSocket callbacks.

**👤 Intended User**: An AI agent or developer implementing Phase 1, who needs to validate the shutdown mechanism before writing production code.

**📋 Expected Outcome**: Empirical evidence of how libulfius behaves during shutdown, followed by a decision on implementation changes (if any).

**⏱️ Estimated Time**: 4-8 hours (includes writing tests, running with various tools, analyzing results)

---

## Quick Start

If you're about to implement Phase 1, **do this test first**:

1. **Read "Why This Test Exists"** - Understand what assumptions we're validating
2. **Implement the test program** from "Phase 1: Minimal Reproduction Test"
3. **Run all 4 test cases** documented in "Test Cases"
4. **Run with valgrind and AddressSanitizer** to catch memory issues
5. **Determine which scenario** (A/B/C) matches reality in "Expected Outcomes & Decision Matrix"
6. **Update the implementation** based on the decision matrix
7. **Update docs/phase-1-details.md** with verified behavior

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

## Test Strategy

### Phase 1: Minimal Reproduction Test

Create a standalone test program that isolates libulfius shutdown behavior without OpenAI dependencies.

**Test program**: `tests/shutdown/ulfius_shutdown_test.c`

#### Test Cases

**Test 1: Callback Execution During Shutdown**
```c
// Setup:
// 1. Start ulfius server with WebSocket endpoint
// 2. Connect client via WebSocket
// 3. Client sends message
// 4. Server callback starts executing and sleeps for 5 seconds
// 5. After 1 second, send SIGINT to server process
// 6. Observe behavior

// Expected outcomes to verify:
// A. Does ulfius_stop_framework() block until callback completes?
// B. Does ulfius_stop_framework() return immediately and cancel callback?
// C. Does callback crash/segfault when using websocket handle?
// D. How long does shutdown take?
```

**Test 2: WebSocket Send During Shutdown**
```c
// Setup:
// 1. Start ulfius server with WebSocket endpoint
// 2. Connect client via WebSocket
// 3. Client sends message
// 4. Server callback enters loop:
//    - Check shutdown flag
//    - If not shutdown, send message chunk and sleep 200ms
//    - Repeat for 10 iterations (2 seconds total)
// 5. After 500ms, send SIGINT to server process
// 6. Observe behavior

// Expected outcomes to verify:
// A. Does ulfius_websocket_send_message() work during shutdown?
// B. Does it return error codes? What codes?
// C. Does it crash/block/succeed?
// D. When does the callback actually exit?
```

**Test 3: Multiple Concurrent Callbacks During Shutdown**
```c
// Setup:
// 1. Start ulfius server with WebSocket endpoint
// 2. Connect 5 clients simultaneously
// 3. All clients send messages at the same time
// 4. Each callback sleeps for 3 seconds (different times per callback)
// 5. After 1 second, send SIGINT to server process
// 6. Observe behavior

// Expected outcomes to verify:
// A. Does ulfius_stop_framework() wait for ALL callbacks?
// B. Do some callbacks get cancelled while others complete?
// C. What is the maximum shutdown time?
// D. Any memory leaks or crashes?
```

**Test 4: Client Disconnect During Callback**
```c
// Setup:
// 1. Start ulfius server with WebSocket endpoint
// 2. Connect client via WebSocket
// 3. Client sends message
// 4. Server callback starts loop (send chunk + sleep, 10 iterations)
// 5. After 500ms, client disconnects (close WebSocket)
// 6. Observe behavior

// Expected outcomes to verify:
// A. Does close callback run immediately or wait for message callback?
// B. What happens to ulfius_websocket_send_message() calls after disconnect?
// C. Does callback continue running or get cancelled?
// D. Can we access connection state safely?
```

#### Implementation Skeleton

```c
#include <ulfius.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// Global state
static volatile sig_atomic_t shutdown_requested = 0;
static pthread_mutex_t callback_mutex = PTHREAD_MUTEX_INITIALIZER;
static int active_callbacks = 0;

// Per-connection context
typedef struct {
    int connection_id;
    char *name;
} test_conn_t;

void signal_handler(int signum) {
    (void)signum;
    printf("[SIGNAL] Shutdown requested\n");
    shutdown_requested = 1;
}

void websocket_onopen_callback(const struct _u_request *request,
                               struct _u_websocket_manager *manager,
                               void *user_data) {
    printf("[OPEN] Connection opened\n");
    // Allocate connection context
    test_conn_t *conn = malloc(sizeof(test_conn_t));
    conn->connection_id = /* generate ID */;
    conn->name = strdup("test-connection");

    ulfius_set_websocket_user_data(manager, conn);
}

void websocket_onmessage_callback(const struct _u_request *request,
                                  struct _u_websocket_manager *manager,
                                  const struct _u_websocket_message *message,
                                  void *user_data) {
    test_conn_t *conn = ulfius_websocket_user_data_get(manager);

    pthread_mutex_lock(&callback_mutex);
    active_callbacks++;
    printf("[MESSAGE] Callback started (active=%d, conn_id=%d)\n",
           active_callbacks, conn->connection_id);
    pthread_mutex_unlock(&callback_mutex);

    // Simulate long-running operation
    for (int i = 0; i < 10; i++) {
        if (shutdown_requested) {
            printf("[MESSAGE] Shutdown detected at iteration %d\n", i);
            break;
        }

        printf("[MESSAGE] Processing iteration %d (conn_id=%d)\n",
               i, conn->connection_id);

        // Try to send message
        char response[64];
        snprintf(response, sizeof(response), "chunk %d", i);
        int ret = ulfius_websocket_send_message(manager,
                                                U_WEBSOCKET_OPCODE_TEXT,
                                                strlen(response),
                                                response);

        printf("[MESSAGE] Send result: %d\n", ret);

        usleep(500000); // 500ms
    }

    pthread_mutex_lock(&callback_mutex);
    active_callbacks--;
    printf("[MESSAGE] Callback finished (active=%d, conn_id=%d)\n",
           active_callbacks, conn->connection_id);
    pthread_mutex_unlock(&callback_mutex);
}

void websocket_onclose_callback(const struct _u_request *request,
                                struct _u_websocket_manager *manager,
                                void *user_data) {
    test_conn_t *conn = ulfius_websocket_user_data_get(manager);

    printf("[CLOSE] Connection closing (conn_id=%d, active_callbacks=%d)\n",
           conn ? conn->connection_id : -1, active_callbacks);

    if (conn) {
        free(conn->name);
        free(conn);
    }

    printf("[CLOSE] Connection closed\n");
}

int main(void) {
    struct _u_instance instance;

    // Initialize
    if (ulfius_init_instance(&instance, 8080, NULL, NULL) != U_OK) {
        fprintf(stderr, "Failed to initialize instance\n");
        return EXIT_FAILURE;
    }

    // Register WebSocket endpoint
    if (ulfius_add_websocket_endpoint(&instance, "/ws",
                                      &websocket_onopen_callback,
                                      &websocket_onmessage_callback,
                                      &websocket_onclose_callback,
                                      NULL) != U_OK) {
        fprintf(stderr, "Failed to add websocket endpoint\n");
        ulfius_clean_instance(&instance);
        return EXIT_FAILURE;
    }

    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Start server
    printf("[MAIN] Starting server on port 8080\n");
    if (ulfius_start_framework(&instance) != U_OK) {
        fprintf(stderr, "Failed to start server\n");
        ulfius_clean_instance(&instance);
        return EXIT_FAILURE;
    }

    printf("[MAIN] Server running, waiting for shutdown signal\n");

    // Main loop
    while (!shutdown_requested) {
        usleep(100000); // 100ms
    }

    printf("[MAIN] Shutdown signal received, stopping server (active_callbacks=%d)\n",
           active_callbacks);
    printf("[MAIN] Calling ulfius_stop_framework()...\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ulfius_stop_framework(&instance);

    clock_gettime(CLOCK_MONOTONIC, &end);
    long shutdown_ms = (end.tv_sec - start.tv_sec) * 1000 +
                      (end.tv_nsec - start.tv_nsec) / 1000000;

    printf("[MAIN] ulfius_stop_framework() returned after %ld ms\n", shutdown_ms);
    printf("[MAIN] Active callbacks after stop: %d\n", active_callbacks);

    printf("[MAIN] Cleaning up instance\n");
    ulfius_clean_instance(&instance);

    printf("[MAIN] Shutdown complete\n");
    return EXIT_SUCCESS;
}
```

#### Client Test Script

Create a Python WebSocket client to automate testing:

**File**: `tests/shutdown/ws_test_client.py`

```python
#!/usr/bin/env python3
import asyncio
import websockets
import sys
import time

async def test_client(client_id, message_count=1):
    uri = "ws://localhost:8080/ws"

    try:
        print(f"[CLIENT-{client_id}] Connecting to {uri}")
        async with websockets.connect(uri) as websocket:
            print(f"[CLIENT-{client_id}] Connected")

            for i in range(message_count):
                message = f"test message {i} from client {client_id}"
                print(f"[CLIENT-{client_id}] Sending: {message}")
                await websocket.send(message)

                # Receive responses
                try:
                    while True:
                        response = await asyncio.wait_for(
                            websocket.recv(),
                            timeout=1.0
                        )
                        print(f"[CLIENT-{client_id}] Received: {response}")
                except asyncio.TimeoutError:
                    print(f"[CLIENT-{client_id}] No more responses")
                    break
                except websockets.exceptions.ConnectionClosed:
                    print(f"[CLIENT-{client_id}] Connection closed by server")
                    break

                if i < message_count - 1:
                    await asyncio.sleep(0.5)

    except Exception as e:
        print(f"[CLIENT-{client_id}] Error: {e}")

async def test_multiple_clients(num_clients):
    """Test with multiple concurrent clients"""
    tasks = [test_client(i) for i in range(num_clients)]
    await asyncio.gather(*tasks)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        num_clients = int(sys.argv[1])
    else:
        num_clients = 1

    asyncio.run(test_multiple_clients(num_clients))
```

#### Test Execution Script

**File**: `tests/shutdown/run_shutdown_tests.sh`

```bash
#!/bin/bash
set -e

# Build test program
echo "=== Building shutdown test ==="
gcc -o ulfius_shutdown_test ulfius_shutdown_test.c \
    -lulfius -lpthread -Wall -Wextra -g

echo ""
echo "=== Test 1: Single callback during shutdown ==="
echo "Expected: Verify if ulfius_stop_framework() waits for callback"
echo ""
echo "Steps:"
echo "1. Run: ./ulfius_shutdown_test"
echo "2. In another terminal, run: python3 ws_test_client.py 1"
echo "3. Wait 1 second, then press Ctrl+C in server terminal"
echo "4. Observe shutdown timing and any errors"
echo ""
read -p "Press Enter to continue..."

echo ""
echo "=== Test 2: Multiple concurrent callbacks during shutdown ==="
echo "Expected: Verify shutdown behavior with multiple active connections"
echo ""
echo "Steps:"
echo "1. Run: ./ulfius_shutdown_test"
echo "2. In another terminal, run: python3 ws_test_client.py 5"
echo "3. Wait 1 second, then press Ctrl+C in server terminal"
echo "4. Observe if all callbacks complete or get cancelled"
echo ""
read -p "Press Enter to continue..."

echo ""
echo "=== Test 3: Client disconnect during callback ==="
echo "Expected: Verify close callback behavior during message processing"
echo ""
echo "Steps:"
echo "1. Run: ./ulfius_shutdown_test"
echo "2. In another terminal, run: python3 ws_test_client.py 1"
echo "3. Kill the client with Ctrl+C after 1 second"
echo "4. Observe server callback behavior"
echo ""
read -p "Press Enter to continue..."

echo ""
echo "=== Running with Valgrind ==="
echo "Checking for memory leaks and use-after-free"
echo ""
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes --verbose \
         ./ulfius_shutdown_test &

VALGRIND_PID=$!
sleep 2

python3 ws_test_client.py 3 &
CLIENT_PID=$!

sleep 2
echo "Sending SIGINT to test program..."
kill -INT $VALGRIND_PID

wait $VALGRIND_PID
wait $CLIENT_PID

echo ""
echo "=== Tests complete ==="
echo "Review output above for:"
echo "- Shutdown timing (should meet < 200ms requirement)"
echo "- Memory leaks (valgrind should show no leaks)"
echo "- Use-after-free errors (valgrind should show none)"
echo "- Whether callbacks continue or get cancelled"
```

---

### Phase 2: Integration with OpenAI Mock

After understanding libulfius behavior, test with OpenAI-like streaming:

**File**: `tests/shutdown/openai_mock_server.py`

```python
#!/usr/bin/env python3
"""Mock OpenAI SSE endpoint for testing shutdown behavior"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import time
import json

class OpenAIMockHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/v1/chat/completions":
            self.send_error(404)
            return

        content_length = int(self.headers['Content-Length'])
        body = self.rfile.read(content_length)

        # Send SSE stream
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.end_headers()

        # Simulate slow streaming response (10 chunks over 5 seconds)
        for i in range(10):
            chunk = {
                "id": "chatcmpl-test",
                "object": "chat.completion.chunk",
                "created": int(time.time()),
                "model": "gpt-4o-mini",
                "choices": [{
                    "index": 0,
                    "delta": {"content": f"chunk {i} "},
                    "finish_reason": None
                }]
            }

            data = f"data: {json.dumps(chunk)}\n\n"
            try:
                self.wfile.write(data.encode())
                self.wfile.flush()
                time.sleep(0.5)
            except BrokenPipeError:
                print(f"[MOCK] Client disconnected at chunk {i}")
                return

        # Send final chunk
        final = {
            "id": "chatcmpl-test",
            "object": "chat.completion.chunk",
            "created": int(time.time()),
            "model": "gpt-4o-mini",
            "choices": [{
                "index": 0,
                "delta": {},
                "finish_reason": "stop"
            }]
        }
        self.wfile.write(f"data: {json.dumps(final)}\n\n".encode())
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()

if __name__ == "__main__":
    server = HTTPServer(('127.0.0.1', 8081), OpenAIMockHandler)
    print("Mock OpenAI server listening on http://127.0.0.1:8081")
    server.serve_forever()
```

Test the full ikigai server with mock OpenAI:

1. Start mock OpenAI: `python3 openai_mock_server.py`
2. Configure ikigai to use `http://127.0.0.1:8081` as OpenAI base URL
3. Run ikigai server
4. Connect client and send query
5. During streaming, send SIGINT
6. Verify graceful shutdown

---

## Expected Outcomes & Decision Matrix

### Scenario A: libulfius waits for callbacks to complete

**Observations:**
- `ulfius_stop_framework()` blocks until all callbacks return
- Shutdown time = max callback runtime
- No crashes, no memory leaks

**Decision:**
- ✅ Current design is safe
- ✅ Keep synchronous blocking pattern
- ⚠️ Add explicit timeout (max 10 seconds) to prevent hung callbacks from blocking shutdown indefinitely
- ⚠️ Document that long-running OpenAI requests delay shutdown

**Implementation changes:**
```c
// httpd.c
while (!g_httpd_shutdown) {
    usleep(200000);
}

ik_log_info("Shutting down server...");
ulfius_stop_framework(&instance);  // Blocks until callbacks complete
ulfius_clean_instance(&instance);
```

---

### Scenario B: libulfius returns immediately, callbacks continue

**Observations:**
- `ulfius_stop_framework()` returns quickly
- Callbacks continue running in background threads
- `ulfius_websocket_send_message()` might fail/crash after stop
- Memory might be freed while callbacks access it

**Decision:**
- ❌ Current design is UNSAFE
- 🔧 Must implement callback tracking and synchronization

**Implementation changes:**

```c
// Add global callback tracker
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t all_done;
    int active_count;
} ik_callback_tracker_t;

static ik_callback_tracker_t g_callbacks = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .all_done = PTHREAD_COND_INITIALIZER,
    .active_count = 0
};

// Handler module - wrap callbacks
void ik_handler_websocket_message_callback(...) {
    pthread_mutex_lock(&g_callbacks.mutex);
    g_callbacks.active_count++;
    pthread_mutex_unlock(&g_callbacks.mutex);

    // ... existing callback code ...

    pthread_mutex_lock(&g_callbacks.mutex);
    g_callbacks.active_count--;
    if (g_callbacks.active_count == 0) {
        pthread_cond_signal(&g_callbacks.all_done);
    }
    pthread_mutex_unlock(&g_callbacks.mutex);
}

// httpd.c - wait for callbacks
ik_log_info("Shutting down server...");
ulfius_stop_framework(&instance);

pthread_mutex_lock(&g_callbacks.mutex);
while (g_callbacks.active_count > 0) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 10; // 10 second timeout

    int ret = pthread_cond_timedwait(&g_callbacks.all_done,
                                     &g_callbacks.mutex,
                                     &timeout);
    if (ret == ETIMEDOUT) {
        ik_log_warn("Timeout waiting for %d active callbacks",
                    g_callbacks.active_count);
        break;
    }
}
pthread_mutex_unlock(&g_callbacks.mutex);

ulfius_clean_instance(&instance);
```

---

### Scenario C: libulfius cancels callbacks forcefully

**Observations:**
- Callbacks are terminated mid-execution (pthread_cancel)
- Destructors might not run
- Memory leaks possible

**Decision:**
- ❌ Current design is UNSAFE
- 🔧 Must ensure cleanup happens even if callback is cancelled

**Implementation changes:**

```c
// Add pthread cleanup handlers
void cleanup_connection_state(void *arg) {
    ik_handler_ws_conn_t *conn = arg;
    if (conn && conn->ctx) {
        talloc_free(conn->ctx);
    }
}

void ik_handler_websocket_message_callback(...) {
    ik_handler_ws_conn_t *conn = user_data;

    pthread_cleanup_push(cleanup_connection_state, conn);

    // ... existing callback code ...

    pthread_cleanup_pop(0); // Don't execute, normal path
}
```

---

## Verification Checklist

After running all tests, verify:

- [ ] `ulfius_stop_framework()` behavior is documented with evidence
- [ ] Shutdown time is measured (must be < 10 seconds)
- [ ] Memory leaks checked with valgrind (zero leaks)
- [ ] Use-after-free checked with AddressSanitizer (zero errors)
- [ ] Client disconnect during callback is safe
- [ ] Multiple concurrent callbacks are handled correctly
- [ ] Decision made on which scenario (A/B/C) matches reality
- [ ] Implementation updated based on decision
- [ ] Integration test added to CI that verifies shutdown behavior

---

## Documentation Updates

After verification, update these documents:

1. **docs/phase-1-details.md** - Add empirical findings about libulfius behavior
2. **docs/decisions.md** - Record ADR for shutdown mechanism design
3. **AGENT.md** - Add any new testing requirements

---

## Timeline

- **Week 1**: Implement and run Phase 1 tests (minimal reproduction)
- **Week 2**: Analyze results, implement necessary changes
- **Week 3**: Implement Phase 2 tests (with OpenAI mock)
- **Week 4**: Final integration testing and documentation

---

## Success Criteria

This verification is complete when ALL of the following are true:

### Empirical Evidence Gathered

- [ ] All 4 test cases from Phase 1 have been executed
- [ ] Tests run successfully with valgrind (zero errors, zero leaks)
- [ ] Tests run successfully with AddressSanitizer (zero errors)
- [ ] Timing measurements recorded for `ulfius_stop_framework()` duration
- [ ] Behavior documented: which scenario (A/B/C) matches reality

### Decision Made and Implemented

- [ ] Scenario determination: We know definitively which behavior libulfius exhibits
- [ ] Decision matrix consulted: Implementation changes identified (if any)
- [ ] Code changes implemented based on decision (if required)
- [ ] New synchronization code tested (if Scenario B or C)
- [ ] New cleanup handlers tested (if Scenario C)

### Documentation Updated

- [ ] `docs/phase-1-details.md` updated with empirical findings
  - Remove language like "has been verified" without citations
  - Add references to test results
  - Remove or update assumptions based on verified behavior
- [ ] `docs/decisions.md` updated with ADR for shutdown mechanism
  - Document which scenario we observed
  - Justify implementation approach based on evidence
  - Reference test files for verification
- [ ] `docs/shutdown-verification.md` updated with actual test results
  - Add "Results" section showing what we found
  - Include log excerpts, timing data, valgrind output

### Tests Added to Codebase

- [ ] Shutdown test added to `tests/integration/` directory
- [ ] Test integrated into CI pipeline (`make check-integration`)
- [ ] Test documentation added (README in tests/integration/)
- [ ] Test runs reliably (no flaky failures)

### Validation

- [ ] Another developer/agent can read the updated docs and understand the shutdown behavior
- [ ] The test can be run by someone else and produce the same results
- [ ] No remaining "assumed" or "should" language around shutdown behavior in specs

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

## Next Steps After Verification

Once this test is complete:

1. **If Scenario A** (waits): Proceed with Phase 1 implementation as documented
2. **If Scenario B** (returns): Implement callback tracking, update docs, then proceed
3. **If Scenario C** (cancels): Implement cleanup handlers, update docs, then proceed

In all cases: **Update phase-1-details.md before implementing any other modules.**
