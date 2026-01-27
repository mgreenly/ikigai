# doc_cache

Caches file contents to avoid repeated filesystem reads.

## Files

- [src/doc_cache.h](../../src/doc_cache.h)
- [src/doc_cache.c](../../src/doc_cache.c)

## Usage

Create a cache with a talloc context and a path resolver:

```c
ik_doc_cache_t *cache = ik_doc_cache_create(ctx, paths);
```

Retrieve document content by path. The cache loads from disk on first access, returns cached content on subsequent calls:

```c
char *content = NULL;
res_t r = ik_doc_cache_get(cache, "ik://config/settings.json", &content);
if (is_err(&r)) {
    // handle file read error
}
// use content - do NOT free it
```

Paths can be `ik://` URIs or filesystem paths. The cache translates URIs internally, so both forms resolve to the same entry.

When a file changes, invalidate its cache entry:

```c
ik_doc_cache_invalidate(cache, "ik://config/settings.json");
```

The cache is freed automatically when its talloc parent is freed. Returned content pointers become invalid at that point.

## API

```c
ik_doc_cache_t *ik_doc_cache_create(TALLOC_CTX *ctx, ik_paths_t *paths);
res_t ik_doc_cache_get(ik_doc_cache_t *cache, const char *path, char **out_content);
void ik_doc_cache_invalidate(ik_doc_cache_t *cache, const char *path);
void ik_doc_cache_clear(ik_doc_cache_t *cache);
```

## Dependencies

- [paths](paths.md) - URI translation
- [file_utils](file_utils.md) - file reading
- [error](error.md) - result types
