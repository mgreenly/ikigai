---
name: makefile
description: Makefile skill for the ikigai project
---

# Makefile

Build system for ikigai with testing, coverage enforcement, and multi-distro packaging.

## Build

- `make all` - Build the ikigai client binary with the current BUILD mode (default: debug).
- `make release` - Clean and rebuild with release optimizations and security hardening.
- `make build-tests` - Compile all test binaries without running them.

## Installation

- `make install` - Install binary to PREFIX/bin and config to PREFIX/etc/ikigai (default: /usr/local).
- `make uninstall` - Remove installed binary and configuration files.
- `make install-deps` - Install build dependencies using apt-get (Debian/Ubuntu).

## Testing

- `make check` - Build and run all tests (unit + integration), then print success message.
- `make check-unit` - Build and run unit tests with parallel execution support via Make's -j flag.
- `make check-integration` - Build and run integration tests including database tests.
- `make clean-test-runs` - Remove .run sentinel files created by parallel test execution.

## Mock Verification

- `make verify-mocks` - Verify OpenAI mock fixtures against real API using OPENAI_API_KEY or credentials.json.
- `make verify-mocks-anthropic` - Verify Anthropic mock fixtures against real API using ANTHROPIC_API_KEY or credentials.json.
- `make verify-mocks-google` - Verify Google mock fixtures against real API using GOOGLE_API_KEY or credentials.json.
- `make verify-mocks-all` - Run verification for all provider mock fixtures sequentially.
- `make verify-credentials` - Validate API keys in ~/.config/ikigai/credentials.json without exposing values.

## VCR Recording

- `make vcr-record-openai` - Re-record OpenAI VCR fixtures by running tests with VCR_RECORD=1.
- `make vcr-record-anthropic` - Re-record Anthropic VCR fixtures by running tests with VCR_RECORD=1.
- `make vcr-record-google` - Re-record Google VCR fixtures by running tests with VCR_RECORD=1.
- `make vcr-record-all` - Re-record all provider VCR fixtures in sequence.

## Dynamic Analysis

- `make check-sanitize` - Run all tests with AddressSanitizer and UndefinedBehaviorSanitizer enabled.
- `make check-valgrind` - Run all tests under Valgrind Memcheck with leak detection and origin tracking.
- `make check-helgrind` - Run all tests under Valgrind Helgrind for thread error detection.
- `make check-tsan` - Run all tests with ThreadSanitizer for data race detection.
- `make check-dynamic` - Run all dynamic analysis checks sequentially (or parallel with PARALLEL=1).

## Quality Assurance

- `make coverage` - Build with gcov instrumentation, run tests, and enforce 100% line/function/branch coverage.
- `make lint` - Run all lint checks (combines complexity and filesize).
- `make complexity` - Check cyclomatic complexity (threshold: 15) and nesting depth (threshold: 5).
- `make filesize` - Verify all non-vendor source files are under 16KB.
- `make ci` - Run complete CI pipeline: filesize, complexity, coverage, tests, and all dynamic analysis.

## Code Quality

- `make fmt` - Format source code with uncrustify using K&R style and 120-character line length.
- `make tags` - Generate ctags index for src/ directory.
- `make cloc` - Count lines of code in src/, tests/, and Makefile using cloc.

## Distribution

- `make dist` - Create source distribution tarball in distros/dist/.
- `make distro-images` - Build Docker images for all distributions in distros/*/.
- `make distro-images-clean` - Remove all Docker images for distributions.
- `make distro-clean` - Clean build artifacts using the first Docker image.
- `make distro-check` - Run CI checks on all supported distributions via Docker.
- `make distro-package` - Build distribution packages (.deb, .rpm) in Docker containers.

## Utility

- `make clean` - Remove all build artifacts, coverage data, gcov files, and tags.
- `make help` - Display detailed help with all targets, build modes, and examples.

## Build Modes

| Mode | Flags | Purpose |
|------|-------|---------|
| `BUILD=debug` | `-O0 -g3 -DDEBUG` | Default build with full debug symbols |
| `BUILD=release` | `-O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2` | Optimized production build with hardening |
| `BUILD=sanitize` | `-O0 -g3 -fsanitize=address,undefined` | Debug build with ASan and UBSan |
| `BUILD=tsan` | `-O0 -g3 -fsanitize=thread` | Debug build with ThreadSanitizer |
| `BUILD=valgrind` | `-O0 -g3 -fno-omit-frame-pointer` | Debug build optimized for Valgrind backtraces |

## Common Workflows

```bash
# Build and run tests
make check

# Build release binary
make release

# Run tests with sanitizers
make check-sanitize

# Generate coverage report
make coverage

# Full CI pipeline
make ci

# Build with specific mode
make all BUILD=release

# Run unit tests in parallel
make -j8 check-unit

# Verify API fixtures work against real APIs
make verify-mocks-all

# Format code before committing
make fmt

# Check code quality
make lint

# Build distribution packages
make distro-package DISTROS="debian fedora"
```

## Important Notes

- Never run parallel make with different BUILD targets (incompatible flags).
- Coverage requires 100% on lines, functions, and branches.
- Max file size: 16KB; complexity threshold: 15; nesting depth: 5.
- Use SKIP_SIGNAL_TESTS=1 under sanitizers to skip signal handler tests.
- Vendor files (yyjson, fzy) compile with relaxed warnings.
- LCOV exclusion markers are capped at 2684 occurrences.
