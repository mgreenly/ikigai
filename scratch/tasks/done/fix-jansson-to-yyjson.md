# Task: Fix jansson usage - migrate to yyjson

**Priority:** CRITICAL - Must run before other test tasks
**Model:** sonnet
**Thinking:** thinking
**Estimated effort:** 15 minutes

## Context

During task execution, `tests-anthropic-basic.md` incorrectly created test files using the **jansson** JSON library instead of **yyjson** which is the standard JSON library used throughout the codebase (52 files).

Subsequently, another task added `-ljansson` to `CLIENT_LIBS` in the Makefile to "fix" the linking issue, but this is wrong - we should use yyjson consistently.

## Problem

**File with incorrect dependency:**
- `tests/unit/providers/anthropic/anthropic_client_test.c` - uses `#include <jansson.h>` and jansson API

**Incorrect Makefile change:**
- `CLIENT_LIBS` in Makefile includes `-ljansson` (should be removed)

**Codebase standard:**
- All 52 files in `src/` use `yyjson.h` and yyjson API
- No files in `src/` use jansson

## Task

Convert the Anthropic client test to use yyjson and remove the jansson dependency.

### Step 1: Update anthropic_client_test.c

File: `/home/ai4mgreenly/projects/ikigai/rel-07/tests/unit/providers/anthropic/anthropic_client_test.c`

Replace jansson includes and API calls with yyjson equivalents:

**Old (jansson):**
```c
#include <jansson.h>
...
json_error_t error;
json_t *root = json_loads(json, 0, &error);
json_t *model = json_object_get(root, "model");
ck_assert_str_eq(json_string_value(model), "claude-sonnet-4-5-20250929");
json_t *messages = json_object_get(root, "messages");
ck_assert(json_is_array(messages));
json_decref(root);
```

**New (yyjson):**
```c
#include "yyjson.h"
...
yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
ck_assert_ptr_nonnull(doc);
yyjson_val *root = yyjson_doc_get_root(doc);
yyjson_val *model = yyjson_obj_get(root, "model");
ck_assert_str_eq(yyjson_get_str(model), "claude-sonnet-4-5-20250929");
yyjson_val *messages = yyjson_obj_get(root, "messages");
ck_assert(yyjson_is_arr(messages));
yyjson_doc_free(doc);
```

**Key API mappings:**
- `json_loads(str, 0, &err)` → `yyjson_read(str, strlen(str), 0)`
- `json_object_get(obj, key)` → `yyjson_obj_get(obj, key)`
- `json_string_value(val)` → `yyjson_get_str(val)`
- `json_integer_value(val)` → `yyjson_get_int(val)`
- `json_is_array(val)` → `yyjson_is_arr(val)`
- `json_is_object(val)` → `yyjson_is_obj(val)`
- `json_is_null(val)` → `yyjson_is_null(val)`
- `json_decref(val)` → `yyjson_doc_free(doc)` (only for doc, not individual values)

### Step 2: Update Makefile

File: `/home/ai4mgreenly/projects/ikigai/rel-07/Makefile`

Remove `-ljansson` from CLIENT_LIBS:

**Old:**
```makefile
CLIENT_LIBS ?= -ltalloc -luuid -lb64 -lpthread -lutf8proc -lcurl -lpq -lxkbcommon -ljansson
```

**New:**
```makefile
CLIENT_LIBS ?= -ltalloc -luuid -lb64 -lpthread -lutf8proc -lcurl -lpq -lxkbcommon
```

### Step 3: Verify no other jansson usage

Run verification to ensure no other files use jansson:

```bash
# Should return 0 matches in src/ and tests/
grep -r "jansson\.h" src/ tests/ 2>/dev/null | grep -v "\.o:" | wc -l

# Should return 0
grep -r "json_loads\|json_object_get\|json_decref" tests/ | grep -v "\.o:" | wc -l
```

### Step 4: Verify compilation

Ensure the test compiles and runs:

```bash
make clean
make tests/unit/providers/anthropic/anthropic_client_test
./tests/unit/providers/anthropic/anthropic_client_test
```

### Step 5: Run full test suite

```bash
make check
```

Note: There may be pre-existing test failures unrelated to this change.

## Postconditions

- [ ] `tests/unit/providers/anthropic/anthropic_client_test.c` uses yyjson API only
- [ ] No `#include <jansson.h>` in any test or source file
- [ ] Makefile does not include `-ljansson` in CLIENT_LIBS
- [ ] Anthropic client test compiles without errors
- [ ] Anthropic client test passes all assertions
- [ ] No regressions in other tests
- [ ] Changes committed to git with message: `fix: Replace jansson with yyjson in Anthropic tests`
- [ ] Clean worktree (`git status --porcelain` returns empty)

## Skills

No additional skills needed - this is a straightforward API migration.

## Dependencies

None - this should be the FIRST task to run.

## Notes

- yyjson is faster and has a cleaner API than jansson
- yyjson is already included in the build (no new dependencies)
- This maintains consistency with the rest of the codebase
- The task that added jansson was attempting to fix a symptom rather than the root cause
