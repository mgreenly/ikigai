yolo/plan/edit - mode


## Quality Pipeline Tools

Tools for automated code quality checks. The `/quality` pipeline skips checks if tools are missing.

### Required (apt-get)

| Tool | Package | Purpose |
|------|---------|---------|
| cflow | `cflow` | Call graph analysis, dead code detection |
| ctags | `universal-ctags` | Function/symbol indexing |
| iwyu | `iwyu` | Include-what-you-use, header hygiene |

```bash
sudo apt-get install cflow universal-ctags iwyu
```

### Optional (manual install)

| Tool | Install | Purpose |
|------|---------|---------|
| cpd (PMD) | `~/.local/share/pmd-7.0.0/` | Copy-paste/duplicate code detection |

CPD requires Java 11+. Install:
```bash
cd /tmp
wget https://github.com/pmd/pmd/releases/download/pmd_releases%2F7.0.0/pmd-dist-7.0.0-bin.zip
unzip pmd-dist-7.0.0-bin.zip -d ~/.local/share/
mv ~/.local/share/pmd-bin-7.0.0 ~/.local/share/pmd-7.0.0

# Create wrapper scripts in ~/.local/bin/
cat > ~/.local/bin/pmd << 'EOF'
#!/bin/bash
export PMD_HOME="$HOME/.local/share/pmd-7.0.0"
exec "$PMD_HOME/bin/pmd" "$@"
EOF
chmod +x ~/.local/bin/pmd

cat > ~/.local/bin/cpd << 'EOF'
#!/bin/bash
exec ~/.local/bin/pmd cpd "$@"
EOF
chmod +x ~/.local/bin/cpd
```

### GCC Built-in Analysis

| Flag | Purpose |
|------|---------|
| `-fanalyzer` | Path-sensitive static analysis (null derefs, use-after-free, leaks) |

`-fanalyzer` catches bugs that runtime sanitizers miss (code paths not exercised by tests).
Slow - run as separate CI check, not every build.

```bash
# Test run
make clean && make BUILD=debug CFLAGS="$(CFLAGS) -fanalyzer" 2>&1 | tee analyzer.log
```

### Banned Functions

Use `#pragma GCC poison` to enforce banned function usage at compile time.

Create `src/banned.h` (include from `src/common.h` or similar):

```c
#pragma GCC poison sprintf strcpy strcat gets strncpy strtok
#pragma GCC poison atoi atol atof  /* use strtol/strtod with error checking */
```

Compile-time error if any are used:
```
error: attempt to use poisoned "sprintf"
```

| Banned | Use Instead |
|--------|-------------|
| `sprintf` | `snprintf` |
| `strcpy`, `strcat` | `strlcpy`, `strlcat` or sized variants |
| `gets` | `fgets` |
| `strncpy` | `strlcpy` (strncpy doesn't null-terminate) |
| `strtok` | `strtok_r` (thread-safe) |
| `atoi`, `atol`, `atof` | `strtol`, `strtod` (with error checking) |

### Makefile Targets

| Target | Tool | Description |
|--------|------|-------------|
| `make dead-code` | cflow, ctags | List orphaned functions |
| `make iwyu` | iwyu | (TODO) Check include hygiene |
| `make cpd` | cpd | (TODO) Find duplicate code |
| `make analyze` | gcc -fanalyzer | (TODO) Path-sensitive static analysis |

### Usage Examples

```bash
# Dead code detection
make dead-code

# Include hygiene (per-file)
iwyu src/repl.c

# Duplicate detection (use 'cpp' for C code, exclude vendor/)
cpd --minimum-tokens 50 --language cpp --dir src/ --exclude src/vendor/
```



separate credentials from config

can't change the system message because the config overwrites it when it loads

add a /tool command that shows the tool's available

A /chain command that let's you start a series of prompts, each starting on fresh context after the successful completion of the previous step.

A /loop command that let's you start the same prompt if it is still running when it stops becuase of context exhastion.

Context should de-deuplicate

skill file that are dynamic, when read they actually return up-todate info.

A command that extract all the related context of a given prompt from the history of an agent and record it to a structured StoredAsset that can be referenced.  Example, you've been discussing topic Y and you say /store Y to document X.  This then marks all releative memory as hiden and removes it from the active context.  Sort of like selective compatction.  Thoughts...


when autocompleting /model the individual model names could be presented followed by a short text explain what the model is optimized for

incorporate spell check into the prompt maybe just syntax highlighting words that don't check

