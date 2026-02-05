# check-compile: Compile all source files to object files

.PHONY: check-compile

check-compile:
ifdef FILE
	@obj=$$(echo $(FILE) | sed 's|^apps/|$(BUILDDIR)/apps/|; s|^shared/|$(BUILDDIR)/shared/|; s|^vendor/|$(BUILDDIR)/vendor/|; s|^tools/|$(BUILDDIR)/tools/|; s|^tests/|$(BUILDDIR)/tests/|; s|\.c$$|.o|'); \
	mkdir -p $$(dirname $$obj); \
	if echo "$(FILE)" | grep -q "^vendor/"; then \
		cflags="$(VENDOR_CFLAGS)"; \
	else \
		cflags="$(CFLAGS)"; \
	fi; \
	if output=$$($(CC) $$cflags -c $(FILE) -o $$obj 2>&1); then \
		echo "🟢 $(FILE)"; \
	else \
		echo "$$output" | grep -E "^[^:]+:[0-9]+:[0-9]+:" | while read line; do \
			echo "🔴 $$line"; \
		done; \
		exit 1; \
	fi
else ifdef RAW
	$(MAKE) -k -j$(MAKE_JOBS) $(ALL_OBJECTS)
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
