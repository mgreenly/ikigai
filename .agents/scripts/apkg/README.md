# apkg - Agent Package Manager

Manages installation of agent packages (skills, scripts, commands) from the apkg repository.

## Command

```bash
deno run --allow-net --allow-read --allow-write .agents/scripts/apkg/run.ts <subcommand> [options]
```

## Subcommands

### list

List all available packages from the apkg repository.

**Usage:**
```bash
.agents/scripts/apkg/run.ts list [--verbose]
```

**Options:**
- `--verbose` or `-v`: Show detailed part information for each package

**Returns:**
```json
{
  "success": true,
  "data": {
    "packages": [
      {
        "name": "coverage",
        "description": "Code coverage analysis tools",
        "partCount": 3
      }
    ]
  }
}
```

With `--verbose`:
```json
{
  "success": true,
  "data": {
    "packages": [
      {
        "name": "coverage",
        "description": "Code coverage analysis tools",
        "parts": ["skills/coverage-guru.md", "scripts/coverage", "commands/coverage.md"],
        "partCount": 3
      }
    ]
  }
}
```

### installed

List currently installed files in the `.agents` directory.

**Usage:**
```bash
.agents/scripts/apkg/run.ts installed
```

**Returns:**
```json
{
  "success": true,
  "data": {
    "installed": {
      "skills": ["coverage-guru.md", "default.md"],
      "scripts": ["apkg", "coverage", "task-list"],
      "commands": ["auto-tasks.md", "next-task.md"]
    },
    "total": 7
  }
}
```

### install

Install a package or specific part from the apkg repository.

**Usage:**
```bash
.agents/scripts/apkg/run.ts install <target>
```

**Arguments:**

| Argument | Description | Example |
|----------|-------------|---------|
| target | Package name or type/name | `coverage` or `skills/coverage-guru.md` |

**Installation targets:**
- `<name>`: Install all parts of a package (e.g., `coverage`)
- `<type>/<name>`: Install a specific part (e.g., `skills/coverage-guru.md`)

**Returns:**
```json
{
  "success": true,
  "data": {
    "target": "coverage",
    "mode": "package",
    "installed": [
      {
        "source": "https://raw.githubusercontent.com/mgreenly/apkg/HEAD/agents/skills/coverage-guru.md",
        "destination": "/home/ai4mgreenly/projects/ikigai/main/.agents/skills/coverage-guru.md",
        "size": 2973
      }
    ]
  }
}
```

**Notes:**
- Files are not overwritten if they already exist (size will be 0)
- Use `update` to force re-download

### update

Update (re-download) a package or specific part from the apkg repository.

**Usage:**
```bash
.agents/scripts/apkg/run.ts update <target>
```

**Arguments:**

| Argument | Description | Example |
|----------|-------------|---------|
| target | Package name or type/name | `coverage` or `skills/coverage-guru.md` |

**Returns:**
Same format as `install` command.

**Notes:**
- Forces overwrite of existing files
- Useful for updating to latest version from repository

## Error Handling

All commands return JSON with consistent error format:

```json
{
  "success": false,
  "error": "Error message describing what went wrong",
  "code": "ERROR_CODE"
}
```

**Error codes:**
- `LIST_FAILED`: Failed to fetch or parse manifest
- `INSTALLED_FAILED`: Failed to read local .agents directory
- `PACKAGE_NOT_FOUND`: Requested package not in manifest
- `INVALID_TYPE`: Part type must be skills, scripts, or commands
- `INSTALL_FAILED`: Failed to download or write file
- `MISSING_ARGUMENT`: Required argument not provided
- `INVALID_SUBCOMMAND`: Unknown subcommand

## Examples

List available packages:
```bash
.agents/scripts/apkg/run.ts list
```

Show detailed package information:
```bash
.agents/scripts/apkg/run.ts list --verbose
```

Show what's currently installed:
```bash
.agents/scripts/apkg/run.ts installed
```

Install entire coverage package:
```bash
.agents/scripts/apkg/run.ts install coverage
```

Install just the coverage skill:
```bash
.agents/scripts/apkg/run.ts install skills/coverage-guru.md
```

Update coverage package to latest version:
```bash
.agents/scripts/apkg/run.ts update coverage
```

## Repository

Packages are fetched from: https://github.com/mgreenly/apkg

Manifest URL: https://raw.githubusercontent.com/mgreenly/apkg/HEAD/manifest.json
