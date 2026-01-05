# Testing Strategies for External Tool System

Research on how to test subprocess spawning, async I/O, and external tool execution in C.

## Challenge Statement

Testing external tool systems involves:
1. **Process spawning** - Hard to mock/fake `fork`, `exec`, `posix_spawn`
2. **Async I/O** - Event loops with `select()`, timing-dependent behavior
3. **External dependencies** - Tools must exist and be executable
4. **Error conditions** - Timeouts, crashes, invalid JSON
5. **Integration complexity** - Multiple subsystems interacting

**Key insight:** C lacks mature subprocess mocking frameworks (unlike Python's `pytest-subprocess`).

## Testing Pyramid Strategy

```
         /\
        /  \        E2E Tests (few)
       /----\       - Full tool execution
      /      \      - Real processes
     /--------\     Integration Tests (some)
    /          \    - Component interaction
   /------------\   - Fake tools
  /______________\  Unit Tests (many)
                    - Logic only
                    - Wrapper seams
```

## Unit Testing Strategy

### Principle: Test Logic, Not System Calls

**Don't test:**
- ✗ That `posix_spawn()` works (OS tests that)
- ✗ That pipes transfer data (kernel tests that)
- ✗ That `select()` multiplexes I/O (POSIX tests that)

**Do test:**
- ✓ Schema parsing logic
- ✓ Parameter validation
- ✓ Error envelope construction
- ✓ Registry management (add, lookup, iterate)
- ✓ JSON manipulation
- ✓ Response wrapping

### Wrapper Pattern for Testability

**Problem:** Can't mock system calls directly in C.

**Solution:** Wrapper layer (ikigai already has `src/wrapper.c`!)

**Pattern:**
```c
// wrapper.h - Functions that wrap system calls
typedef struct process_t process_t;

process_t *ik_process_spawn(const char *path,
                             char *const argv[],
                             int *stdin_fd,
                             int *stdout_fd,
                             int *stderr_fd);

int ik_process_wait(process_t *proc, int *exit_code);
void ik_process_kill(process_t *proc, int signal);

// For testing, use function pointers
struct process_ops {
    process_t *(*spawn)(const char *, char *const[], int*, int*, int*);
    int (*wait)(process_t *, int*);
    void (*kill)(process_t *, int);
};

extern struct process_ops *process_ops;  // Swappable in tests
```

**Production implementation:**
```c
// wrapper.c
static process_t *real_spawn(...) {
    // Real posix_spawn implementation
}

static int real_wait(...) {
    // Real waitpid implementation
}

static struct process_ops real_ops = {
    .spawn = real_spawn,
    .wait = real_wait,
    .kill = real_kill,
};

struct process_ops *process_ops = &real_ops;  // Default
```

**Test implementation:**
```c
// tests/unit/tool/fake_process.c
static process_t *fake_spawn(const char *path,
                              char *const argv[],
                              int *stdin_fd,
                              int *stdout_fd,
                              int *stderr_fd) {
    // Create fake process
    // Return pre-canned stdout via pipe
    // Don't actually spawn anything
}

static struct process_ops fake_ops = {
    .spawn = fake_spawn,
    .wait = fake_wait,
    .kill = fake_kill,
};

void setup_fake_processes() {
    process_ops = &fake_ops;
}

void teardown_fake_processes() {
    process_ops = &real_ops;
}
```

**Unit test:**
```c
void test_tool_registry_discovery(void **state) {
    setup_fake_processes();

    // Configure fake to return specific schema
    fake_process_set_output("bash", "{\"name\": \"bash\", ...}");

    // Run discovery (uses fake process_ops)
    tool_registry_t *registry = tool_registry_create(NULL);
    res_t result = tool_registry_scan(registry, "/fake/path");

    assert_true(is_ok(&result));

    tool_entry_t *entry = tool_registry_lookup(registry, "bash");
    assert_non_null(entry);
    assert_string_equal(entry->name, "bash");

    talloc_free(registry);
    teardown_fake_processes();
}
```

### Test Cases for Unit Testing

**Schema Parsing:**
```c
test_parse_valid_schema();
test_parse_invalid_json();
test_parse_missing_name();
test_parse_missing_description();
test_parse_missing_parameters();
test_parse_nested_parameter_types();
```

**Parameter Validation:**
```c
test_validate_all_required_present();
test_validate_missing_required_param();
test_validate_string_type();
test_validate_integer_type();
test_validate_boolean_type();
test_validate_extra_params_allowed();
```

**Response Wrapping:**
```c
test_wrap_successful_execution();
test_wrap_tool_timeout();
test_wrap_tool_crash();
test_wrap_invalid_json_output();
test_wrap_missing_result_fields();
```

**Registry Operations:**
```c
test_registry_create();
test_registry_add_entry();
test_registry_lookup_exists();
test_registry_lookup_not_found();
test_registry_user_overrides_system();
test_registry_build_tools_array();
```

## Integration Testing Strategy

### Principle: Test Component Interaction with Fakes

**Don't test:**
- ✗ Full system end-to-end (save for E2E)

**Do test:**
- ✓ Discovery → Registry pipeline
- ✓ Execution → Wrapper → Response
- ✓ Timeout behavior
- ✓ Error propagation

### Fake Tools for Testing

**Create minimal test tools:**

**tests/fixtures/tools/success**
```bash
#!/bin/bash
# Tool that succeeds immediately

if [ "$1" = "--schema" ]; then
  cat <<'EOF'
{
  "name": "success",
  "description": "Always succeeds",
  "parameters": {},
  "returns": {"result": {"type": "string"}}
}
EOF
else
  echo '{"result": "success"}'
fi
```

**tests/fixtures/tools/timeout**
```bash
#!/bin/bash
# Tool that never returns

if [ "$1" = "--schema" ]; then
  cat <<'EOF'
{
  "name": "timeout",
  "description": "Never finishes",
  "parameters": {},
  "returns": {}
}
EOF
else
  sleep 999999
fi
```

**tests/fixtures/tools/crash**
```bash
#!/bin/bash
# Tool that crashes

if [ "$1" = "--schema" ]; then
  cat <<'EOF'
{
  "name": "crash",
  "description": "Exits with error",
  "parameters": {},
  "returns": {}
}
EOF
else
  exit 139  # Simulate segfault
fi
```

**tests/fixtures/tools/bad-json**
```bash
#!/bin/bash
# Tool that returns invalid JSON

if [ "$1" = "--schema" ]; then
  cat <<'EOF'
{
  "name": "bad_json",
  "description": "Returns invalid JSON",
  "parameters": {},
  "returns": {}
}
EOF
else
  echo 'this is not json'
fi
```

### Integration Test Cases

**Discovery Integration:**
```c
void test_discovery_scan_directory(void **state) {
    tool_registry_t *registry = tool_registry_create(NULL);

    // Scan tests/fixtures/tools/ (real tools)
    res_t result = tool_registry_scan(registry, "tests/fixtures/tools");

    assert_true(is_ok(&result));

    // Verify expected tools found
    assert_non_null(tool_registry_lookup(registry, "success"));
    assert_non_null(tool_registry_lookup(registry, "timeout"));
    assert_non_null(tool_registry_lookup(registry, "crash"));

    talloc_free(registry);
}
```

**Execution Integration:**
```c
void test_execute_tool_success(void **state) {
    tool_entry_t *entry = create_test_tool_entry("tests/fixtures/tools/success");

    char *arguments = "{\"input\": \"test\"}";
    res_t result = tool_execute(NULL, entry, arguments);

    assert_true(is_ok(&result));

    // Parse wrapped response
    yyjson_doc *doc = yyjson_read(result.ok, strlen(result.ok), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    assert_true(yyjson_obj_get_bool(root, "tool_success"));

    yyjson_doc_free(doc);
    talloc_free(entry);
}
```

**Timeout Integration:**
```c
void test_execute_tool_timeout(void **state) {
    tool_entry_t *entry = create_test_tool_entry("tests/fixtures/tools/timeout");

    // Set short timeout (1 second)
    res_t result = tool_execute_with_timeout(NULL, entry, "{}", 1);

    assert_true(is_ok(&result));

    yyjson_doc *doc = yyjson_read(result.ok, strlen(result.ok), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Should have tool_success: false
    assert_false(yyjson_obj_get_bool(root, "tool_success"));

    // Should have error_code: TOOL_TIMEOUT
    const char *error_code = yyjson_get_str(yyjson_obj_get(root, "error_code"));
    assert_string_equal(error_code, "TOOL_TIMEOUT");

    yyjson_doc_free(doc);
    talloc_free(entry);
}
```

**Crash Handling Integration:**
```c
void test_execute_tool_crash(void **state) {
    tool_entry_t *entry = create_test_tool_entry("tests/fixtures/tools/crash");

    res_t result = tool_execute(NULL, entry, "{}");

    assert_true(is_ok(&result));

    yyjson_doc *doc = yyjson_read(result.ok, strlen(result.ok), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    assert_false(yyjson_obj_get_bool(root, "tool_success"));

    const char *error_code = yyjson_get_str(yyjson_obj_get(root, "error_code"));
    assert_string_equal(error_code, "TOOL_CRASHED");

    yyjson_doc_free(doc);
    talloc_free(entry);
}
```

**Invalid JSON Integration:**
```c
void test_execute_tool_invalid_json(void **state) {
    tool_entry_t *entry = create_test_tool_entry("tests/fixtures/tools/bad-json");

    res_t result = tool_execute(NULL, entry, "{}");

    assert_true(is_ok(&result));

    yyjson_doc *doc = yyjson_read(result.ok, strlen(result.ok), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    assert_false(yyjson_obj_get_bool(root, "tool_success"));

    const char *error_code = yyjson_get_str(yyjson_obj_get(root, "error_code"));
    assert_string_equal(error_code, "INVALID_OUTPUT");

    yyjson_doc_free(doc);
    talloc_free(entry);
}
```

## End-to-End Testing Strategy

### Principle: Test Real System Behavior

**E2E Test Scope:**
- Full ikigai REPL running
- Real tool executables
- Actual LLM API calls (or mocked HTTP)
- Complete conversation flow

**Example E2E Test:**

```c
void test_e2e_tool_discovery_and_usage(void **state) {
    // 1. Start ikigai with test tools directory
    repl_ctx_t *repl = repl_create_for_testing(NULL);
    repl->tool_system_dir = "tests/fixtures/tools";
    repl->tool_user_dir = NULL;

    // 2. Initialize (triggers discovery)
    res_t init_result = repl_init(repl);
    assert_true(is_ok(&init_result));

    // 3. Wait for discovery to complete
    while (repl->tool_scan_state == TOOL_SCAN_IN_PROGRESS) {
        repl_event_loop_iteration(repl);
    }

    assert_int_equal(repl->tool_scan_state, TOOL_SCAN_COMPLETE);

    // 4. Verify tools were discovered
    tool_entry_t *success_tool = tool_registry_lookup(repl->tool_registry, "success");
    assert_non_null(success_tool);

    // 5. Simulate LLM tool call
    const char *tool_call_json =
        "{\"name\": \"success\", \"arguments\": {\"input\": \"test\"}}";

    // 6. Execute tool (as REPL would)
    res_t exec_result = repl_execute_tool_call(repl, tool_call_json);
    assert_true(is_ok(&exec_result));

    // 7. Verify result format
    yyjson_doc *doc = yyjson_read(exec_result.ok, strlen(exec_result.ok), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    assert_true(yyjson_obj_get_bool(root, "tool_success"));

    yyjson_doc_free(doc);
    repl_destroy(repl);
}
```

### E2E Test Cases

**Discovery E2E:**
```c
test_e2e_startup_discovers_system_tools();
test_e2e_user_tools_override_system();
test_e2e_failed_tools_logged_to_scrollback();
test_e2e_refresh_command_rescans();
```

**Execution E2E:**
```c
test_e2e_tool_call_successful_execution();
test_e2e_tool_call_with_timeout();
test_e2e_tool_call_with_crash();
test_e2e_tool_call_invalid_parameters();
```

**Integration with LLM E2E:**
```c
test_e2e_llm_calls_tool_and_receives_result();
test_e2e_llm_handles_tool_error();
test_e2e_multiple_tool_calls_in_conversation();
```

## C Testing Frameworks

### CMocka

**Features:**
- Mock object support
- Test fixtures (setup/teardown)
- Memory leak detection
- Assert macros
- Actively maintained

**Example:**
```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void test_tool_registry_lookup(void **state) {
    tool_registry_t *registry = tool_registry_create(NULL);
    tool_entry_t *entry = create_test_entry("bash", "/bin/bash");

    tool_registry_add(registry, entry);

    tool_entry_t *found = tool_registry_lookup(registry, "bash");
    assert_ptr_equal(found, entry);

    tool_entry_t *not_found = tool_registry_lookup(registry, "nonexistent");
    assert_null(not_found);

    talloc_free(registry);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_tool_registry_lookup),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

**Build integration:**
```makefile
TEST_REGISTRY_SOURCES = tests/unit/tool/registry_test.c \
                        src/tool_registry.c \
                        src/error.c

TEST_REGISTRY_OBJ = $(patsubst %.c,$(BUILDDIR)/%.o,$(TEST_REGISTRY_SOURCES))

tests/unit/tool/registry_test: $(TEST_REGISTRY_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(TEST_LIBS) -lcmocka

check: tests/unit/tool/registry_test
	./tests/unit/tool/registry_test
```

### Unity + CMock

**Unity:** Unit testing framework (lightweight)
**CMock:** Mock generator for Unity

**Features:**
- Very lightweight (single .c + .h)
- Embedded-friendly
- CMock generates mocks from headers
- Good for isolated unit tests

**Example:**
```c
#include "unity.h"

void setUp(void) {
    // Runs before each test
}

void tearDown(void) {
    // Runs after each test
}

void test_schema_parse_valid(void) {
    const char *json = "{\"name\": \"test\", \"description\": \"...\"}";

    tool_schema_t *schema = parse_tool_schema(json);

    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_EQUAL_STRING("test", schema->name);

    free_tool_schema(schema);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_schema_parse_valid);
    return UNITY_END();
}
```

### Recommendation for ikigai

**Use CMocka:**
- ✓ Already familiar (used in other tests?)
- ✓ Mock support (helpful for complex integration)
- ✓ Memory leak detection (critical with talloc)
- ✓ Standard test/setup/teardown pattern
- ✓ Good documentation

## Test Organization

### Directory Structure

```
tests/
├── unit/
│   ├── tool/
│   │   ├── registry_test.c         # Registry add/lookup/iterate
│   │   ├── schema_parse_test.c     # Schema JSON parsing
│   │   ├── param_validate_test.c   # Parameter validation
│   │   ├── response_wrap_test.c    # Response envelope wrapping
│   │   └── fake_process.c          # Fake process ops for mocking
│   └── ...
├── integration/
│   ├── tool/
│   │   ├── discovery_test.c        # Full discovery pipeline
│   │   ├── execution_test.c        # Tool spawning + result
│   │   ├── timeout_test.c          # Timeout behavior
│   │   └── error_handling_test.c   # Crash, invalid JSON, etc.
│   └── ...
├── e2e/
│   ├── tool_usage_test.c           # Full REPL + tools
│   ├── llm_integration_test.c      # With mocked HTTP
│   └── ...
└── fixtures/
    ├── tools/
    │   ├── success                 # Fake tool: succeeds
    │   ├── timeout                 # Fake tool: never returns
    │   ├── crash                   # Fake tool: exits with error
    │   ├── bad-json                # Fake tool: invalid JSON
    │   └── complex-params          # Fake tool: nested params
    └── schemas/
        ├── valid.json              # Valid schema examples
        └── invalid.json            # Invalid schema examples
```

### Makefile Integration

```makefile
# Unit tests (many, fast)
UNIT_TESTS = tests/unit/tool/registry_test \
             tests/unit/tool/schema_parse_test \
             tests/unit/tool/param_validate_test \
             tests/unit/tool/response_wrap_test

# Integration tests (some, medium)
INTEGRATION_TESTS = tests/integration/tool/discovery_test \
                    tests/integration/tool/execution_test \
                    tests/integration/tool/timeout_test

# E2E tests (few, slow)
E2E_TESTS = tests/e2e/tool_usage_test

ALL_TESTS = $(UNIT_TESTS) $(INTEGRATION_TESTS) $(E2E_TESTS)

# Run all tests
check: $(ALL_TESTS)
	@echo "Running unit tests..."
	@for test in $(UNIT_TESTS); do $$test || exit 1; done
	@echo "Running integration tests..."
	@for test in $(INTEGRATION_TESTS); do $$test || exit 1; done
	@echo "Running E2E tests..."
	@for test in $(E2E_TESTS); do $$test || exit 1; done
	@echo "All tests passed!"

# Quick checks (unit + integration, skip E2E)
check-fast: $(UNIT_TESTS) $(INTEGRATION_TESTS)
	@for test in $(UNIT_TESTS) $(INTEGRATION_TESTS); do $$test || exit 1; done
```

## Coverage Goals

**100% Line Coverage Requirement:**

ikigai requires 100% line coverage. Every branch, every error path.

**Strategy:**
1. **Unit tests** - Cover all logic paths
2. **Integration tests** - Cover error conditions (timeout, crash)
3. **Fake fixtures** - Trigger every error code path
4. **Coverage report** - `make coverage` validates 100%

**Makefile coverage target:**
```makefile
coverage: CFLAGS += --coverage
coverage: LDFLAGS += --coverage
coverage: clean check
	@lcov --capture --directory $(BUILDDIR) --output-file coverage.info
	@lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
	@lcov --list coverage.info
	@genhtml coverage.info --output-directory coverage-report
	@echo "Coverage report: coverage-report/index.html"
	@lcov --summary coverage.info | grep "lines" | grep -q "100.0%"
```

**Ensuring 100%:**
- Test every error return (`ERR()` paths)
- Test every branch (if/else, switch cases)
- Test boundary conditions (empty, full, null)
- Test cleanup (destructors called, fds closed)

## Continuous Integration

**GitHub Actions example:**
```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libcmocka-dev lcov

      - name: Build
        run: make

      - name: Unit tests
        run: make check-fast

      - name: E2E tests
        run: make check

      - name: Coverage
        run: make coverage

      - name: Upload coverage
        uses: codecov/codecov-action@v2
        with:
          files: ./coverage.info
```

## Summary

| Test Level | Purpose | Tools | Count |
|------------|---------|-------|-------|
| **Unit** | Test logic in isolation | CMocka, fake process ops | Many (20-30) |
| **Integration** | Test component interaction | CMocka, fake tool scripts | Some (10-15) |
| **E2E** | Test full system | Real REPL, mocked HTTP | Few (5-10) |

**Key Strategies:**
1. **Wrapper pattern** - Make system calls swappable for testing
2. **Fake tools** - Shell scripts that simulate behaviors
3. **CMocka framework** - Standard C unit testing
4. **100% coverage** - No untested paths
5. **Fast feedback** - Unit tests run in milliseconds

**Validation:** Testing approach is feasible and follows C best practices.

## Sources

- [CMocka - Unit Testing Framework for C](https://cmocka.org/)
- [Unity - Simple Unit Testing for C](https://github.com/ThrowTheSwitch/Unity)
- [CMock - Mock/Stub Generator](https://github.com/ThrowTheSwitch/CMock)
- [Testing with CMocka (DSE Projects)](https://boschglobal.github.io/dse.doc/docs/devel/testing/cmocka/)
- [Embedded C/C++ Unit Testing with Mocks](https://interrupt.memfault.com/blog/unit-test-mocking)
- [testfixtures (Python subprocess mocking - for comparison)](https://testfixtures.readthedocs.io/en/latest/popen.html)
- [pytest-subprocess (Python - for patterns)](https://pypi.org/project/pytest-subprocess/)
