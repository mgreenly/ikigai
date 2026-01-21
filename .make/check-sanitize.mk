# check-sanitize: Run tests with AddressSanitizer and UndefinedBehaviorSanitizer
# Reports only sanitizer errors, not test assertion failures

.PHONY: check-sanitize

SANITIZE_BUILDDIR = build-sanitize
LSAN_SUPP = .suppressions/lsan.supp

check-sanitize:
ifdef FILE
	@if [ ! -x "$(FILE)" ]; then \
		echo "🔴 $(FILE): binary not found"; \
		exit 1; \
	fi; \
	stderr=$$(mktemp); \
	LSAN_OPTIONS="suppressions=$(LSAN_SUPP)" "$(FILE)" >"$$stderr" 2>&1 || true; \
	if grep -qE '==.*==ERROR: AddressSanitizer|runtime error:' "$$stderr"; then \
		sed 's/^/🔴 /' "$$stderr"; \
		rm -f "$$stderr"; exit 1; \
	else \
		echo "🟢 $(FILE)"; rm -f "$$stderr"; \
	fi
else
	@$(MAKE) -s BUILDDIR=$(SANITIZE_BUILDDIR) BUILD=sanitize check-link >/dev/null 2>&1 || true
	@tmpdir=$$(mktemp -d); \
	find $(SANITIZE_BUILDDIR)/tests/unit $(SANITIZE_BUILDDIR)/tests/integration \
		-name '*_test' -type f -executable 2>/dev/null | \
	xargs -P$(MAKE_JOBS) -I{} sh -c ' \
		tmpdir="$$1"; bin="$$2"; \
		stderr="$$tmpdir/stderr.$$$$"; \
		LSAN_OPTIONS="suppressions=.suppressions/lsan.supp" "$$bin" >"$$stderr" 2>&1; \
		if grep -qE "==.*==ERROR: AddressSanitizer|runtime error:" "$$stderr"; then \
			echo "$$bin" >> "$$tmpdir/sanitizer_failed"; \
		fi; \
		rm -f "$$stderr"' _ "$$tmpdir" {}; \
	sanitizer_count=0; \
	if [ -f "$$tmpdir/sanitizer_failed" ]; then \
		while read bin; do echo "🔴 $$bin"; done < "$$tmpdir/sanitizer_failed"; \
		sanitizer_count=$$(wc -l < "$$tmpdir/sanitizer_failed"); \
	fi; \
	rm -rf "$$tmpdir"; \
	if [ $$sanitizer_count -gt 0 ]; then \
		echo "❌ $$sanitizer_count binaries have sanitizer errors"; \
		exit 1; \
	else \
		echo "✅ No sanitizer errors"; \
	fi
endif
