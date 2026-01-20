# check-compile: Compile all source files
# This target compiles source files to object files and reports status

.PHONY: check-compile

# check-compile: Compile all source files
check-compile:
ifdef FILE
	@obj=$$(echo $(FILE) | sed 's|^src/|$(BUILDDIR)/|; s|^tests/|$(BUILDDIR)/tests/|; s|\.c$$|.o|'); \
	mkdir -p $$(dirname $$obj); \
	if echo "$(FILE)" | grep -q "^src/vendor/"; then \
		cflags="$(VENDOR_CFLAGS)"; \
	else \
		cflags="$(CFLAGS)"; \
	fi; \
	if output=$$($(CC) $$cflags -c $(FILE) -o $$obj 2>&1); then \
		echo "🟢 $(FILE)"; \
	else \
		error=$$(echo "$$output" | head -1); \
		echo "🔴 $$error"; \
	fi
else
	@$(MAKE) -k -j$(MAKE_JOBS) $(ALL_OBJECTS) 2>&1 | grep -E "^(🟢|🔴)" || true; \
	failed=0; \
	for obj in $(ALL_OBJECTS); do \
		[ ! -f "$$obj" ] && failed=$$((failed + 1)); \
	done; \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All files compiled"; \
	else \
		echo "❌ $$failed files failed to compile"; \
		exit 1; \
	fi
endif
