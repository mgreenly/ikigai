# Handler Module (`handler.c/h`)

[← Back to Phase 1 Details](phase-1-details.md)

Manages WebSocket connection lifecycle, handshake protocol, and dispatches tasks to worker thread for processing. Uses libulfius for WebSocket handling and pthread for worker coordination.

## Threading Architecture

**Two threads per connection:**

1. **WebSocket thread** (libulfius-managed):
   - Handles WebSocket protocol (framing, ping/pong)
   - Parses message envelopes
   - Manages handshake protocol
   - Enqueues tasks to worker
   - Blocks waiting for task completion
   - Sends responses back to client

2. **Worker thread** (connection-managed):
   - Processes tasks from queue (OpenAI requests, etc.)
   - Checks abort flag during processing
   - Signals completion when done
   - Can be immediately aborted on disconnect/shutdown

**Coordination:** pthread mutex/condition variables for queue synchronization and completion signaling.

**Verified behavior:** libulfius creates one dedicated thread per WebSocket connection. All callbacks execute sequentially on that thread. WebSocket thread cannot block during task processing because it delegates to worker thread.

**Multi-connection concurrency:** Different WebSocket connections run on different threads with different workers, so the server handles multiple clients concurrently.

## Connection State

```c
typedef struct {
  TALLOC_CTX *ctx;                    // Connection's talloc context (parent for all allocations)
  struct _u_websocket *websocket;     // libulfius WebSocket handle (borrowed, owned by libulfius)
  char *sess_id;                      // 22-char base64url UUID (allocated on ctx)
  ik_cfg_t *cfg_ref;                  // Server config (borrowed, owned by server_main)
  bool handshake_complete;            // true after successful hello/welcome exchange

  // Worker thread and synchronization
  pthread_t worker_thread;            // Worker thread handle
  ik_task_queue_t queue;              // Task queue (single slot for Phase 1)
  volatile sig_atomic_t abort_flag;   // Set by close callback, checked by worker
  bool closed;                        // Connection closed flag
} ik_handler_ws_conn_t;

typedef struct {
  TALLOC_CTX *ctx;                    // Task's talloc context for allocations
  char *type;                         // Task type: "user_query", etc. (allocated on task->ctx)
  char *sess_id;                      // Session ID (borrowed from connection - conn outlives task)
  char *corr_id;                      // Correlation ID (owned by task, allocated on task->ctx, unique per request)
  json_t *payload;                    // Request payload (owned by task, incref'd from message)
  struct _u_websocket *websocket;     // WebSocket handle (borrowed, owned by libulfius)
  volatile sig_atomic_t *abort_flag;  // Points to conn->abort_flag
  ik_cfg_t *cfg_ref;                  // Server config (borrowed)
} ik_task_t;

typedef struct {
  ik_task_t *pending_task;            // Single task slot (Phase 1: one request at a time)
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool has_task;
  bool closed;
} ik_task_queue_t;
```

**Lifecycle:**
1. Client connects → libulfius calls `ik_handler_websocket_manager` → allocate `ik_handler_ws_conn_t`
2. Initialize queue and spawn worker thread
3. Handshake (`hello` → `welcome`) → set `handshake_complete = true`
4. Message exchange (`user_query` → `assistant_response` chunks) - **FIRE AND FORGET**
   - WebSocket thread: parse message, create task (with copies of all data), enqueue, **RETURN IMMEDIATELY**
   - Worker thread: dequeue task, call `ik_openai_stream_request()`, send responses directly to client
   - OpenAI streams chunks → worker sends each chunk via `ulfius_websocket_send_message()`
   - Worker completes or aborts → frees task context, loops back to wait for next task
5. Client disconnects or error → libulfius calls close callback:
   - Set `abort_flag = 1` and `closed = true`
   - Signal queue to wake worker
   - `pthread_join(worker_thread)` (wait for worker to exit cleanly)
   - `talloc_free(conn->ctx)`

**Memory management:**
- Connection context created with `talloc_new(NULL)` (top-level context for connection state)
- All connection data allocated on `conn->ctx` (including sess_id)
- **Each task gets its own context**: `talloc_new(NULL)` for task allocations
- Task owns: type (copied), corr_id (generated), payload (incref'd)
- Task borrows: sess_id (from conn), websocket (from conn), abort_flag (from conn), cfg_ref (from conn)
- Worker frees task context when done: `talloc_free(task->ctx)`
- Connection cleanup: `talloc_free(conn->ctx)` in close callback after joining worker

**Thread safety:**
- `abort_flag` is `sig_atomic_t` (safe for worker to read, close callback to write)
- Queue operations protected by mutex
- **No completion signaling needed** - fire and forget pattern
- `ulfius_websocket_send_message()` called from worker thread (safe due to libulfius internal locking)
- Task-owned data (type, corr_id, payload) not shared after enqueue
- Borrowed pointers (sess_id, websocket, abort_flag, cfg_ref) safe because connection outlives task (pthread_join guarantees this)

## API

```c
// Main WebSocket callback - registered with libulfius
void ik_handler_websocket_manager(const struct _u_request *request,
                                   struct _u_websocket_manager *websocket_manager,
                                   void *user_data);

// Message handler - called for each incoming WebSocket message
static void handler_message_callback(struct _u_websocket_manager *websocket_manager,
                                     const struct _u_websocket_message *message,
                                     void *user_data);

// Close handler - called when connection closes
static void handler_close_callback(struct _u_websocket_manager *websocket_manager,
                                   void *user_data);

// Worker thread entry point
static void* worker_thread_fn(void *arg);

// Stream callback for OpenAI responses (called from worker thread)
static void openai_stream_callback(const char *json_chunk, void *user_data);

// Task queue operations (simplified - no completion signaling)
static void task_queue_init(ik_task_queue_t *queue);
static void task_queue_push(ik_task_queue_t *queue, ik_task_t *task);  // Takes ownership of task
static ik_task_t* task_queue_pop(ik_task_queue_t *queue);  // Blocks until task available or closed
static void task_queue_close(ik_task_queue_t *queue);  // Wakes worker to exit
static void task_queue_destroy(ik_task_queue_t *queue);  // Cleanup mutex/cond
```

## Handshake Protocol

**Client → Server (hello):**
```json
{
  "type": "hello",
  "identity": "hostname@username"
}
```

**Server → Client (welcome):**
```json
{
  "type": "welcome",
  "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA"
}
```

**Handshake validation:**
- Parse JSON, check for `type` field
- If `type == "hello"`, validate `identity` field exists and is a string
- Generate `sess_id` using `ik_protocol_generate_uuid()`
- Send `welcome` message
- Set `handshake_complete = true`
- **Note:** Identity value is validated but NOT stored (future phases may store it for authorization)

**Before handshake completes:**
- Reject any message other than `hello` with error
- Close connection after sending error

## Message Flow

See [phase-1-details.md](phase-1-details.md#message-flow) for complete message flow documentation including:
- User Query processing (fire-and-forget pattern)
- Assistant Response streaming
- Correlation ID generation and lifecycle

## Connection Cleanup

**Normal disconnect:**
- Client closes WebSocket
- libulfius calls `websocket_close_callback` immediately (not blocked by worker)
- Close callback:
  1. Set `conn->abort_flag = 1` and `conn->closed = true`
  2. Call `task_queue_close(&conn->queue)` to wake worker
  3. `pthread_join(conn->worker_thread, NULL)` - wait for worker to exit
  4. Destroy queue mutexes/conds: `task_queue_destroy(&conn->queue)`
  5. Free connection context: `talloc_free(conn->ctx)`

**Disconnect during OpenAI streaming (immediate abort):**
- Client disconnects → TCP FIN received
- libulfius calls `websocket_close_callback` immediately
- Close callback sets `conn->abort_flag = 1`
- Worker thread is running `ik_openai_stream_req()`, which uses curl multi loop
- curl multi loop checks `abort_flag` each poll iteration
- When abort detected, curl request is aborted via `curl_multi_remove_handle()`
- Worker thread returns from `ik_openai_stream_req()` with `OK(NULL)` (abort is not error)
- Worker exits its event loop (sees `conn->closed == true`)
- Close callback's `pthread_join()` returns
- Connection context freed

**Benefit:** OpenAI request stops promptly on disconnect. No wasted API calls or bandwidth.

## Correlation ID Flow

**Complete flow diagram:**

```
1. Client sends user_query (no corr_id field - server generates it)
   ↓
2. websocket_message_callback receives message
   ↓
3. Create task, generate unique corr_id for this task:
   task->corr_id = ik_protocol_generate_uuid(task_ctx)
   ↓
4. Enqueue task to worker (task owns corr_id)
   ↓
5. Worker dequeues task, calls ik_openai_stream_req(..., openai_stream_callback, task)
   ↓
6. OpenAI sends chunk → sse_write_callback → openai_stream_callback(chunk, task)
   ↓
7. openai_stream_callback accesses task fields:
   - task->sess_id → "VQ6EAOKbQdSnFkRmVUQAAA" (borrowed from conn)
   - task->corr_id → "8fKm3pLxTdOqZ1YnHjW9Gg" (owned by task)
   ↓
8. Build assistant_response envelope with sess_id + corr_id
   ↓
9. Send to client via task->websocket
   ↓
10. Repeat steps 6-9 for each chunk until [DONE]
   ↓
11. Worker frees task: talloc_free(task->ctx) (frees corr_id and all task data)
```

**Key insight:** `corr_id` is task-scoped, not connection-scoped. Each task generates its own unique corr_id, uses it for all chunks in that exchange, then frees it when the task completes. Connection's sess_id is borrowed by all tasks and lives for the entire connection lifetime.

## Memory Management

**Connection vs Task Scoping:**
- **Connection-scoped data** (lives for entire WebSocket connection):
  - `sess_id` - identifies the connection, allocated on `conn->ctx`
  - `websocket` - libulfius handle, borrowed
  - `abort_flag` - shared by all tasks on this connection
  - `cfg_ref` - server config, borrowed from main

- **Task-scoped data** (lives for one request-response exchange):
  - `corr_id` - unique per request, allocated on `task->ctx`
  - `type` - message type, allocated on `task->ctx`
  - `payload` - request JSON, owned by task via incref
  - All task data freed with `talloc_free(task->ctx)` when request completes

**Borrowed pointers (task → connection):**
- `task->sess_id` - points to `conn->sess_id` (safe: conn outlives task via pthread_join)
- `task->websocket` - points to `conn->websocket` (safe: libulfius handle valid until close)
- `task->abort_flag` - points to `&conn->abort_flag` (safe: conn outlives task)
- `task->cfg_ref` - points to `conn->cfg_ref` (safe: config lives for entire server lifetime)

## Test Coverage

`tests/unit/handler_test.c`:
- Handshake: hello → welcome exchange
- Session ID generation and validation
- Reject messages before handshake
- User query parsing and correlation ID generation
- Session ID mismatch error
- Unknown message type error
- Invalid JSON error
- Connection cleanup (verify talloc cleanup, pthread_join completes)
- Task queue operations (push, pop, close)
- Worker thread spawning and cleanup
- Abort flag handling during streaming

`tests/integration/websocket_openai_test.c`:
- Full flow: connect → handshake → query → stream response → disconnect
- Multiple queries on same connection (correlation IDs are different)
- Client disconnect during streaming (verify prompt abort)
- Worker thread processes task and signals completion correctly
- OpenAI error propagation (auth error, network error)
- Verify no resource leaks (all threads joined, all mutexes destroyed)
