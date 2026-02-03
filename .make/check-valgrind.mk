# check-valgrind: Run tests under Valgrind Memcheck
# Reports only Valgrind errors, not test assertion failures
#
# Note: check framework forks for each test, so valgrind's --error-exitcode
# gets masked by the framework's exit code. We detect errors by parsing output.

.PHONY: check-valgrind

VALGRIND_BUILDDIR = build-valgrind
VALGRIND_SUPP = .suppressions/valgrind.supp

# Pattern to detect valgrind memory errors in output
VALGRIND_ERROR_PATTERN = definitely lost: [1-9]|ERROR SUMMARY: [1-9]

check-valgrind:
ifdef FILE
	@# Convert source path to binary path if needed (tests/foo.c -> build-valgrind/tests/foo)
	@file="$(FILE)"; \
	if echo "$$file" | grep -q '\.c$$'; then \
		file="$(VALGRIND_BUILDDIR)/$${file%.c}"; \
	fi; \
	if [ ! -x "$$file" ]; then \
		echo "🔴 $$file: binary not found"; \
		exit 1; \
	fi; \
	output=$$(mktemp); \
	valgrind --leak-check=full --errors-for-leak-kinds=definite --suppressions=$(VALGRIND_SUPP) \
		"$$file" >"$$output" 2>&1; \
	exitcode=$$?; \
	if grep -qE '$(VALGRIND_ERROR_PATTERN)' "$$output"; then \
		grep -E '^==[0-9]+== ' "$$output" | sed 's/^/🔴 /'; \
		rm -f "$$output"; exit 1; \
	elif [ $$exitcode -ne 0 ]; then \
		echo "🔴 $(FILE): pre-existing test failure"; \
		rm -f "$$output"; exit 1; \
	else \
		echo "🟢 $(FILE)"; rm -f "$$output"; \
	fi
else ifdef RAW
	@# RAW mode - run tests with full Valgrind output visible
	$(MAKE) BUILDDIR=$(VALGRIND_BUILDDIR) BUILD=valgrind check-link
	@mkdir -p reports/check
	@find tests/unit tests/integration -type d 2>/dev/null | sed 's|^tests/|reports/check/|' | xargs mkdir -p 2>/dev/null || true
	@for bin in $$(find $(VALGRIND_BUILDDIR)/tests/unit $(VALGRIND_BUILDDIR)/tests/integration \
		-name '*_test' -type f -executable 2>/dev/null); do \
		echo "=== $$bin ==="; \
		valgrind --leak-check=full --errors-for-leak-kinds=definite --suppressions=$(VALGRIND_SUPP) \
			"$$bin" || exit 1; \
	done
else
	@if ! $(MAKE) -s BUILDDIR=$(VALGRIND_BUILDDIR) BUILD=valgrind check-link >/dev/null 2>&1; then \
		echo "🔴 Pre-existing build failures - fix compilation/linking before checking for memory errors"; \
		exit 1; \
	fi
	@mkdir -p reports/check
	@find tests/unit tests/integration -type d 2>/dev/null | sed 's|^tests/|reports/check/|' | xargs mkdir -p 2>/dev/null || true
	@tmpdir=$$(mktemp -d); \
	find $(VALGRIND_BUILDDIR)/tests/unit $(VALGRIND_BUILDDIR)/tests/integration \
		-name '*_test' -type f -executable 2>/dev/null | \
	xargs -P1 -I{} sh -c ' \
		tmpdir="$$1"; bin="$$2"; \
		output="$$tmpdir/$$(basename $$bin).out"; \
		valgrind --leak-check=full --errors-for-leak-kinds=definite --suppressions=.suppressions/valgrind.supp \
			"$$bin" >"$$output" 2>&1; \
		exitcode=$$?; \
		if grep -qE "definitely lost: [1-9]|ERROR SUMMARY: [1-9]" "$$output"; then \
			echo "$$bin" >> "$$tmpdir/valgrind_failed"; \
		elif [ $$exitcode -ne 0 ]; then \
			echo "$$bin" >> "$$tmpdir/test_failed"; \
		fi; \
		rm -f "$$output"' _ "$$tmpdir" {}; \
	if [ -f "$$tmpdir/test_failed" ]; then \
		echo "🔴 Pre-existing test failures - fix tests before checking for memory errors"; \
		rm -rf "$$tmpdir"; \
		exit 1; \
	fi; \
	valgrind_count=0; \
	if [ -f "$$tmpdir/valgrind_failed" ]; then \
		while read bin; do \
			src="$${bin#$(VALGRIND_BUILDDIR)/}.c"; \
			echo "🔴 $$src"; \
		done < "$$tmpdir/valgrind_failed"; \
		valgrind_count=$$(wc -l < "$$tmpdir/valgrind_failed"); \
	fi; \
	rm -rf "$$tmpdir"; \
	if [ $$valgrind_count -gt 0 ]; then \
		echo "❌ $$valgrind_count binaries have Valgrind errors"; \
		exit 1; \
	else \
		echo "✅ No Valgrind errors"; \
	fi
endif
