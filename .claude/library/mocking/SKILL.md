---
name: mocking
description: Mocking skill for the ikigai project
---

# Mocking

Mock external dependencies (libraries, system calls, vendor inlines), never our own code.

## The MOCKABLE Pattern

`wrapper.h` provides zero-overhead mocking via weak symbols:

```c
#ifdef NDEBUG
    #define MOCKABLE static inline  // Release: zero overhead
#else
    #define MOCKABLE __attribute__((weak))  // Debug: overridable
#endif

MOCKABLE int posix_write_(int fd, const void *buf, size_t count);
```

## Using Mocks in Tests

```c
#include "wrapper.h"

// Override weak symbol
int posix_write_(int fd, const void *buf, size_t count) {
    return -1;  // Simulate failure
}

void test_write_error_handling(void) {
    res_t r = save_file("test.txt", "content");
    assert(IS_ERR(r));
}
```

## Available Wrappers

### POSIX (syscalls & stdio)
| Wrapper | Wraps |
|---------|-------|
| `posix_open_()` | `open()` |
| `posix_close_()` | `close()` |
| `posix_read_()` | `read()` |
| `posix_write_()` | `write()` |
| `posix_stat_()` | `stat()` |
| `posix_mkdir_()` | `mkdir()` |
| `posix_access_()` | `access()` |
| `posix_rename_()` | `rename()` |
| `posix_getcwd_()` | `getcwd()` |
| `posix_pipe_()` | `pipe()` |
| `posix_fcntl_()` | `fcntl()` |
| `posix_select_()` | `select()` |
| `posix_sigaction_()` | `sigaction()` |
| `posix_ioctl_()` | `ioctl()` |
| `posix_tcgetattr_()` | `tcgetattr()` |
| `posix_tcsetattr_()` | `tcsetattr()` |
| `posix_tcflush_()` | `tcflush()` |
| `posix_fdopen_()` | `fdopen()` |
| `fopen_()` | `fopen()` |
| `fclose_()` | `fclose()` |
| `fread_()` | `fread()` |
| `fwrite_()` | `fwrite()` |
| `fseek_()` | `fseek()` |
| `ftell_()` | `ftell()` |
| `popen_()` | `popen()` |
| `pclose_()` | `pclose()` |
| `opendir_()` | `opendir()` |

### C stdlib
| Wrapper | Wraps |
|---------|-------|
| `snprintf_()` | `snprintf()` |
| `vsnprintf_()` | `vsnprintf()` |
| `gmtime_()` | `gmtime()` |
| `strftime_()` | `strftime()` |

### talloc
| Wrapper | Wraps |
|---------|-------|
| `talloc_zero_()` | `talloc_zero_size()` |
| `talloc_strdup_()` | `talloc_strdup()` |
| `talloc_array_()` | `talloc_zero_size()` |
| `talloc_realloc_()` | `talloc_realloc_size()` |
| `talloc_asprintf_()` | `talloc_vasprintf()` |

### yyjson
| Wrapper | Wraps |
|---------|-------|
| `yyjson_read_file_()` | `yyjson_read_file()` |
| `yyjson_mut_write_file_()` | `yyjson_mut_write_file()` |
| `yyjson_read_()` | `yyjson_read()` |
| `yyjson_doc_get_root_()` | `yyjson_doc_get_root()` |
| `yyjson_obj_get_()` | `yyjson_obj_get()` |
| `yyjson_get_sint_()` | `yyjson_get_sint()` |
| `yyjson_get_str_()` | `yyjson_get_str()` |
| `yyjson_mut_obj_add_str_()` | `yyjson_mut_obj_add_str()` |
| `yyjson_mut_obj_add_int_()` | `yyjson_mut_obj_add_int()` |
| `yyjson_mut_obj_add_uint_()` | `yyjson_mut_obj_add_uint()` |

### libcurl
| Wrapper | Wraps |
|---------|-------|
| `curl_easy_init_()` | `curl_easy_init()` |
| `curl_easy_cleanup_()` | `curl_easy_cleanup()` |
| `curl_easy_perform_()` | `curl_easy_perform()` |
| `curl_easy_setopt_()` | `curl_easy_setopt()` |
| `curl_easy_getinfo_()` | `curl_easy_getinfo()` |
| `curl_easy_strerror_()` | `curl_easy_strerror()` |
| `curl_slist_append_()` | `curl_slist_append()` |
| `curl_slist_free_all_()` | `curl_slist_free_all()` |
| `curl_multi_init_()` | `curl_multi_init()` |
| `curl_multi_cleanup_()` | `curl_multi_cleanup()` |
| `curl_multi_add_handle_()` | `curl_multi_add_handle()` |
| `curl_multi_remove_handle_()` | `curl_multi_remove_handle()` |
| `curl_multi_perform_()` | `curl_multi_perform()` |
| `curl_multi_fdset_()` | `curl_multi_fdset()` |
| `curl_multi_timeout_()` | `curl_multi_timeout()` |
| `curl_multi_info_read_()` | `curl_multi_info_read()` |
| `curl_multi_strerror_()` | `curl_multi_strerror()` |

### libpq & pthread
| Wrapper | Wraps |
|---------|-------|
| `PQgetvalue_()` | `PQgetvalue()` |
| `pq_exec_()` | `PQexec()` |
| `pq_exec_params_()` | `PQexecParams()` |
| `pthread_mutex_init_()` | `pthread_mutex_init()` |
| `pthread_mutex_destroy_()` | `pthread_mutex_destroy()` |
| `pthread_mutex_lock_()` | `pthread_mutex_lock()` |
| `pthread_mutex_unlock_()` | `pthread_mutex_unlock()` |
| `pthread_create_()` | `pthread_create()` |
| `pthread_join_()` | `pthread_join()` |

### Internal ikigai
| Wrapper | Wraps |
|---------|-------|
| `ik_db_init_()` | `ik_db_init()` |
| `ik_db_message_insert_()` | `ik_db_message_insert()` |
| `ik_scrollback_append_line_()` | `ik_scrollback_append_line()` |

## Adding New Wrappers

1. Declare in `wrapper.h`: `MOCKABLE return_type func_(args);`
2. Implement in `wrapper.c`: call original function
3. Replace direct calls with wrapper
4. Override in tests to inject failures

For vendor inlines (like `yyjson_doc_get_root()`), wrap once to contain expansion and enable testing both branches.

## What NOT to Mock

Our own code, pure functions, simple data structures.

## References

- `src/wrapper.h` - Declarations
- `src/wrapper.c` - Implementations
