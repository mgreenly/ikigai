/**
 * @file commands_bg.c
 * @brief Background process slash commands: /ps, /pinspect, /pkill, /pwrite, /pclose
 */

#include "apps/ikigai/commands_bg.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/bg_process_io.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "shared/error.h"
#include "shared/panic.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <talloc.h>

#include "shared/poison.h"

static res_t append_line(ik_repl_ctx_t *repl, const char *line)
{
    return ik_scrollback_append_line(repl->current->scrollback, line, strlen(line));
}

static res_t warn_ok(void *ctx, ik_repl_ctx_t *repl, const char *msg)
{
    char *w = ik_scrollback_format_warning(ctx, msg);
    ik_scrollback_append_line(repl->current->scrollback, w, strlen(w));
    talloc_free(w);
    return OK(NULL);
}

static bg_process_t *find_proc(bg_manager_t *mgr, int32_t id)
{
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->processes[i]->id == id) return mgr->processes[i];
    }
    return NULL;
}

static void fmt_age(char *b, size_t n, int64_t s)
{
    if (s < 60)        snprintf(b, n, "%" PRId64 "s", s);
    else if (s < 3600) snprintf(b, n, "%" PRId64 "m%" PRId64 "s", s/60, s%60);
    else               snprintf(b, n, "%" PRId64 "h%" PRId64 "m", s/3600, (s%3600)/60);
}

static void fmt_size(char *b, size_t n, int64_t bytes)
{
    if (bytes < 1024)         snprintf(b, n, "%" PRId64 "B", bytes);
    else if (bytes < 1048576) snprintf(b, n, "%.1fKB", (double)bytes/1024.0);
    else                      snprintf(b, n, "%.1fMB", (double)bytes/1048576.0);
}

static void fmt_status(char *b, size_t n, const bg_process_t *p)
{
    switch (p->status) {
    case BG_STATUS_STARTING:  snprintf(b, n, "starting");             break;
    case BG_STATUS_RUNNING:   snprintf(b, n, "running");              break;
    case BG_STATUS_EXITED:    snprintf(b, n, "exited(%d)", p->exit_code); break;
    case BG_STATUS_KILLED:    snprintf(b, n, "killed");               break;
    case BG_STATUS_TIMED_OUT: snprintf(b, n, "timed_out");            break;
    case BG_STATUS_FAILED:    snprintf(b, n, "failed");               break;
    default:                  snprintf(b, n, "unknown");              break; // LCOV_EXCL_LINE
    }
}

static bool is_terminal(bg_status_t s)
{
    return s==BG_STATUS_EXITED || s==BG_STATUS_KILLED ||
           s==BG_STATUS_TIMED_OUT || s==BG_STATUS_FAILED;
}

static int64_t proc_age(const bg_process_t *p)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t a = (int64_t)(now.tv_sec - p->started_at.tv_sec);
    return a < 0 ? 0 : a;
}

static void fmt_ttl(char *b, size_t n, const bg_process_t *p, int64_t age)
{
    if (is_terminal(p->status)) { snprintf(b, n, "\xe2\x80\x94"); return; }
    if (p->ttl_seconds < 0)     { snprintf(b, n, "forever");      return; }
    int64_t r = (int64_t)p->ttl_seconds - age;
    fmt_age(b, n, r < 0 ? 0 : r);
}

/* Parse positive integer from start of args. Sets *rest to remainder or NULL.
 * Returns -1 on failure. */
static int32_t parse_id(const char *args, const char **rest)
{
    if (rest) *rest = NULL;
    if (!args || !*args) return -1;
    char *e = NULL;
    long v = strtol(args, &e, 10);
    if (e==args || v<=0 || v>INT32_MAX) return -1;
    while (*e==' ' || *e=='\t') e++;
    if (rest) *rest = *e ? e : NULL;
    return (int32_t)v;
}

static void copy_cmd(const char *cmd, char *b, size_t n)
{
    if (!cmd) { snprintf(b, n, "(null)"); return; }
    size_t len = strlen(cmd);
    if (len+1 <= n) { memcpy(b, cmd, len+1); return; }
    memcpy(b, cmd, n-4);
    memcpy(b+n-4, "...", 4);
}

/* /ps */
res_t ik_cmd_ps(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    (void)args;
    bg_manager_t *mgr = repl->current->bg_manager;
    if (!mgr || mgr->count == 0)
        return warn_ok(ctx, repl, "No background processes.");

    char hdr[128];
    snprintf(hdr, sizeof(hdr), "%-4s %-7s %-12s %-9s %-9s %-10s %s",
             "ID","PID","STATUS","AGE","TTL LEFT","OUTPUT","COMMAND");
    res_t r = append_line(repl, hdr);
    if (is_err(&r)) return r;

    char age[16], ttl[16], sz[12], st[16], cmd[32], line[256];
    for (int i = 0; i < mgr->count; i++) {
        bg_process_t *p = mgr->processes[i];
        int64_t a = proc_age(p);
        fmt_age(age, sizeof(age), a);
        fmt_ttl(ttl, sizeof(ttl), p, a);
        fmt_size(sz, sizeof(sz), p->total_bytes);
        fmt_status(st, sizeof(st), p);
        copy_cmd(p->command, cmd, sizeof(cmd));
        snprintf(line, sizeof(line), "%-4" PRId32 " %-7d %-12s %-9s %-9s %-10s %s",
                 p->id, (int)p->pid, st, age, ttl, sz, cmd);
        r = append_line(repl, line);
        if (is_err(&r)) return r;
    }
    return OK(NULL);
}

/* /pinspect */

typedef enum { PINSPECT_TAIL, PINSPECT_LINES, PINSPECT_SINCE_LAST, PINSPECT_FULL }
    pinspect_mode_t;

typedef struct { int32_t id; pinspect_mode_t mode; int64_t tail_n, start, end; }
    pinspect_args_t;

static bool parse_pinspect(void *ctx, const char *args, pinspect_args_t *o, char **e)
{
    *e = NULL;
    o->mode = PINSPECT_TAIL; o->tail_n = 50; o->start = 1; o->end = -1;
    const char *rest = NULL;
    int32_t id = parse_id(args, &rest);
    if (id < 0) {
        *e = talloc_strdup(ctx,
            "Usage: /pinspect <id> [--tail=N|--lines=S-E|--since-last|--full]");
        return false;
    }
    o->id = id;
    if (!rest) return true;
    if (strncmp(rest, "--tail=", 7)==0) {
        char *p = NULL; long n = strtol(rest+7, &p, 10);
        if (p==rest+7 || n<=0) { *e=talloc_strdup(ctx,"Invalid --tail value: must be a positive integer"); return false; }
        o->mode = PINSPECT_TAIL; o->tail_n = (int64_t)n;
    } else if (strncmp(rest, "--lines=", 8)==0) {
        const char *q = rest+8; char *p = NULL;
        long s = strtol(q, &p, 10);
        if (p==q || *p!='-' || s<=0) { *e=talloc_strdup(ctx,"Invalid --lines value: use --lines=START-END"); return false; }
        q = p+1; long en = strtol(q, &p, 10);
        if (p==q || en<s) { *e=talloc_strdup(ctx,"Invalid --lines range: end must be >= start"); return false; }
        o->mode = PINSPECT_LINES; o->start = (int64_t)s; o->end = (int64_t)en;
    } else if (strcmp(rest, "--since-last")==0) {
        o->mode = PINSPECT_SINCE_LAST;
    } else if (strcmp(rest, "--full")==0) {
        o->mode = PINSPECT_FULL;
    } else {
        *e = talloc_asprintf(ctx, "Unknown option: %s", rest);
        return false;
    }
    return true;
}

res_t ik_cmd_pinspect(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    char *em = NULL; pinspect_args_t pa;
    if (!parse_pinspect(ctx, args, &pa, &em)) return warn_ok(ctx, repl, em);

    bg_manager_t *mgr = repl->current->bg_manager;
    if (!mgr) return warn_ok(ctx, repl, "No background processes.");

    bg_process_t *proc = find_proc(mgr, pa.id);
    if (!proc) {
        char *msg = talloc_asprintf(ctx, "Process %" PRId32 " not found.", pa.id);
        return warn_ok(ctx, repl, msg);
    }

    int64_t age = proc_age(proc);
    char ab[16], sb[16]; fmt_age(ab, sizeof(ab), age); fmt_status(sb, sizeof(sb), proc);
    int64_t tl = bg_line_index_count(proc->line_index);
    char hdr[256];
    snprintf(hdr, sizeof(hdr),
             "Process %" PRId32 ": %s | lines: %" PRId64 " | bytes: %" PRId64 " | age: %s",
             proc->id, sb, tl, proc->total_bytes, ab);
    res_t r = append_line(repl, hdr); if (is_err(&r)) return r;
    r = append_line(repl, "");        if (is_err(&r)) return r;

    if (proc->output_fd < 0 || tl == 0) return append_line(repl, "(no output)");

    bg_read_mode_t mode; int64_t tn=50, sl=1, el=tl;
    switch (pa.mode) {
    case PINSPECT_TAIL:       mode=BG_READ_TAIL;       tn=pa.tail_n;                       break;
    case PINSPECT_LINES:      mode=BG_READ_RANGE;      sl=pa.start; el=pa.end<0?tl:pa.end; break;
    case PINSPECT_SINCE_LAST: mode=BG_READ_SINCE_LAST;                                     break;
    case PINSPECT_FULL:       mode=BG_READ_TAIL;       tn=tl>0?tl:1;                       break;
    default: mode=BG_READ_TAIL; break; // LCOV_EXCL_LINE
    }

    uint8_t *ob = NULL; size_t ol = 0;
    r = bg_process_read_output(proc, ctx, mode, tn, sl, el, &ob, &ol);
    if (is_err(&r)) { talloc_free(r.err); return append_line(repl, "(error reading output)"); }
    if (!ob || ol==0) return append_line(repl, "(no new output)");

    char *p = (char *)ob, *ls = p, *be = p + (ptrdiff_t)ol;
    while (p < be) {
        if (*p=='\n') { *p='\0'; r=append_line(repl,ls); if (is_err(&r)) return r; ls=p+1; }
        p++;
    }
    if (ls < be) { *be='\0'; r=append_line(repl,ls); if (is_err(&r)) return r; }
    return OK(NULL);
}

/* /pkill */
res_t ik_cmd_pkill(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    int32_t id = parse_id(args, NULL);
    if (id < 0) return warn_ok(ctx, repl, "Usage: /pkill <id>");

    bg_manager_t *mgr = repl->current->bg_manager;
    if (!mgr) return warn_ok(ctx, repl, "No background processes.");

    bg_process_t *proc = find_proc(mgr, id);
    if (!proc) {
        char *msg = talloc_asprintf(ctx, "Process %" PRId32 " not found.", id);
        return warn_ok(ctx, repl, msg);
    }
    res_t r = bg_process_kill(proc);
    if (is_err(&r)) {
        char *msg = talloc_asprintf(ctx, "Cannot kill process %" PRId32 ": %s",
                                    id, error_message(r.err));
        talloc_free(r.err);
        return warn_ok(ctx, repl, msg);
    }
    char st[16]; fmt_status(st, sizeof(st), proc);
    char *line = talloc_asprintf(ctx, "Process %" PRId32 " killed. Final status: %s", id, st);
    r = append_line(repl, line);
    talloc_free(line);
    return r;
}

/* /pwrite */

typedef struct { int32_t id; const char *text; bool raw, send_eof; } pwrite_args_t;

static bool parse_pwrite(void *ctx, const char *args, pwrite_args_t *o, char **e)
{
    *e = NULL; o->raw = false; o->send_eof = false; o->text = NULL;
    const char *rest = NULL;
    int32_t id = parse_id(args, &rest);
    if (id < 0 || !rest) {
        *e = talloc_strdup(ctx, "Usage: /pwrite <id> [--raw] [--eof] <text>");
        return false;
    }
    o->id = id;
    const char *p = rest;
    for (;;) {
        if (strncmp(p,"--raw",5)==0 && (p[5]==' '||!p[5])) { o->raw=true; p+=5; while(*p==' ')p++; }
        else if (strncmp(p,"--eof",5)==0 && (p[5]==' '||!p[5])) { o->send_eof=true; p+=5; while(*p==' ')p++; }
        else break;
        if (!*p) { *e=talloc_strdup(ctx,"Usage: /pwrite <id> [--raw] [--eof] <text>"); return false; }
    }
    o->text = p;
    return true;
}

res_t ik_cmd_pwrite(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    char *em = NULL; pwrite_args_t pa;
    if (!parse_pwrite(ctx, args, &pa, &em)) return warn_ok(ctx, repl, em);

    bg_manager_t *mgr = repl->current->bg_manager;
    if (!mgr) return warn_ok(ctx, repl, "No background processes.");

    bg_process_t *proc = find_proc(mgr, pa.id);
    if (!proc) {
        char *msg = talloc_asprintf(ctx, "Process %" PRId32 " not found.", pa.id);
        return warn_ok(ctx, repl, msg);
    }
    res_t r = bg_process_write_stdin(proc, pa.text, strlen(pa.text), !pa.raw);
    if (is_err(&r)) {
        char *msg = talloc_asprintf(ctx, "Failed to write to process %" PRId32 ": %s",
                                    pa.id, error_message(r.err));
        talloc_free(r.err);
        return warn_ok(ctx, repl, msg);
    }
    if (pa.send_eof) {
        r = bg_process_close_stdin(proc);
        if (is_err(&r)) {
            char *msg = talloc_asprintf(ctx, "Failed to close stdin for process %" PRId32 ": %s",
                                        pa.id, error_message(r.err));
            talloc_free(r.err);
            return warn_ok(ctx, repl, msg);
        }
    }
    char *line = talloc_asprintf(ctx, "Written to process %" PRId32 "%s.",
                                 pa.id, pa.send_eof ? " (stdin closed)" : "");
    r = append_line(repl, line);
    talloc_free(line);
    return r;
}

/* /pclose */
res_t ik_cmd_pclose(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    int32_t id = parse_id(args, NULL);
    if (id < 0) return warn_ok(ctx, repl, "Usage: /pclose <id>");

    bg_manager_t *mgr = repl->current->bg_manager;
    if (!mgr) return warn_ok(ctx, repl, "No background processes.");

    bg_process_t *proc = find_proc(mgr, id);
    if (!proc) {
        char *msg = talloc_asprintf(ctx, "Process %" PRId32 " not found.", id);
        return warn_ok(ctx, repl, msg);
    }
    res_t r = bg_process_close_stdin(proc);
    if (is_err(&r)) {
        char *msg = talloc_asprintf(ctx, "Failed to close stdin for process %" PRId32 ": %s",
                                    id, error_message(r.err));
        talloc_free(r.err);
        return warn_ok(ctx, repl, msg);
    }
    char *line = talloc_asprintf(ctx, "Sent EOF to process %" PRId32 " (stdin closed).", id);
    r = append_line(repl, line);
    talloc_free(line);
    return r;
}
