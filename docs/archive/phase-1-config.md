# Config Module (`config.c/h`)

[← Back to Phase 1 Details](phase-1-details.md)

**Note**: Configuration is loaded once at startup. Changes require server restart (see decisions.md: "Why No Config Hot-Reload?").

## API

```c
typedef struct {
  char *openai_api_key;
  char *listen_address;
  int listen_port;
} ik_cfg_t;

ik_result_t ik_cfg_load(TALLOC_CTX *ctx, const char *path);
```

## Path Expansion

Use `getenv("HOME")` for tilde expansion:

```c
static char *expand_tilde(TALLOC_CTX *ctx, const char *path) {
    if (path[0] != '~') {
        return talloc_strdup(ctx, path);
    }

    const char *home = getenv("HOME");
    if (!home) {
        return NULL;  // Caller returns ERR(ctx, IO, "HOME environment variable not set")
    }

    return talloc_asprintf(ctx, "%s%s", home, path + 1);
}
```

**Error if HOME is not set** - return `IK_ERR_IO` with message "HOME environment variable not set"

## Auto-Creation Behavior

If `~/.ikigai/config.json` does not exist:

1. Create `~/.ikigai/` directory (mode 0755)
2. Create config file with default content:

```json
{
  "openai_api_key": "YOUR_API_KEY_HERE",
  "listen_address": "127.0.0.1",
  "listen_port": 1984
}
```

3. Continue loading the newly created file
4. **Do not validate api_key content** - let OpenAI API handle authentication errors naturally

## Validation Rules

- JSON structure is valid (return `IK_ERR_PARSE` on parse failure)
- Required fields exist: `openai_api_key`, `listen_address`, `listen_port`
- Field types: `openai_api_key` is string, `listen_address` is string, `listen_port` is number
- Port range: 1024-65535 (non-privileged ports only)
- **Do NOT check if api_key is placeholder** - server will start successfully, OpenAI will return auth error on first request

## Memory Management

- `ik_cfg_t` and all strings allocated on provided `ctx`
- Use libjansson's default allocator (malloc/free)
- Extract values with `talloc_strdup()` / `talloc_asprintf()`
- Call `json_decref()` when done with jansson objects
- Caller cleans up with `talloc_free(ctx)` - no `cfg_free()` function

## Test Coverage

`tests/unit/config_test.c`:
- Load valid config file
- Auto-create missing directory
- Auto-create missing config file
- Parse error on invalid JSON
- Error on missing fields
- Error on wrong field types
- Port validation (too low, too high, valid)
- HOME not set error
- Tilde expansion works correctly
