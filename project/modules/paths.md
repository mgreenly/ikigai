# paths

Resolves application directories and translates `ik://` URIs.

## Files

- [src/paths.h](../../src/paths.h)
- [src/paths.c](../../src/paths.c)

## Usage

Initialize from environment variables set by the wrapper script:

```c
ik_paths_t *paths = NULL;
res_t r = ik_paths_init(ctx, &paths);
if (is_err(&r)) {
    // required IKIGAI_*_DIR variable missing
}
```

Get standard directories:

```c
const char *config = ik_paths_get_config_dir(paths);
const char *data = ik_paths_get_data_dir(paths);
// returned strings owned by paths - do NOT free
```

Translate between `ik://` URIs and filesystem paths:

```c
char *fs_path = NULL;
ik_paths_translate_ik_uri_to_path(ctx, paths, "ik://shared/notes.txt", &fs_path);
// fs_path is now something like /home/user/.local/state/ikigai/shared/notes.txt

char *uri = NULL;
ik_paths_translate_path_to_ik_uri(ctx, paths, fs_path, &uri);
// uri is now ik://shared/notes.txt
```

Expand tilde in user-provided paths:

```c
char *expanded = NULL;
ik_paths_expand_tilde(ctx, "~/documents/file.txt", &expanded);
```

## API

```c
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out);

const char *ik_paths_get_bin_dir(ik_paths_t *paths);
const char *ik_paths_get_config_dir(ik_paths_t *paths);
const char *ik_paths_get_data_dir(ik_paths_t *paths);
const char *ik_paths_get_libexec_dir(ik_paths_t *paths);
const char *ik_paths_get_cache_dir(ik_paths_t *paths);
const char *ik_paths_get_state_dir(ik_paths_t *paths);
const char *ik_paths_get_tools_system_dir(ik_paths_t *paths);
const char *ik_paths_get_tools_user_dir(ik_paths_t *paths);
const char *ik_paths_get_tools_project_dir(ik_paths_t *paths);

res_t ik_paths_expand_tilde(TALLOC_CTX *ctx, const char *path, char **out);
res_t ik_paths_translate_ik_uri_to_path(TALLOC_CTX *ctx, ik_paths_t *paths, const char *input, char **out);
res_t ik_paths_translate_path_to_ik_uri(TALLOC_CTX *ctx, ik_paths_t *paths, const char *input, char **out);
```

## Dependencies

- [error](error.md) - result types
