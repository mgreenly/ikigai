# check-helgrind: Run tests under Valgrind Helgrind (thread error detector)
# Reports data races, lock misuse, and threading errors
#
# Note: check framework forks for each test, so valgrind's --error-exitcode
# gets masked by the framework's exit code. We detect errors by parsing output.

.PHONY: check-helgrind

HELGRIND_BUILDDIR = build-helgrind
HELGRIND_SUPP = .suppressions/helgrind.supp

# Pattern to detect helgrind thread errors in output
HELGRIND_ERROR_PATTERN = Possible data race|ERROR SUMMARY: [1-9]

check-helgrind:
ifdef FILE
	@# Convert source path to binary path if needed (tests/foo.c -> build-helgrind/tests/foo)
	@file="$(FILE)"; \
	if echo "$$file" | grep -q '\.c$$'; then \
		file="$(HELGRIND_BUILDDIR)/$${file%.c}"; \
	fi; \
	if [ ! -x "$$file" ]; then \
		echo "🔴 $$file: binary not found"; \
		exit 1; \
	fi; \
	output=$$(mktemp); \
	valgrind --tool=helgrind --suppressions=$(HELGRIND_SUPP) \
		"$$file" >"$$output" 2>&1; \
	exitcode=$$?; \
	if grep -qE '$(HELGRIND_ERROR_PATTERN)' "$$output"; then \
		grep -E '^==[0-9]+== ' "$$output" | sed 's/^/🔴 /'; \
		rm -f "$$output"; exit 1; \
	elif [ $$exitcode -ne 0 ]; then \
		echo "🔴 $(FILE): pre-existing test failure"; \
		rm -f "$$output"; exit 1; \
	else \
		echo "🟢 $(FILE)"; rm -f "$$output"; \
	fi
else ifdef RAW
	@# RAW mode - run tests with full Helgrind output visible
	$(MAKE) BUILDDIR=$(HELGRIND_BUILDDIR) BUILD=valgrind check-link
	@mkdir -p reports/check
	@find tests/unit tests/integration -type d 2>/dev/null | sed 's|^tests/|reports/check/|' | xargs mkdir -p 2>/dev/null || true
	@for bin in $$(find $(HELGRIND_BUILDDIR)/tests/unit $(HELGRIND_BUILDDIR)/tests/integration \
		-name '*_test' -type f -executable 2>/dev/null); do \
		echo "=== $$bin ==="; \
		valgrind --tool=helgrind --suppressions=$(HELGRIND_SUPP) \
			"$$bin" || exit 1; \
	done
else
	@if ! $(MAKE) -s BUILDDIR=$(HELGRIND_BUILDDIR) BUILD=valgrind check-link >/dev/null 2>&1; then \
		echo "🔴 Pre-existing build failures - fix compilation/linking before checking for thread errors"; \
		exit 1; \
	fi
	@mkdir -p reports/check
	@find tests/unit tests/integration -type d 2>/dev/null | sed 's|^tests/|reports/check/|' | xargs mkdir -p 2>/dev/null || true
	@tmpdir=$$(mktemp -d); \
	find $(HELGRIND_BUILDDIR)/tests/unit $(HELGRIND_BUILDDIR)/tests/integration \
		-name '*_test' -type f -executable 2>/dev/null | \
	xargs -P1 -I{} sh -c ' \
		tmpdir="$$1"; bin="$$2"; \
		output="$$tmpdir/$$(basename $$bin).out"; \
		valgrind --tool=helgrind --suppressions=.suppressions/helgrind.supp \
			"$$bin" >"$$output" 2>&1; \
		exitcode=$$?; \
		if grep -qE "Possible data race|ERROR SUMMARY: [1-9]" "$$output"; then \
			echo "$$bin" >> "$$tmpdir/helgrind_failed"; \
		elif [ $$exitcode -ne 0 ]; then \
			echo "$$bin" >> "$$tmpdir/test_failed"; \
		fi; \
		rm -f "$$output"' _ "$$tmpdir" {}; \
	if [ -f "$$tmpdir/test_failed" ]; then \
		echo "🔴 Pre-existing test failures - fix tests before checking for thread errors"; \
		rm -rf "$$tmpdir"; \
		exit 1; \
	fi; \
	helgrind_count=0; \
	if [ -f "$$tmpdir/helgrind_failed" ]; then \
		while read bin; do \
			src="$${bin#$(HELGRIND_BUILDDIR)/}.c"; \
			echo "🔴 $$src"; \
		done < "$$tmpdir/helgrind_failed"; \
		helgrind_count=$$(wc -l < "$$tmpdir/helgrind_failed"); \
	fi; \
	rm -rf "$$tmpdir"; \
	if [ $$helgrind_count -gt 0 ]; then \
		echo "❌ $$helgrind_count binaries have Helgrind errors"; \
		exit 1; \
	else \
		echo "✅ No Helgrind errors"; \
	fi
endif
