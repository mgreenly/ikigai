# check-filesize: Verify source files are under size limit
# Default limit: 16KB (16384 bytes)

.PHONY: check-filesize

FILESIZE_LIMIT ?= 16384

check-filesize:
ifdef FILE
	@# Single file mode
	@if [ ! -f "$(FILE)" ]; then \
		echo "🔴 $(FILE): file not found"; \
		exit 1; \
	fi; \
	size=$$(wc -c < "$(FILE)"); \
	if [ $$size -gt $(FILESIZE_LIMIT) ]; then \
		echo "🔴 $(FILE): $$size bytes (exceeds $(FILESIZE_LIMIT))"; \
		exit 1; \
	else \
		echo "🟢 $(FILE)"; \
	fi
else ifdef RAW
	@failed=0; \
	for file in $(SRC_FILES) $(TEST_FILES); do \
		size=$$(wc -c < "$$file"); \
		if [ $$size -gt $(FILESIZE_LIMIT) ]; then \
			echo "FAIL $$file: $$size bytes (exceeds $(FILESIZE_LIMIT))"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	if [ $$failed -gt 0 ]; then \
		exit 1; \
	fi
else
	@# Bulk mode - check all source files
	@passed=0; failed=0; \
	for file in $(SRC_FILES) $(TEST_FILES); do \
		size=$$(wc -c < "$$file"); \
		if [ $$size -gt $(FILESIZE_LIMIT) ]; then \
			echo "🔴 $$file: $$size bytes (exceeds $(FILESIZE_LIMIT))"; \
			failed=$$((failed + 1)); \
		else \
			echo "🟢 $$file"; \
			passed=$$((passed + 1)); \
		fi; \
	done; \
	total=$$((passed + failed)); \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All $$total files under size limit"; \
	else \
		echo "❌ $$failed/$$total files exceed size limit"; \
		exit 1; \
	fi
endif
