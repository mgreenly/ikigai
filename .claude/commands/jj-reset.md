---
description: Reset jj working copy to fresh state from main@origin
---

Reset the jj working copy to a fresh state:

1. Run `jj git fetch` to fetch latest from remote
2. Run `jj new main@origin` to create a new working set from main
3. List all bookmarks with `jj bookmark list` and delete any bookmarks that are not `main`

Execute these commands now. No arguments needed.
