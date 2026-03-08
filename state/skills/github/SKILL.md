# GitHub Pull Request (using Jujutsu and GitHub CLI)

When you need to push a commit and create a Pull Request on GitHub, you can use a combination of Jujutsu (`jj`) and the GitHub CLI (`gh`).

## Steps

1. **Create a bookmark for the commit**
   Use `jj bookmark create` to name your commit.
   ```bash
   jj bookmark create -r @- <branch-name>
   ```

2. **Track the bookmark and push it**
   Track the bookmark on the remote and push it to `origin`.
   ```bash
   jj bookmark track <branch-name>@origin
   jj git push -b <branch-name>
   ```

3. **Create the Pull Request**
   Use the GitHub CLI (`gh`) to open the pull request against the repository.
   ```bash
   gh pr create --head <branch-name> --title "<PR Title>" --body "<PR Description>"
   ```

## Example
```bash
jj bookmark create -r @- docs-readme-update
jj bookmark track docs-readme-update@origin && jj git push -b docs-readme-update
gh pr create --head docs-readme-update --title "docs: update project description in README" --body "Update the README description to mention historical summaries and embedding-searchable topical history."
```
