# Build Integration

Makefile changes and build dependencies for web-related external tools.

## Overview

All three tools are C programs built from source and installed as external executables:

- `web-search-brave-tool` - Brave Search API client
- `web-search-google-tool` - Google Custom Search API client
- `web-fetch-tool` - HTTP fetcher with HTML→markdown conversion

## Build Target

Add `make tools` target that builds all three external tools.

### Target Definition

```makefile
tools: web-search-brave-tool web-search-google-tool web-fetch-tool

web-search-brave-tool: tools/web-search-brave.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(HTTP_LIBS)

web-search-google-tool: tools/web-search-google.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(HTTP_LIBS)

web-fetch-tool: tools/web-fetch.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(HTTP_LIBS) $(XML_LIBS)
```

### Source Location

Tool source files live in `tools/` directory:

```
tools/
├── web-search-brave.c
├── web-search-google.c
└── web-fetch.c
```

Not in `src/` because these are external executables, not ikigai core code.

## Dependencies

### libxml2

Required for HTML parsing in `web-fetch-tool`.

**Package names:**
- Debian/Ubuntu: `libxml2-dev`
- Fedora/RHEL: `libxml2-devel`
- macOS: `brew install libxml2`
- Arch: `libxml2`

**Compiler flags:**
```makefile
XML_CFLAGS := $(shell pkg-config --cflags libxml-2.0)
XML_LIBS := $(shell pkg-config --libs libxml-2.0)
```

### HTTP Client Library

Decision needed: Which HTTP client library to use?

**Options:**

1. **libcurl**
   - Pros: Mature, widely available, full-featured
   - Cons: Large dependency, more than we need
   - Flags: `$(shell pkg-config --cflags --libs libcurl)`

2. **http-parser + raw sockets**
   - Pros: Minimal, full control
   - Cons: More code to write, TLS handling complex

3. **neon**
   - Pros: WebDAV-focused, good HTTP support
   - Cons: Less common, may not be in all distros

**Recommendation:** libcurl for reliability and TLS handling.

**Compiler flags:**
```makefile
HTTP_CFLAGS := $(shell pkg-config --cflags libcurl)
HTTP_LIBS := $(shell pkg-config --libs libcurl)
```

### JSON Library

Tools need JSON parsing for:
1. Tool input from stdin (request parameters)
2. Tool output to stdout (response data)
3. Credentials file parsing (`~/.config/ikigai/credentials.json`)
4. API response parsing (Brave/Google return JSON)

**Options:**

1. **jsmn** (minimalist)
   - Pros: Single header, no dependencies
   - Cons: Manual parsing, more code

2. **cJSON**
   - Pros: Simple API, easy to use
   - Cons: Additional dependency
   - Flags: `-lcjson`

3. **jansson**
   - Pros: Clean API, good documentation
   - Cons: Less common
   - Flags: `$(shell pkg-config --cflags --libs jansson)`

**Recommendation:** cJSON for simplicity.

**Compiler flags:**
```makefile
JSON_LIBS := -lcjson
```

**Credentials file structure** (see `tool-schemas.md`):
```json
{
  "web_search": {
    "brave": {"api_key": "..."},
    "google": {"api_key": "...", "engine_id": "..."}
  }
}
```

Tools must parse nested JSON to extract credentials.

## Makefile Variables

Add to top of Makefile:

```makefile
# External tool dependencies
XML_CFLAGS := $(shell pkg-config --cflags libxml-2.0)
XML_LIBS := $(shell pkg-config --libs libxml-2.0)
HTTP_CFLAGS := $(shell pkg-config --cflags libcurl)
HTTP_LIBS := $(shell pkg-config --libs libcurl)
JSON_LIBS := -lcjson

# Update CFLAGS to include external tool flags
CFLAGS += $(XML_CFLAGS) $(HTTP_CFLAGS)
```

## Installation

External tools install to `libexec/ikigai/`:

```makefile
install: tools
	install -d $(DESTDIR)$(PREFIX)/libexec/ikigai
	install -m 755 web-search-brave-tool $(DESTDIR)$(PREFIX)/libexec/ikigai/
	install -m 755 web-search-google-tool $(DESTDIR)$(PREFIX)/libexec/ikigai/
	install -m 755 web-fetch-tool $(DESTDIR)$(PREFIX)/libexec/ikigai/
```

**Path resolution:**
- `PREFIX` defaults to `/usr/local`
- System install: `/usr/local/libexec/ikigai/web-search-brave-tool`
- User override: `~/.ikigai/tools/web-search-brave-tool`
- Project override: `.ikigai/tools/web-search-brave-tool`

## Clean Target

Update clean target to remove built tools:

```makefile
clean:
	rm -f $(OBJS) ikigai
	rm -f web-search-brave-tool web-search-google-tool web-fetch-tool
	rm -f tests/unit/*.o tests/integration/*.o
```

## Dependency Checking

Optional: Add dependency checks to fail fast if libraries missing:

```makefile
check-deps:
	@pkg-config --exists libxml-2.0 || (echo "libxml2 not found. Install libxml2-dev" && false)
	@pkg-config --exists libcurl || (echo "libcurl not found. Install libcurl-dev" && false)
	@echo "Dependencies OK"

tools: check-deps
```

## Build Order

Normal build flow:

```bash
make              # Build ikigai (core)
make tools        # Build external tools
make install      # Install both core and tools
```

## Platform Considerations

### Linux
- Standard pkg-config paths work
- Install dev packages via package manager

### macOS
- May need `PKG_CONFIG_PATH` for Homebrew libraries
- Example: `export PKG_CONFIG_PATH=/usr/local/opt/libxml2/lib/pkgconfig`

### BSD
- Similar to Linux, check package names

## Testing Tools

Tools can be tested independently:

```bash
# Test schema output
./web-search-brave-tool --schema

# Test execution
echo '{"query":"test search"}' | ./web-search-brave-tool

# Test with missing credentials (should fail gracefully)
env -u BRAVE_API_KEY ./web-search-brave-tool <<< '{"query":"test"}'
```

## Documentation

Update `project/external-tool-architecture.md` to reference these build instructions.

Update `README.md` to document:
- Build dependencies (libxml2, libcurl, cjson)
- Installation instructions
- Credential configuration

## Implementation Notes

### Library Choice Rationale

**libxml2**: Industry standard, excellent HTML parsing, well-maintained

**libcurl**: De facto standard for HTTP in C, handles TLS, redirects, timeouts automatically

**cJSON**: Lightweight, simple API, sufficient for tool I/O needs

### Alternative: Static Linking

Could statically link dependencies to create standalone tools:

```makefile
LDFLAGS += -static
```

Pros: No runtime dependencies, easier deployment
Cons: Larger binaries, security updates require rebuild

Decision: Dynamic linking preferred for standard distro packages.

### Error Handling

All tools must:
1. Validate JSON input from stdin
2. Return meaningful error messages on stdout
3. Exit with non-zero code on failure
4. Never write to stderr (protocol uses stdout only)

### Timeouts

Tools must respect the 30-second execution timeout enforced by ikigai's external tool framework.

Internal HTTP timeouts should be shorter (e.g., 10 seconds) to allow for retry logic.
