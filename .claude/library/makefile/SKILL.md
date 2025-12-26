---
name: makefile
description: Makefile skill for the ikigai project
---

# Makefile

Build system for ikigai with testing, coverage enforcement, and multi-distro packaging.

## Build

- `make all` - Build client binary (default: debug mode)
- `make release` - Rebuild with optimizations and hardening
- `make clean` - Remove all build artifacts
- `make build-tests` - Compile tests without running

## Installation

- `make install` - Install to PREFIX (default: /usr/local)
- `make uninstall` - Remove installed files
- `make install-deps` - Install build deps (Debian/Ubuntu)

## Testing

- `make check` - Run all tests (unit + integration)
- `make check-unit` - Run unit tests with parallel support
- `make check-integration` - Run integration tests
- `make clean-test-runs` - Remove .run sentinel files

## Mock Verification

- `make verify-mocks` - Verify OpenAI fixtures (OPENAI_API_KEY)
- `make verify-mocks-anthropic` - Verify Anthropic fixtures
- `make verify-mocks-google` - Verify Google fixtures
- `make verify-mocks-all` - Verify all provider fixtures
- `make verify-credentials` - Validate API keys

## VCR Recording

- `make vcr-record-openai` - Re-record OpenAI fixtures
- `make vcr-record-anthropic` - Re-record Anthropic fixtures
- `make vcr-record-google` - Re-record Google fixtures
- `make vcr-record-all` - Re-record all fixtures

## Dynamic Analysis

- `make check-sanitize` - Run with ASan and UBSan
- `make check-valgrind` - Run with Valgrind Memcheck
- `make check-helgrind` - Run with Valgrind Helgrind
- `make check-tsan` - Run with ThreadSanitizer
- `make check-dynamic` - Run all dynamic analysis

## Quality Assurance

- `make coverage` - Enforce 100% line/function/branch coverage
- `make lint` - Run all lint checks
- `make complexity` - Check complexity (15) and nesting (5)
- `make filesize` - Verify files under 16KB
- `make ci` - Run complete CI pipeline

## Code Quality

- `make fmt` - Format with uncrustify (K&R, 120 cols)
- `make tags` - Generate ctags for src/
- `make cloc` - Count lines of code

## Distribution

- `make dist` - Create source tarball
- `make distro-images` - Build Docker images
- `make distro-images-clean` - Remove Docker images
- `make distro-clean` - Clean via Docker
- `make distro-check` - CI checks on all distros
- `make distro-package` - Build .deb and .rpm packages

## Utility

- `make help` - Display detailed help

## Build Modes

| Mode | Flags | Purpose |
|------|-------|---------|
| `BUILD=debug` | `-O0 -g3 -DDEBUG` | Default build with full debug symbols |
| `BUILD=release` | `-O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2` | Optimized production build with hardening |
| `BUILD=sanitize` | `-O0 -g3 -fsanitize=address,undefined` | Debug build with ASan and UBSan |
| `BUILD=tsan` | `-O0 -g3 -fsanitize=thread` | Debug build with ThreadSanitizer |
| `BUILD=valgrind` | `-O0 -g3 -fno-omit-frame-pointer` | Debug build optimized for Valgrind |

## Important Notes

- Never run parallel make with different targets (incompatible BUILD flags)
- Coverage requires 100% on all metrics
- Max file size: 16KB; complexity: 15; nesting: 5
- Use SKIP_SIGNAL_TESTS=1 under sanitizers
- Vendor files (yyjson, fzy) use relaxed warnings
