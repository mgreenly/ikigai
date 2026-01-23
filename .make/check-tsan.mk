# check-tsan: Run tests with ThreadSanitizer
# Reports only TSan errors (data races, deadlocks), not test assertion failures

.PHONY: check-tsan

TSAN_BUILDDIR = build-tsan
TSAN_SUPP = .suppressions/tsan.supp

check-tsan:
ifdef FILE
	@if [ ! -x "$(FILE)" ]; then \
		echo "🔴 $(FILE): binary not found"; \
		exit 1; \
	fi; \
	output=$$(mktemp); \
	TSAN_OPTIONS="suppressions=$(TSAN_SUPP)" "$(FILE)" >"$$output" 2>&1; \
	exitcode=$$?; \
	if grep -qE 'WARNING: ThreadSanitizer:' "$$output"; then \
		grep -E 'WARNING: ThreadSanitizer:|^  |^    ' "$$output" | sed 's/^/🔴 /'; \
		rm -f "$$output"; exit 1; \
	elif [ $$exitcode -ne 0 ]; then \
		echo "🔴 $(FILE): pre-existing test failure"; \
		rm -f "$$output"; exit 1; \
	else \
		echo "🟢 $(FILE)"; rm -f "$$output"; \
	fi
else ifdef RAW
	@# RAW mode - run tests with full TSan output visible
	$(MAKE) BUILDDIR=$(TSAN_BUILDDIR) BUILD=tsan check-link
	@for bin in $$(find $(TSAN_BUILDDIR)/tests/unit $(TSAN_BUILDDIR)/tests/integration \
		-name '*_test' -type f -executable 2>/dev/null); do \
		echo "=== $$bin ==="; \
		TSAN_OPTIONS="suppressions=$(TSAN_SUPP)" "$$bin" || exit 1; \
	done
else
	@if ! $(MAKE) -s BUILDDIR=$(TSAN_BUILDDIR) BUILD=tsan check-link >/dev/null 2>&1; then \
		echo "🔴 Pre-existing build failures - fix compilation/linking before checking for races"; \
		exit 1; \
	fi
	@tmpdir=$$(mktemp -d); \
	find $(TSAN_BUILDDIR)/tests/unit $(TSAN_BUILDDIR)/tests/integration \
		-name '*_test' -type f -executable 2>/dev/null | \
	xargs -P$(MAKE_JOBS) -I{} sh -c ' \
		tmpdir="$$1"; bin="$$2"; \
		output="$$tmpdir/$$(basename $$bin).out"; \
		TSAN_OPTIONS="suppressions=.suppressions/tsan.supp" "$$bin" >"$$output" 2>&1; \
		exitcode=$$?; \
		if grep -qE "WARNING: ThreadSanitizer:" "$$output"; then \
			echo "$$bin" >> "$$tmpdir/tsan_failed"; \
		elif [ $$exitcode -ne 0 ]; then \
			echo "$$bin" >> "$$tmpdir/test_failed"; \
		fi; \
		rm -f "$$output"' _ "$$tmpdir" {}; \
	if [ -f "$$tmpdir/test_failed" ]; then \
		echo "🔴 Pre-existing test failures - fix tests before checking for races"; \
		rm -rf "$$tmpdir"; \
		exit 1; \
	fi; \
	tsan_count=0; \
	if [ -f "$$tmpdir/tsan_failed" ]; then \
		while read bin; do echo "🔴 $$bin"; done < "$$tmpdir/tsan_failed"; \
		tsan_count=$$(wc -l < "$$tmpdir/tsan_failed"); \
	fi; \
	rm -rf "$$tmpdir"; \
	if [ $$tsan_count -gt 0 ]; then \
		echo "❌ $$tsan_count binaries have ThreadSanitizer errors"; \
		exit 1; \
	else \
		echo "✅ No ThreadSanitizer errors"; \
	fi
endif
