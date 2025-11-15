# Naming Conventions

## Public Symbol Pattern

All public symbols (functions, types, globals) follow: `ik_MODULE_THING`

- `ik_` - Project namespace prefix
- `MODULE` - Module identifier (config, protocol, openai, handler, httpd, input_buffer)
- `THING` - Descriptive name using approved abbreviations

Examples:
- `ik_cfg_load()` - Function in config module
- `ik_protocol_msg_t` - Type in protocol module
- `ik_handler_ws_conn_t` - Type in handler module
- `ik_input_buffer_create()` - Function in input_buffer module
- `ik_input_buffer_cursor_move_left()` - Function in input_buffer module (cursor subcomponent)

### Module Organization

**Principle: One subdirectory = One module**

When a module is organized into a subdirectory (e.g., `src/input_buffer/`), ALL public symbols from that module use the same module prefix, regardless of which file they're defined in.

**Example: input_buffer module**
```
src/input_buffer/
  ├── core.c          → ik_input_buffer_create(), ik_input_buffer_insert()
  ├── cursor.c        → ik_input_buffer_cursor_move_left()
  ├── layout.c        → ik_input_buffer_ensure_layout()
  └── multiline.c     → ik_input_buffer_cursor_up()
```

All symbols use `ik_input_buffer_*` prefix even though they're in separate files. The subdirectory provides code organization, while the symbol prefix provides namespace in C's global symbol table.

**Rationale:**
- Clear module ownership in C's flat namespace
- Consistent with module boundaries (header files)
- Prevents naming conflicts
- Self-documenting: symbol name tells you which module it belongs to

## Approved Abbreviations

These abbreviations are universally recognized by C programmers and MUST be used consistently throughout the codebase (type names, function names, struct fields, variables).

| Full Word | Abbreviation | Notes |
|-----------|--------------|-------|
| context | `ctx` | Universal in C APIs |
| configuration | `cfg` | Standard Unix convention |
| message | `msg` | Standard across networking code |
| connection | `conn` | Standard in networking |
| request | `req` | HTTP/networking standard |
| response | `resp` | HTTP/networking standard |
| buffer | `buf` | Universal in C |
| length | `len` | Universal in C |
| string | `str` | Common in C string handling |
| pointer | `ptr` | Standard C terminology |
| temporary | `tmp` | Universal in C |
| error | `err` | Common in error handling |
| result | `res` | Common for function results |
| maximum | `max` | Mathematical standard |
| minimum | `min` | Mathematical standard |
| count | `cnt` | Common counter abbreviation |
| number | `num` | Mathematical standard |
| index | `idx` | Array/list standard |
| argument | `arg` | Standard C convention (argc/argv) |
| callback | `cb` | Standard in async/event code |

## Domain-Specific Abbreviations

These are standard in their respective domains:

| Full Word | Abbreviation | Domain |
|-----------|--------------|--------|
| WebSocket | `ws` | Networking |
| session | `sess` | Web/networking |
| correlation | `corr` | Distributed systems |
| authentication | `auth` | Security |

## Words That Must NOT Be Abbreviated

The following must always be spelled out completely:

- `handler` - Already concise
- `manager` - Abbreviations (mgr, mngr) are unclear
- `server` - Abbreviations (srv, svr) reduce clarity
- `client` - Abbreviations (cli, clnt) are ambiguous
- `function` - fn/func are unclear
- `parameter` - param loses clarity
- `shutdown` - Already clear
- `payload` - Technical term, keep precise
- `protocol` - Module name, keep clear
- `openai` - Brand name, never abbreviate
- `httpd` - Already an abbreviation (HTTP daemon)

## Brand Names and Acronyms

These are already abbreviations or must remain intact:

- `HTTP` - Never expand or abbreviate further
- `JSON` - Never expand or abbreviate further
- `UUID` - Never expand or abbreviate further
- `SSE` - Server-Sent Events, standard abbreviation
- `OpenAI` - Brand name, never abbreviate

## Usage Examples

**Type definitions**:
```c
typedef struct {
    char *sess_id;
    char *corr_id;
    char *type;
    json_t *payload;
} ik_protocol_msg_t;

typedef struct {
    TALLOC_CTX *ctx;
    struct _u_websocket *websocket;
    char *sess_id;
    char *corr_id;
    ik_cfg_t *cfg_ref;
    bool handshake_complete;
} ik_handler_ws_conn_t;
```

**Function signatures**:
```c
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path);
res_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str);
res_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                  const ik_cfg_t *cfg,
                                  json_t *req_payload,
                                  openai_stream_cb_t cb,
                                  void *cb_data,
                                  volatile sig_atomic_t *shutdown_flag);
```

**Function implementation**:
```c
void ik_handler_websocket_msg_cb(struct _u_websocket_manager *ws_mgr,
                                  const struct _u_websocket_msg *msg,
                                  void *user_data) {
    ik_handler_ws_conn_t *conn = user_data;
    TALLOC_CTX *tmp = talloc_new(conn->ctx);

    res_t res = ik_protocol_msg_parse(tmp, json_str);
    if (is_err(&res)) {
        const char *err_msg = error_message(res.err);
        // handle error
    }

    ik_protocol_msg_t *parsed_msg = res.ok;
    // ... process message ...

    talloc_free(tmp);
}
```

## Special Conventions

**Borrowed pointers** use `_ref` suffix:
```c
ik_cfg_t *cfg_ref;      // Borrowed, caller owns
ik_handler_t *handler;  // Owned by this struct
```

**Global variables** use `g_` prefix:
```c
extern volatile sig_atomic_t g_httpd_shutdown;
```

**Internal/static symbols** don't need `ik_` prefix:
```c
static void parse_handshake(const char *json) { ... }
static sse_parser_t *sse_parser_create(TALLOC_CTX *ctx) { ... }
```

## Rationale

1. **Consistency**: Same abbreviation everywhere (types, functions, fields, variables)
2. **Readability**: Universally recognized abbreviations don't require mental translation
3. **Brevity**: Shorter names reduce line wrapping and visual clutter
4. **Tradition**: Follow established C/Unix conventions that have stood the test of time
5. **High bar**: Only abbreviations that are completely unambiguous to experienced C programmers

If an abbreviation isn't on the approved list, spell the word out completely.
