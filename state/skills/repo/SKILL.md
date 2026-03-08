---
name: repo
description: Create repositories using the real ralph-pipeline repo-create script
---

# Repo

Create repositories using Ralph's three-tier git model:

1. GitHub remote repository
2. Bare repo at `/mnt/store/git/<org>/<repo>`
3. Local working clone

This Ikigai skill references the real script in the sibling `ralph-pipeline` checkout.

## Script Location

Use:

- **From ikigai repo root:** `../ralph-pipeline/scripts/repo-create`
- **From this skill directory (`share/skills/repo`):** `../../../../ralph-pipeline/scripts/repo-create`

## Usage

```bash
../ralph-pipeline/scripts/repo-create --org ORG --repo REPO [--private] [--local-path PATH]
```

## Flags

| Flag | Required | Default | Description |
|---|---|---|---|
| `--org` | yes | — | GitHub organization |
| `--repo` | yes | — | Repository name |
| `--private` | no | public | Create a private GitHub repo |
| `--local-path` | no | `~/projects/<repo>` | Local clone path |

## Output

```json
{"ok": true, "org": "foo", "repo": "bar", "github": "foo/bar", "bare": "/mnt/store/git/foo/bar", "local": "/home/user/projects/bar"}
```

## Examples

```bash
../ralph-pipeline/scripts/repo-create --org mgreenly --repo my-app
../ralph-pipeline/scripts/repo-create --org mgreenly --repo my-app --private
../ralph-pipeline/scripts/repo-create --org mgreenly --repo my-app --local-path ~/work/my-app
```

## Pre-flight Expectations

The script validates before making changes, including:

- `gh` authentication
- bare repo path does not already exist
- local path does not already exist
- `/mnt/store/git` is writable

## Error Handling

On failure, inspect the JSON output. It may include a `completed` array showing which steps already succeeded. Do not assume automatic rollback.
