# Task: Create Performance Benchmarking Test Suite

**Model:** sonnet/thinking
**Depends on:** tests-anthropic-basic.md, tests-openai-basic.md, tests-google-basic.md, tests-common-utilities.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task creates a comprehensive performance benchmarking suite to ensure the multi-provider abstraction maintains acceptable performance characteristics. The benchmarks measure critical path operations and establish baseline metrics for regression detection.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:
- Use curl_multi (NOT curl_easy)
- Benchmarks measure async operation overhead, not blocking time
- Test the fdset/perform/info_read pattern performance
- NEVER use blocking calls in benchmarks

Reference: `scratch/plan/05-testing/strategy.md`

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] Provider implementations complete (Anthropic, OpenAI, Google)
- [ ] Basic unit tests passing for all providers

## Pre-Read

**Skills:**
- `/load memory` - talloc-based memory management and ownership rules
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/anthropic/` - Anthropic provider implementation
- `src/providers/openai/` - OpenAI provider implementation
- `src/providers/google/` - Google provider implementation
- `src/providers/common/http_multi.c` - HTTP client infrastructure
- `src/providers/common/sse_parser.c` - SSE parser infrastructure
- `tests/helpers/mock_http.h` - Mock HTTP infrastructure

**Plan:**
- `scratch/plan/05-testing/strategy.md` - Testing strategy (includes performance section)
- `scratch/plan/01-architecture/provider-interface.md` - Vtable interface specification
- `scratch/plan/02-data-formats/request-response.md` - Request/response formats
- `scratch/plan/02-data-formats/streaming.md` - Streaming event formats

## Objective

Create performance benchmark tests that measure critical path operation timings across all providers, establish performance baselines, and detect regressions. Target: <100 microseconds for serialization/parsing operations.

## Rationale

Performance benchmarks serve multiple purposes:
1. **Verify overhead is acceptable** - Multi-provider abstraction adds layers; verify overhead is minimal
2. **Detect regressions** - CI integration catches performance degradation early
3. **Guide optimization** - Identify bottlenecks in provider implementations
4. **Document performance** - Establish baseline metrics for future development
5. **Provider comparison** - Ensure all providers have similar performance characteristics

## Interface

### Benchmark Test Files

| File | Purpose |
|------|---------|
| `tests/performance/bench_request_serialization.c` | Request serialization benchmarks |
| `tests/performance/bench_response_parsing.c` | Response parsing benchmarks |
| `tests/performance/bench_message_conversion.c` | Message format conversion benchmarks |
| `tests/performance/bench_tool_dispatch.c` | Tool call dispatch overhead |
| `tests/performance/bench_streaming_overhead.c` | SSE parsing and event dispatch overhead |
| `tests/performance/bench_memory_patterns.c` | Memory allocation pattern benchmarks |

### Benchmark Fixture Files

| File | Purpose |
|------|---------|
| `tests/fixtures/benchmarks/request_simple.json` | Simple text-only request |
| `tests/fixtures/benchmarks/request_complex.json` | Complex request (10 messages, 5 tools) |
| `tests/fixtures/benchmarks/response_simple.json` | Simple text response |
| `tests/fixtures/benchmarks/response_complex.json` | Complex response (multiple content blocks) |
| `tests/fixtures/benchmarks/stream_basic.txt` | Basic SSE stream fixture |
| `tests/fixtures/benchmarks/stream_complex.txt` | Complex SSE stream (tool calls, thinking) |

### Benchmark Utilities

| Function | Purpose |
|----------|---------|
| `benchmark_run(name, iterations, fn, ctx)` | Execute benchmark with timing |
| `benchmark_report(name, iterations, elapsed_ns)` | Print benchmark results |
| `benchmark_create_request_simple(ctx)` | Create simple benchmark request |
| `benchmark_create_request_complex(ctx)` | Create complex benchmark request (10 messages, 5 tools) |
| `benchmark_create_response_simple(ctx)` | Create simple benchmark response |
| `benchmark_create_response_complex(ctx)` | Create complex benchmark response |

## Behaviors

### Timing Methodology

**Clock Source:**
- Use `clock_gettime(CLOCK_MONOTONIC, &ts)` for high-resolution timing
- Calculate elapsed time in nanoseconds: `(end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec)`
- Convert to microseconds for reporting: `elapsed_ns / 1000`

**Iteration Strategy:**
- Run warmup iterations (100) to prime caches
- Run measurement iterations (1000) for stable averages
- Calculate: min, max, avg, p50, p95, p99
- Report all percentiles for visibility into variance

**Statistical Validity:**
- Run each benchmark 5 times, report median of averages
- Flag high variance (stddev > 20% of mean) as unstable
- CI gates on p95, not average (protects against tail latencies)

### Benchmark Categories

#### 1. Request Serialization Benchmarks

**Target:** <100 microseconds per request (complex)

**Scenarios:**
- Simple text request (1 user message) → Anthropic JSON
- Simple text request (1 user message) → OpenAI JSON
- Simple text request (1 user message) → Google JSON
- Complex request (10 messages, 5 tools) → Anthropic JSON
- Complex request (10 messages, 5 tools) → OpenAI JSON
- Complex request (10 messages, 5 tools) → Google JSON
- Request with thinking level (budget calculation) → all providers
- Request with tool definitions → all providers

**Measurement:**
- Start timer before serialization
- Call provider's request builder function
- Stop timer after JSON string returned
- Includes: struct traversal, JSON building, string allocation

#### 2. Response Parsing Benchmarks

**Target:** <100 microseconds per response (complex)

**Scenarios:**
- Simple text response (Anthropic) → internal format
- Simple text response (OpenAI) → internal format
- Simple text response (Google) → internal format
- Complex response (multiple blocks, tool calls) → all providers
- Response with thinking content → all providers
- Response with usage metadata → all providers
- Error response parsing → all providers

**Measurement:**
- Start timer with raw JSON string
- Call provider's response parser
- Stop timer after internal struct populated
- Includes: JSON parsing, struct allocation, field mapping

#### 3. Message Conversion Benchmarks

**Target:** <50 microseconds per message (typical)

**Scenarios:**
- User message (text only) → canonical format
- Assistant message (text only) → canonical format
- Message with tool call → canonical format
- Message with tool result → canonical format
- Message with thinking block → canonical format
- Canonical → Anthropic wire format
- Canonical → OpenAI wire format
- Canonical → Google wire format

**Measurement:**
- Start timer with source format
- Call conversion function
- Stop timer after target format populated
- Includes: field mapping, string duplication, struct allocation

#### 4. Tool Dispatch Overhead

**Target:** <20 microseconds per dispatch

**Scenarios:**
- Tool call extraction from response
- Tool ID generation (for providers without native IDs)
- Tool parameter validation
- Tool result serialization
- Tool call round-trip (extract → execute → result)

**Measurement:**
- Start timer with parsed response containing tool call
- Extract tool info, generate ID, create result structure
- Stop timer after result ready for next request
- Includes: JSON access, string operations, struct allocation

#### 5. Streaming Overhead Benchmarks

**Target:** <50 microseconds per SSE event

**Scenarios:**
- SSE event parsing (single event)
- SSE event dispatch to callback
- Text delta accumulation (100 deltas)
- Tool call accumulation (streaming tool calls)
- Multiple event types in sequence
- Per-provider SSE parsing (Anthropic, OpenAI, Google)

**Measurement:**
- Start timer with raw SSE data
- Feed to SSE parser, invoke callbacks
- Stop timer after all events processed
- Includes: event parsing, callback dispatch, state accumulation

#### 6. Memory Allocation Pattern Benchmarks

**Target:** <10 allocations per request (typical)

**Scenarios:**
- Request allocation pattern (count allocations)
- Response allocation pattern (count allocations)
- Streaming allocation pattern (count allocations per event)
- Memory peak usage (max resident during benchmark)
- Talloc hierarchy depth (measure tree depth)

**Measurement:**
- Hook talloc allocation functions
- Count allocations during operation
- Measure peak memory usage
- Verify no memory leaks (talloc_total_blocks at end == start)
- Report allocations per operation

### Per-Provider Benchmarks

**Comparative Analysis:**
Run identical operations across all three providers to ensure:
- No single provider is significantly slower
- Abstraction overhead is consistent
- Provider-specific optimizations are effective

**Report Format:**
```
Request Serialization (Complex):
  Anthropic:  87.3 μs (p95: 92.1 μs)
  OpenAI:     91.2 μs (p95: 96.8 μs)
  Google:     89.5 μs (p95: 94.3 μs)
  Variance:   4.3% (acceptable)
```

### Baseline Establishment

**Initial Baseline:**
- Run all benchmarks on clean system
- Record results in `tests/performance/baseline.txt`
- Commit baseline with initial implementation
- Use as comparison point for future runs

**Baseline Format:**
```
# Performance Baseline - rel-07
# Recorded: 2025-12-23
# System: Linux 6.12.57, x86_64
# Compiler: gcc 12.2.0 -O2

[benchmark_name]
iterations=1000
avg_us=87.3
p50_us=85.1
p95_us=92.1
p99_us=98.7
```

### Regression Detection

**CI Integration:**
- Run benchmarks in CI on every commit
- Compare against baseline (allow 10% degradation)
- Fail build if p95 exceeds baseline p95 * 1.10
- Report performance improvements (speedups)
- Track performance trends over time

**Regression Thresholds:**
- Critical path (request/response): +10% fails CI
- Non-critical path (tool dispatch): +20% warning only
- Memory allocations: +5 allocations fails CI
- Streaming overhead: +15% fails CI

**Manual Override:**
Provide `SKIP_PERF_GATES=1` environment variable for emergency situations where performance regression is known and accepted (requires justification in commit message).

### Reproducibility

**Fixed Fixtures:**
- All benchmarks use identical fixtures across runs
- Fixtures committed to git (deterministic)
- No random data generation in benchmarks
- Same request/response structures every time

**Environment Control:**
- Document CPU/memory specs in baseline
- Disable CPU frequency scaling (if possible)
- Run with consistent compiler flags (-O2)
- Minimize background processes in CI

**Variance Handling:**
- Run multiple iterations (1000+)
- Report percentiles, not just averages
- Flag high variance as test instability
- Re-run unstable benchmarks automatically

## Directory Structure

```
tests/performance/
├── bench_request_serialization.c   - Request serialization benchmarks
├── bench_response_parsing.c        - Response parsing benchmarks
├── bench_message_conversion.c      - Message format conversion
├── bench_tool_dispatch.c           - Tool call dispatch overhead
├── bench_streaming_overhead.c      - SSE parsing overhead
├── bench_memory_patterns.c         - Memory allocation patterns
├── benchmark_utils.c               - Shared benchmark utilities
├── benchmark_utils.h               - Benchmark utility declarations
└── baseline.txt                    - Performance baseline metrics

tests/fixtures/benchmarks/
├── request_simple.json             - Simple request fixture
├── request_complex.json            - Complex request fixture (10 msgs, 5 tools)
├── response_simple.json            - Simple response fixture
├── response_complex.json           - Complex response fixture
├── stream_basic.txt                - Basic SSE stream
└── stream_complex.txt              - Complex SSE stream
```

## Test Scenarios

### Request Serialization Benchmark

1. Create benchmark request (simple and complex variants)
2. Start high-resolution timer
3. Serialize request to provider wire format
4. Stop timer
5. Calculate elapsed time in microseconds
6. Repeat 1000 times, collect timings
7. Report: min, max, avg, p50, p95, p99
8. Assert: p95 < 100 microseconds (complex), p95 < 50 microseconds (simple)

### Response Parsing Benchmark

1. Load fixture with provider response JSON
2. Start high-resolution timer
3. Parse response into internal format
4. Stop timer
5. Calculate elapsed time in microseconds
6. Repeat 1000 times, collect timings
7. Report: min, max, avg, p50, p95, p99
8. Assert: p95 < 100 microseconds (complex), p95 < 50 microseconds (simple)

### Streaming Overhead Benchmark

1. Load fixture with SSE event stream
2. Create SSE parser with mock callback
3. Start timer
4. Feed SSE data to parser
5. Count callback invocations
6. Stop timer
7. Calculate microseconds per event
8. Assert: p95 < 50 microseconds per event

### Memory Allocation Benchmark

1. Hook talloc allocation functions
2. Reset allocation counter
3. Perform operation (request/response/streaming)
4. Record allocation count
5. Free all resources
6. Verify no leaks (talloc counter returns to baseline)
7. Report: allocations per operation
8. Assert: allocations < threshold (10 for simple, 50 for complex)

### Cross-Provider Comparison

1. Run identical benchmark across all three providers
2. Collect timings for each provider
3. Calculate variance between providers
4. Report comparative results
5. Assert: variance < 20% (no provider is significantly slower)

## Postconditions

- [ ] `tests/performance/` directory created
- [ ] 6 benchmark test files created
- [ ] `benchmark_utils.c` and `benchmark_utils.h` created
- [ ] 6 benchmark fixture files created in `tests/fixtures/benchmarks/`
- [ ] Baseline metrics recorded in `tests/performance/baseline.txt`
- [ ] All benchmarks compile without warnings
- [ ] All benchmarks execute successfully
- [ ] Results reported in human-readable format (microseconds)
- [ ] Percentiles calculated and reported (p50, p95, p99)
- [ ] Cross-provider comparison shows <20% variance
- [ ] All benchmarks meet performance targets:
  - Request serialization: p95 < 100μs (complex)
  - Response parsing: p95 < 100μs (complex)
  - Message conversion: p95 < 50μs
  - Tool dispatch: p95 < 20μs
  - Streaming overhead: p95 < 50μs per event
  - Memory allocations: < 50 allocations (complex request)
- [ ] Makefile updated with benchmark targets:
  - `make bench` - Run all benchmarks
  - `make bench-report` - Run benchmarks and generate report
  - `make bench-baseline` - Record new baseline
  - `make bench-compare` - Compare against baseline
- [ ] CI integration documented (performance gates)
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-performance-benchmarking.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Build benchmarks
make tests/performance/bench_request_serialization
make tests/performance/bench_response_parsing
make tests/performance/bench_message_conversion
make tests/performance/bench_tool_dispatch
make tests/performance/bench_streaming_overhead
make tests/performance/bench_memory_patterns

# Run all benchmarks
make bench

# Example output:
# Benchmark: Request Serialization (Simple) - Anthropic
#   Iterations: 1000
#   Avg: 42.3 μs
#   p50: 41.1 μs
#   p95: 48.7 μs
#   p99: 53.2 μs
#   Status: PASS (target: <50μs)

# Generate baseline
make bench-baseline
# Creates/updates tests/performance/baseline.txt

# Compare against baseline
make bench-compare
# Shows diff between current and baseline
# Exits 1 if regressions exceed thresholds

# Verify no memory leaks in benchmarks
valgrind --leak-check=full ./build/tests/performance/bench_request_serialization
# Should show "All heap blocks were freed -- no leaks are possible"

# Verify CI integration
# Check that .github/workflows/ci.yml includes performance gates
grep "make bench-compare" .github/workflows/ci.yml
```

## Integration with CI

### GitHub Actions Job

Add to `.github/workflows/ci.yml`:

```yaml
performance:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v2
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y libpq-dev libcurl4-openssl-dev check
    - name: Build benchmarks
      run: make bench
    - name: Run benchmarks
      run: make bench-report
    - name: Check for regressions
      run: make bench-compare
    - name: Upload benchmark results
      uses: actions/upload-artifact@v2
      with:
        name: benchmark-results
        path: tests/performance/results.txt
```

### Performance Gates

**Blocking Gates (CI fails):**
- Request serialization p95 > 110μs (10% over target)
- Response parsing p95 > 110μs (10% over target)
- Memory allocations > 55 (10% over target)

**Warning Gates (CI warns, doesn't fail):**
- Message conversion p95 > 60μs (20% over target)
- Tool dispatch p95 > 24μs (20% over target)
- Streaming overhead p95 > 60μs (20% over target)

### Benchmark Fixtures

**Reproducibility Requirements:**
- Fixtures must be deterministic (no timestamps, no random IDs)
- Committed to git for consistent CI runs
- Same complexity across all test runs
- Representative of real-world usage patterns

## Success Criteria

**Benchmark Implementation:**
- All 6 benchmark files compile and run
- Timing methodology uses CLOCK_MONOTONIC
- Percentile calculation is correct (p50, p95, p99)
- Results reported in microseconds with 1 decimal place

**Performance Targets:**
- All benchmarks meet or exceed performance targets
- Cross-provider variance < 20%
- Memory allocation patterns efficient (<50 allocations for complex)
- No memory leaks detected

**CI Integration:**
- Makefile targets for running benchmarks
- Baseline comparison detects regressions
- Performance gates documented and enforced
- Results uploaded as artifacts

**Documentation:**
- Baseline metrics recorded and committed
- Performance targets documented in this file
- CI integration documented
- Escape hatch (`SKIP_PERF_GATES=1`) documented

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
