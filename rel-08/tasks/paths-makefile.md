# Task: Makefile Wrapper Script Generation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** paths-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
(Baseline skills jj, errors, style, tdd are pre-loaded. Only list additional skills.)
- None additional - this is Makefile work, not C code

**Plan:**
- `rel-08/plan/paths-module.md` - Wrapper script examples and install structure (lines 14-89)

**Source:**
- `Makefile` - Current install target (line 530)
- Review existing PREFIX, bindir, sysconfdir variables

## Libraries

Use only:
- Make - Build system
- Shell (sh) - For wrapper script
- Standard install command

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] `src/paths.c` and `src/paths.h` exist (from paths-core.md task)

## Objective

Update the Makefile to generate a wrapper script during `make install` that sets the correct environment variables (IKIGAI_BIN_DIR, IKIGAI_CONFIG_DIR, IKIGAI_DATA_DIR, IKIGAI_LIBEXEC_DIR) based on PREFIX and execs the actual binary. This enables the paths module to work correctly by reading paths from the environment.

## Install Structure

The new install layout:

```
PREFIX/
├── bin/
│   └── ikigai                    # wrapper script (generated, executable)
├── etc/ikigai/                   # config files (special case for PREFIX=/usr: /etc/ikigai/)
│   └── config.json
├── share/ikigai/                 # data files
└── libexec/ikigai/               # private executables
    └── ikigai                    # actual binary
```

## Makefile Variables to Add

Define additional directory variables following GNU conventions:

| Variable | Default | Purpose |
|----------|---------|---------|
| `PREFIX` | `/usr/local` | Installation prefix (already exists) |
| `bindir` | `$(PREFIX)/bin` | User executables (already exists) |
| `libexecdir` | `$(PREFIX)/libexec` | Private executables (NEW) |
| `datadir` | `$(PREFIX)/share` | Read-only data (NEW) |
| `sysconfdir` | See logic below | System configuration (already exists) |

## sysconfdir Special Logic

The sysconfdir variable needs special handling:

- **PREFIX=/usr**: sysconfdir = `/etc` (NOT `/usr/etc`)
- **PREFIX=/usr/local**: sysconfdir = `/usr/local/etc`
- **PREFIX=/opt/ikigai**: sysconfdir = `/opt/ikigai/etc`
- **All others**: sysconfdir = `$(PREFIX)/etc`

Implementation pattern:
```make
ifeq ($(PREFIX),/usr)
    sysconfdir ?= /etc
else
    sysconfdir ?= $(PREFIX)/etc
endif
```

## Config Directory Special Logic

The config directory (where config.json lives) also needs special handling:

- **PREFIX=/usr**: configdir = `/etc/ikigai`
- **PREFIX=/usr/local**: configdir = `/usr/local/etc/ikigai`
- **PREFIX=/opt/ikigai**: configdir = `/opt/ikigai/etc` (NOT `/opt/ikigai/etc/ikigai`)
- **All others**: configdir = `$(PREFIX)/etc/ikigai`

Pattern for /opt detection:
```make
ifeq ($(PREFIX),/usr)
    configdir = /etc/ikigai
else ifeq ($(findstring /opt/,$(PREFIX)),/opt/)
    # PREFIX starts with /opt/ - use PREFIX/etc not PREFIX/etc/ikigai
    configdir = $(PREFIX)/etc
else
    configdir = $(sysconfdir)/ikigai
endif
```

## Wrapper Script Generation

During `make install`, generate `$(DESTDIR)$(bindir)/ikigai` as a shell script:

**Wrapper script template:**
```bash
#!/bin/sh
IKIGAI_BIN_DIR=<bindir>
IKIGAI_CONFIG_DIR=<configdir>
IKIGAI_DATA_DIR=<datadir>/ikigai
IKIGAI_LIBEXEC_DIR=<libexecdir>/ikigai
export IKIGAI_BIN_DIR IKIGAI_CONFIG_DIR IKIGAI_DATA_DIR IKIGAI_LIBEXEC_DIR
exec <libexecdir>/ikigai/ikigai "$@"
```

Replace `<bindir>`, `<configdir>`, `<datadir>`, `<libexecdir>` with actual resolved paths.

**Implementation approach - Use printf in Makefile:**

```make
install: all
	# Create directories
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(libexecdir)/ikigai
	install -d $(DESTDIR)$(configdir)
	install -d $(DESTDIR)$(datadir)/ikigai

	# Install actual binary to libexec
	install -m 755 $(CLIENT_TARGET) $(DESTDIR)$(libexecdir)/ikigai/ikigai

	# Generate and install wrapper script to bin
	printf '#!/bin/sh\n' > $(DESTDIR)$(bindir)/ikigai
	printf 'IKIGAI_BIN_DIR=%s\n' "$(bindir)" >> $(DESTDIR)$(bindir)/ikigai
	printf 'IKIGAI_CONFIG_DIR=%s\n' "$(configdir)" >> $(DESTDIR)$(bindir)/ikigai
	printf 'IKIGAI_DATA_DIR=%s\n' "$(datadir)/ikigai" >> $(DESTDIR)$(bindir)/ikigai
	printf 'IKIGAI_LIBEXEC_DIR=%s\n' "$(libexecdir)/ikigai" >> $(DESTDIR)$(bindir)/ikigai
	printf 'export IKIGAI_BIN_DIR IKIGAI_CONFIG_DIR IKIGAI_DATA_DIR IKIGAI_LIBEXEC_DIR\n' >> $(DESTDIR)$(bindir)/ikigai
	printf 'exec %s/ikigai/ikigai "$$@"\n' "$(libexecdir)" >> $(DESTDIR)$(bindir)/ikigai
	chmod 755 $(DESTDIR)$(bindir)/ikigai

	# Install config file
	install -m 644 etc/ikigai/config.json $(DESTDIR)$(configdir)/config.json
```

## Uninstall Target Updates

Update uninstall to remove new locations:

```make
uninstall:
	rm -f $(DESTDIR)$(bindir)/ikigai
	rm -f $(DESTDIR)$(libexecdir)/ikigai/ikigai
	rmdir $(DESTDIR)$(libexecdir)/ikigai 2>/dev/null || true
	rmdir $(DESTDIR)$(libexecdir) 2>/dev/null || true
	rm -f $(DESTDIR)$(configdir)/config.json
	rmdir $(DESTDIR)$(configdir) 2>/dev/null || true
	rmdir $(DESTDIR)$(datadir)/ikigai 2>/dev/null || true
```

## Test Implementation

**This is build infrastructure, not C code. Testing follows a different pattern:**

### Manual Verification Tests

**Test 1: Install to /tmp and verify wrapper script**

```bash
# Clean install
make clean
make all

# Install to test location
make install PREFIX=/tmp/ikigai_test DESTDIR=

# Verify directory structure
test -f /tmp/ikigai_test/bin/ikigai
test -f /tmp/ikigai_test/libexec/ikigai/ikigai
test -x /tmp/ikigai_test/bin/ikigai

# Verify wrapper script content
grep -q "IKIGAI_BIN_DIR=/tmp/ikigai_test/bin" /tmp/ikigai_test/bin/ikigai
grep -q "IKIGAI_CONFIG_DIR=/tmp/ikigai_test/etc/ikigai" /tmp/ikigai_test/bin/ikigai
grep -q "IKIGAI_DATA_DIR=/tmp/ikigai_test/share/ikigai" /tmp/ikigai_test/bin/ikigai
grep -q "IKIGAI_LIBEXEC_DIR=/tmp/ikigai_test/libexec/ikigai" /tmp/ikigai_test/bin/ikigai
grep -q "exec /tmp/ikigai_test/libexec/ikigai/ikigai" /tmp/ikigai_test/bin/ikigai

# Cleanup
rm -rf /tmp/ikigai_test
```

**Test 2: Verify PREFIX=/usr special case**

```bash
# Install to test location with PREFIX=/usr
make install PREFIX=/usr DESTDIR=/tmp/ikigai_usr_test

# Verify /etc not /usr/etc
grep -q "IKIGAI_CONFIG_DIR=/etc/ikigai" /tmp/ikigai_usr_test/usr/bin/ikigai
test ! -f /tmp/ikigai_usr_test/usr/etc/ikigai/config.json
test -f /tmp/ikigai_usr_test/etc/ikigai/config.json

# Cleanup
rm -rf /tmp/ikigai_usr_test
```

**Test 3: Verify uninstall works**

```bash
# Install then uninstall
make install PREFIX=/tmp/ikigai_uninstall_test DESTDIR=
make uninstall PREFIX=/tmp/ikigai_uninstall_test DESTDIR=

# Verify files removed
test ! -f /tmp/ikigai_uninstall_test/bin/ikigai
test ! -f /tmp/ikigai_uninstall_test/libexec/ikigai/ikigai

# Cleanup
rm -rf /tmp/ikigai_uninstall_test
```

## Behaviors

### Wrapper Script Behavior

When executed, the wrapper script:
1. Sets 4 environment variables with absolute paths
2. Exports them so child processes can read
3. Execs (replaces itself with) the actual binary at `$(libexecdir)/ikigai/ikigai`
4. Passes all arguments through via `"$@"`

### Make Install Behavior

`make install` should:
1. Build all targets first (depends on `all`)
2. Create all necessary directories
3. Install actual binary to `$(DESTDIR)$(libexecdir)/ikigai/ikigai`
4. Generate wrapper script to `$(DESTDIR)$(bindir)/ikigai`
5. Make wrapper executable (chmod 755)
6. Install config.json to `$(DESTDIR)$(configdir)/config.json`

### DESTDIR Support

DESTDIR is used for staged installs (packaging):
- `DESTDIR=/tmp/stage make install PREFIX=/usr/local`
- Files go to `/tmp/stage/usr/local/...`
- But paths in wrapper script are still `/usr/local/...` (not `/tmp/stage/usr/local/...`)

## Implementation Steps

1. **Add new Makefile variables** (after existing PREFIX/bindir/sysconfdir):
   - `libexecdir ?= $(PREFIX)/libexec`
   - `datadir ?= $(PREFIX)/share`
   - Add sysconfdir special logic for PREFIX=/usr
   - Add configdir variable with /opt detection

2. **Update install target**:
   - Create directories: bindir, libexecdir/ikigai, configdir, datadir/ikigai
   - Install binary to libexecdir/ikigai/ikigai (not bindir)
   - Generate wrapper script using printf
   - chmod 755 wrapper script
   - Install config.json to configdir

3. **Update uninstall target**:
   - Remove wrapper from bindir
   - Remove binary from libexecdir/ikigai
   - Clean up empty directories

4. **Test manually** (see Test Implementation section above)

5. **Run make check** to ensure nothing broke

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(paths-makefile.md): success - implemented wrapper script generation

Makefile now generates wrapper script during install that sets IKIGAI_*_DIR
environment variables. Binary installed to libexec, wrapper to bin.
Special handling for PREFIX=/usr (/etc not /usr/etc) and /opt prefixes.
Tested with manual verification - wrapper script correct, install/uninstall work.
EOF
)"
```

Report status to orchestration:
- Success: `/task-done paths-makefile`
- Partial/Failed: `/task-fail paths-makefile`

## Postconditions

- [ ] Makefile compiles without errors
- [ ] New variables defined: libexecdir, datadir, configdir
- [ ] sysconfdir has special logic for PREFIX=/usr
- [ ] configdir has special logic for /opt detection
- [ ] install target generates wrapper script
- [ ] install target installs binary to libexecdir/ikigai/ikigai
- [ ] install target installs wrapper to bindir/ikigai
- [ ] Wrapper script is executable (755 permissions)
- [ ] Wrapper script sets all 4 IKIGAI_*_DIR variables correctly
- [ ] Wrapper script execs actual binary with "$@"
- [ ] Manual test 1 passes (basic install verification)
- [ ] Manual test 2 passes (PREFIX=/usr special case)
- [ ] Manual test 3 passes (uninstall cleanup)
- [ ] `make check` still passes (build not broken)
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
