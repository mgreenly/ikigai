/* internal_tools_bg_test.c — unit tests for bg process internal tool handlers */
#include <check.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <talloc.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/internal_tools_bg.h"
#include "shared/error.h"
#include "vendor/yyjson/yyjson.h"

/* Forward declarations for weak symbol overrides */
pid_t forkpty_(int*,char*,struct termios*,struct winsize*);
int pidfd_open_(pid_t,unsigned int); int kill_(pid_t,int);
int setpgid_(pid_t,pid_t); int prctl_(int,unsigned long,unsigned long,unsigned long,unsigned long);
int posix_fcntl_(int,int,int); int posix_close_(int); int posix_mkdir_(const char*,mode_t);
ssize_t posix_write_(int,const void*,size_t); ssize_t posix_pread_(int,void*,size_t,off_t);

static pid_t g_forkpty_pid = 1234;
static int g_forkpty_master = 50, g_pidfd_return = 51;
static int g_kill_last_sig = -1, g_write_count = 0;
static void reset_mocks(void) {
    g_forkpty_pid=1234; g_forkpty_master=50; g_pidfd_return=51;
    g_kill_last_sig=-1; g_write_count=0;
}

pid_t forkpty_(int *m, char *n, struct termios *t, struct winsize *w) {
    (void)n;(void)t;(void)w; if(m)*m=g_forkpty_master; return g_forkpty_pid;
}
int pidfd_open_(pid_t p, unsigned int f) { (void)p;(void)f; return g_pidfd_return; }
int kill_(pid_t p, int s)               { (void)p; g_kill_last_sig=s; return 0; }
int setpgid_(pid_t p, pid_t g)          { (void)p;(void)g; return 0; }
int prctl_(int o,unsigned long a,unsigned long b,unsigned long c,unsigned long d)
    { (void)o;(void)a;(void)b;(void)c;(void)d; return 0; }
int posix_fcntl_(int fd,int c,int a)    { (void)fd;(void)c;(void)a; return 0; }
int posix_close_(int fd)                { (void)fd; return 0; }
int posix_mkdir_(const char *p,mode_t m){ (void)p;(void)m; return 0; }
ssize_t posix_write_(int fd,const void *b,size_t n)
    { (void)fd;(void)b; g_write_count++; return (ssize_t)n; }
ssize_t posix_pread_(int fd,void *b,size_t n,off_t o) { return pread(fd,b,n,o); }

static ik_agent_ctx_t *make_agent(TALLOC_CTX *ctx) {
    ik_agent_ctx_t *a = talloc_zero(ctx, ik_agent_ctx_t);
    a->uuid = talloc_strdup(a,"test-uuid");
    a->bg_manager = bg_manager_create(a);
    return a;
}

static bg_process_t *add_proc(bg_manager_t *mgr, int32_t id, bg_status_t st) {
    bg_process_t *p = talloc_zero(mgr, bg_process_t);
    p->id=id; p->pid=1000+id;
    p->command=talloc_strdup(p,"echo test"); p->label=talloc_strdup(p,"label");
    p->master_fd=-1; p->pidfd=-1; p->output_fd=-1;
    p->status=st; p->stdin_open=(st==BG_STATUS_RUNNING||st==BG_STATUS_STARTING);
    p->ttl_seconds=300;
    clock_gettime(CLOCK_MONOTONIC,&p->started_at);
    p->line_index=bg_line_index_create(p);
    if (mgr->count>=mgr->capacity) {
        int nc=mgr->capacity*2;
        mgr->processes=talloc_realloc(mgr,mgr->processes,bg_process_t*,(unsigned)nc);
        mgr->capacity=nc;
    }
    mgr->processes[mgr->count++]=p;
    return p;
}

/* JSON helpers */
static bool tok(const char *j) {
    yyjson_doc *d=yyjson_read(j,strlen(j),0); if(!d) return false;
    yyjson_val *v=yyjson_obj_get(yyjson_doc_get_root(d),"tool_success");
    bool r=v&&yyjson_is_true(v); yyjson_doc_free(d); return r;
}
static bool ec(const char *j, const char *code) {
    yyjson_doc *d=yyjson_read(j,strlen(j),0); if(!d) return false;
    yyjson_val *v=yyjson_obj_get(yyjson_doc_get_root(d),"error_code");
    bool r=v&&strcmp(yyjson_get_str(v),code)==0; yyjson_doc_free(d); return r;
}
static const char *rs(const char *j,const char *f,char *b,size_t n) {
    yyjson_doc *d=yyjson_read(j,strlen(j),0); b[0]='\0'; if(!d) return b;
    yyjson_val *res=yyjson_obj_get(yyjson_doc_get_root(d),"result");
    yyjson_val *v=res?yyjson_obj_get(res,f):NULL;
    const char *s=v?yyjson_get_str(v):NULL;
    if(s) { strncpy(b,s,n-1); } yyjson_doc_free(d); return b;
}
static int64_t ri(const char *j, const char *f) {
    yyjson_doc *d=yyjson_read(j,strlen(j),0); if(!d) return -999;
    yyjson_val *res=yyjson_obj_get(yyjson_doc_get_root(d),"result");
    yyjson_val *v=res?yyjson_obj_get(res,f):NULL;
    int64_t r=v?yyjson_get_sint(v):-999; yyjson_doc_free(d); return r;
}
static size_t ra(const char *j) {
    yyjson_doc *d=yyjson_read(j,strlen(j),0); if(!d) return 0;
    yyjson_val *res=yyjson_obj_get(yyjson_doc_get_root(d),"result");
    size_t n=res?yyjson_arr_size(res):0; yyjson_doc_free(d); return n;
}
static int tmpfd(char p[64]) {
    strncpy(p,"/tmp/ik_bg_tools_XXXXXX",63);
    int fd=mkstemp(p); if(fd<0) ck_abort_msg("mkstemp"); return fd;
}

/* ---- ps ---- */
START_TEST(test_ps_null_manager) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=talloc_zero(ctx,ik_agent_ctx_t);
    char *r=ik_bg_ps_handler(ctx,a,"{}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_INITIALIZED")); talloc_free(ctx);
}
END_TEST
START_TEST(test_ps_empty_manager) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    char *r=ik_bg_ps_handler(ctx,a,"{}");
    ck_assert(tok(r)); ck_assert_int_eq((int)ra(r),0); talloc_free(ctx);
}
END_TEST
START_TEST(test_ps_returns_process_fields) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    add_proc(a->bg_manager,7,BG_STATUS_RUNNING);
    char *r=ik_bg_ps_handler(ctx,a,"{}");
    ck_assert(tok(r)); ck_assert_int_eq((int)ra(r),1);
    yyjson_doc *doc=yyjson_read(r,strlen(r),0);
    yyjson_val *e=yyjson_arr_get_first(yyjson_obj_get(yyjson_doc_get_root(doc),"result"));
    ck_assert_int_eq((int)yyjson_get_sint(yyjson_obj_get(e,"id")),7);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(e,"status")),"running");
    ck_assert_ptr_nonnull(yyjson_obj_get(e,"age_seconds"));
    ck_assert_ptr_nonnull(yyjson_obj_get(e,"ttl_remaining_seconds"));
    ck_assert_ptr_nonnull(yyjson_obj_get(e,"total_lines"));
    ck_assert_ptr_nonnull(yyjson_obj_get(e,"total_bytes"));
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(e,"command")),"echo test");
    yyjson_doc_free(doc); talloc_free(ctx);
}
END_TEST

/* ---- pstart ---- */
START_TEST(test_pstart_null_manager) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=talloc_zero(ctx,ik_agent_ctx_t);
    char *r=ik_bg_pstart_handler(ctx,a,"{\"command\":\"ls\",\"label\":\"x\",\"ttl_seconds\":60}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_INITIALIZED")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pstart_missing_command) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    char *r=ik_bg_pstart_handler(ctx,a,"{\"label\":\"x\",\"ttl_seconds\":60}");
    ck_assert(!tok(r)); ck_assert(ec(r,"INVALID_ARG")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pstart_success) {
    reset_mocks(); TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    char *r=ik_bg_pstart_handler(ctx,a,
        "{\"command\":\"echo hi\",\"label\":\"test\",\"ttl_seconds\":120}");
    ck_assert(tok(r));
    char buf[32]; ck_assert_str_eq(rs(r,"status",buf,sizeof(buf)),"running");
    ck_assert_int_eq((int)ri(r,"pid"),(int)g_forkpty_pid);
    ck_assert_int_gt((int)ri(r,"id"),0); talloc_free(ctx);
}
END_TEST
START_TEST(test_pstart_concurrency_limit) {
    reset_mocks(); TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    for(int i=0;i<BG_PROCESS_MAX_CONCURRENT;i++) add_proc(a->bg_manager,i+1,BG_STATUS_RUNNING);
    char *r=ik_bg_pstart_handler(ctx,a,"{\"command\":\"ls\",\"label\":\"x\",\"ttl_seconds\":60}");
    ck_assert(!tok(r)); ck_assert(ec(r,"START_FAILED")); talloc_free(ctx);
}
END_TEST

/* ---- pinspect ---- */
START_TEST(test_pinspect_null_manager) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=talloc_zero(ctx,ik_agent_ctx_t);
    char *r=ik_bg_pinspect_handler(ctx,a,"{\"id\":1}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_INITIALIZED")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pinspect_missing_id) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    char *r=ik_bg_pinspect_handler(ctx,a,"{}");
    ck_assert(!tok(r)); ck_assert(ec(r,"INVALID_ARG")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pinspect_process_not_found) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    char *r=ik_bg_pinspect_handler(ctx,a,"{\"id\":99}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_FOUND")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pinspect_returns_fields) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    bg_process_t *proc=add_proc(a->bg_manager,5,BG_STATUS_RUNNING);
    char path[64]; int fd=tmpfd(path);
    const char *text="line1\nline2\n";
    write(fd,text,strlen(text));
    proc->output_fd=fd; proc->total_bytes=(int64_t)strlen(text);
    bg_line_index_append(proc->line_index,(const uint8_t*)text,strlen(text));
    char *r=ik_bg_pinspect_handler(ctx,a,"{\"id\":5}");
    ck_assert(tok(r));
    char buf[32]; ck_assert_str_eq(rs(r,"status",buf,sizeof(buf)),"running");
    ck_assert_int_eq((int)ri(r,"id"),5);
    ck_assert_int_eq((int)ri(r,"total_lines"),2);
    close(fd); unlink(path); talloc_free(ctx);
}
END_TEST
START_TEST(test_pinspect_ansi_stripped) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    bg_process_t *proc=add_proc(a->bg_manager,3,BG_STATUS_RUNNING);
    char path[64]; int fd=tmpfd(path);
    const char *text="\x1b[32mhello\x1b[0m\n";
    write(fd,text,strlen(text));
    proc->output_fd=fd; proc->total_bytes=(int64_t)strlen(text);
    bg_line_index_append(proc->line_index,(const uint8_t*)text,strlen(text));
    char *r=ik_bg_pinspect_handler(ctx,a,"{\"id\":3}");
    ck_assert(tok(r));
    yyjson_doc *doc=yyjson_read(r,strlen(r),0);
    const char *out=yyjson_get_str(yyjson_obj_get(
        yyjson_obj_get(yyjson_doc_get_root(doc),"result"),"output"));
    ck_assert(strstr(out,"\x1b")==NULL);
    ck_assert(strstr(out,"hello")!=NULL);
    yyjson_doc_free(doc); close(fd); unlink(path); talloc_free(ctx);
}
END_TEST
START_TEST(test_pinspect_truncates_at_50kb) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    bg_process_t *proc=add_proc(a->bg_manager,4,BG_STATUS_RUNNING);
    char path[64]; int fd=tmpfd(path);
    /* 200 lines x 260 bytes = 52000 > 51200 (50KB) */
    char line[260]; memset(line,'A',259); line[259]='\n';
    size_t total=0;
    for(int i=0;i<200;i++) {
        write(fd,line,260);
        bg_line_index_append(proc->line_index,(const uint8_t*)line,260);
        total+=260;
    }
    proc->output_fd=fd; proc->total_bytes=(int64_t)total;
    char *r=ik_bg_pinspect_handler(ctx,a,"{\"id\":4,\"tail_lines\":200}");
    ck_assert(tok(r));
    yyjson_doc *doc=yyjson_read(r,strlen(r),0);
    const char *out=yyjson_get_str(yyjson_obj_get(
        yyjson_obj_get(yyjson_doc_get_root(doc),"result"),"output"));
    ck_assert_int_le((int)strlen(out),50*1024);
    yyjson_doc_free(doc); close(fd); unlink(path); talloc_free(ctx);
}
END_TEST

/* ---- pwrite ---- */
START_TEST(test_pwrite_null_manager) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=talloc_zero(ctx,ik_agent_ctx_t);
    char *r=ik_bg_pwrite_handler(ctx,a,"{\"id\":1,\"input\":\"hello\"}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_INITIALIZED")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pwrite_process_not_found) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    char *r=ik_bg_pwrite_handler(ctx,a,"{\"id\":99,\"input\":\"hi\"}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_FOUND")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pwrite_success) {
    reset_mocks(); TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    bg_process_t *proc=add_proc(a->bg_manager,1,BG_STATUS_RUNNING); proc->master_fd=5;
    char *r=ik_bg_pwrite_handler(ctx,a,"{\"id\":1,\"input\":\"hello\"}");
    ck_assert(tok(r)); ck_assert_int_eq((int)ri(r,"id"),1);
    char buf[32]; ck_assert_str_eq(rs(r,"status",buf,sizeof(buf)),"running");
    ck_assert_int_gt(g_write_count,0); talloc_free(ctx);
}
END_TEST
START_TEST(test_pwrite_stdin_closed) {
    reset_mocks(); TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    bg_process_t *proc=add_proc(a->bg_manager,2,BG_STATUS_RUNNING);
    proc->master_fd=5; proc->stdin_open=false;
    char *r=ik_bg_pwrite_handler(ctx,a,"{\"id\":2,\"input\":\"hello\"}");
    ck_assert(!tok(r)); ck_assert(ec(r,"WRITE_FAILED")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pwrite_with_close_stdin) {
    reset_mocks(); TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    bg_process_t *proc=add_proc(a->bg_manager,1,BG_STATUS_RUNNING); proc->master_fd=5;
    char *r=ik_bg_pwrite_handler(ctx,a,"{\"id\":1,\"input\":\"hello\",\"close_stdin\":true}");
    ck_assert(tok(r)); ck_assert(!proc->stdin_open); talloc_free(ctx);
}
END_TEST

/* ---- pkill ---- */
START_TEST(test_pkill_null_manager) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=talloc_zero(ctx,ik_agent_ctx_t);
    char *r=ik_bg_pkill_handler(ctx,a,"{\"id\":1}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_INITIALIZED")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pkill_process_not_found) {
    TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    char *r=ik_bg_pkill_handler(ctx,a,"{\"id\":99}");
    ck_assert(!tok(r)); ck_assert(ec(r,"NOT_FOUND")); talloc_free(ctx);
}
END_TEST
START_TEST(test_pkill_success) {
    reset_mocks(); TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    bg_process_t *proc=add_proc(a->bg_manager,1,BG_STATUS_RUNNING); proc->pid=4242;
    char *r=ik_bg_pkill_handler(ctx,a,"{\"id\":1}");
    ck_assert(tok(r));
    char buf[32]; ck_assert_str_eq(rs(r,"status",buf,sizeof(buf)),"killed");
    ck_assert_int_eq(g_kill_last_sig,SIGTERM); talloc_free(ctx);
}
END_TEST
START_TEST(test_pkill_not_running) {
    reset_mocks(); TALLOC_CTX *ctx=talloc_new(NULL); ik_agent_ctx_t *a=make_agent(ctx);
    add_proc(a->bg_manager,1,BG_STATUS_EXITED);
    char *r=ik_bg_pkill_handler(ctx,a,"{\"id\":1}");
    ck_assert(!tok(r)); ck_assert(ec(r,"KILL_FAILED")); talloc_free(ctx);
}
END_TEST

static Suite *suite(void) {
    Suite *s=suite_create("internal_tools_bg");
    TCase *tc;

    tc=tcase_create("ps");
    tcase_add_test(tc,test_ps_null_manager);
    tcase_add_test(tc,test_ps_empty_manager);
    tcase_add_test(tc,test_ps_returns_process_fields);
    suite_add_tcase(s,tc);

    tc=tcase_create("pstart");
    tcase_add_test(tc,test_pstart_null_manager);
    tcase_add_test(tc,test_pstart_missing_command);
    tcase_add_test(tc,test_pstart_success);
    tcase_add_test(tc,test_pstart_concurrency_limit);
    suite_add_tcase(s,tc);

    tc=tcase_create("pinspect");
    tcase_add_test(tc,test_pinspect_null_manager);
    tcase_add_test(tc,test_pinspect_missing_id);
    tcase_add_test(tc,test_pinspect_process_not_found);
    tcase_add_test(tc,test_pinspect_returns_fields);
    tcase_add_test(tc,test_pinspect_ansi_stripped);
    tcase_add_test(tc,test_pinspect_truncates_at_50kb);
    suite_add_tcase(s,tc);

    tc=tcase_create("pwrite");
    tcase_add_test(tc,test_pwrite_null_manager);
    tcase_add_test(tc,test_pwrite_process_not_found);
    tcase_add_test(tc,test_pwrite_success);
    tcase_add_test(tc,test_pwrite_stdin_closed);
    tcase_add_test(tc,test_pwrite_with_close_stdin);
    suite_add_tcase(s,tc);

    tc=tcase_create("pkill");
    tcase_add_test(tc,test_pkill_null_manager);
    tcase_add_test(tc,test_pkill_process_not_found);
    tcase_add_test(tc,test_pkill_success);
    tcase_add_test(tc,test_pkill_not_running);
    suite_add_tcase(s,tc);

    return s;
}

int32_t main(void) {
    SRunner *sr=srunner_create(suite());
    srunner_set_xml(sr,"reports/check/unit/internal_tools_bg_test.xml");
    srunner_run_all(sr,CK_NORMAL);
    int n=srunner_ntests_failed(sr);
    srunner_free(sr);
    return (n==0)?EXIT_SUCCESS:EXIT_FAILURE;
}
