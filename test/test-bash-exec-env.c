/*
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * test-bash-exec-env.c
 *
 * Reproducer for https://bugzilla.yoctoproject.org/show_bug.cgi?id=16078
 *
 * What this binary simulates
 * --------------------------
 * bash exports getenv(), setenv(), and unsetenv() as dynamic symbols.
 * Because the main executable appears first in the dynamic-linker search
 * order, every library loaded into the process (including libpseudo.so)
 * resolves calls to those names to bash's own implementations.
 *
 * bash maintains a private variable hash table that is separate from the
 * C-library 'environ' array.  bash's setenv/getenv operate on this table;
 * 'environ' is only rebuilt lazily by maybe_make_export_env().
 *
 * The old pseudo workaround (commit 80c6334a) used
 *   pseudo_real_setenv = dlsym(RTLD_NEXT, "setenv")
 * to bypass bash's override and call glibc's setenv directly.
 * dlsym(RTLD_NEXT, …) from within an LD_PRELOAD library searches the
 * shared-library chain *after* the LD_PRELOAD library, so it finds
 * glibc's implementation — it does NOT find the main executable's
 * (bash's) version.
 *
 * glibc's setenv modifies 'environ' directly, allocating a new heap
 * block (block A) for the updated "LD_PRELOAD=libpseudo.so:…" entry.
 * bash's internal variable table is NOT updated.
 *
 * When bash later calls maybe_make_export_env() (before exec-ing each
 * pipeline stage), it calls strvec_flush() which frees every entry in
 * the current 'environ' array — including block A — and rebuilds from
 * the internal table (which still has the original LD_PRELOAD without
 * libpseudo.so).
 *
 * The freed block A therefore remains live in the allocator's free list
 * while pseudo's exec wrapper (pseudo_setupenvp) iterates 'environ'.
 * Whether the access to freed memory produces a crash depends on whether
 * the allocator happens to reuse that block and overwrite its contents
 * before pseudo reads from it.
 *
 * This binary faithfully reproduces the bash conditions:
 *
 *   1.  It exports its own setenv/getenv/unsetenv (like bash) that
 *       maintain a private internal table, separate from 'environ'.
 *       The regular setenv/getenv calls in pseudo therefore go to
 *       *our* functions; only dlsym(RTLD_NEXT, …) from inside
 *       libpseudo.so reaches glibc's implementations.
 *
 *   2.  It implements strvec_flush() and maybe_make_export_env() that
 *       rebuild 'environ' from the internal table, freeing the old
 *       array (which contains glibc-setenv-allocated blocks).
 *
 *   3.  It runs the pipeline pattern from opkg-build: fork N children
 *       in parallel (simulating find|sort|tar|gzip|ar), each child
 *       calling maybe_make_export_env() then execve().  This is the
 *       exact sequence that triggered the production crash.
 *
 * Detection
 * ---------
 * The bug is detected deterministically rather than by relying on the
 * probabilistic heap corruption it causes.  main() strips the derived
 * PSEUDO_* variables (PSEUDO_BINDIR, PSEUDO_LIBDIR, …) from both the
 * internal table and the real 'environ'.  pseudo's fork wrapper then
 * runs pseudo_setupenv() in every child, which re-adds them:
 *
 *   Unfixed pseudo calls glibc's setenv (via dlsym(RTLD_NEXT, …)),
 *   which writes the variables straight back into the real 'environ'
 *   array — the very environ/allocator desync that corrupts bash's
 *   heap in production.  The child observes the reappearing PSEUDO_*
 *   entries and reports the bug.
 *
 *   Fixed pseudo calls the process's own setenv (ours/bash's), which
 *   updates only the internal table, and the exec/system/popen
 *   wrappers operate on a private pseudo_setupenvp() copy and restore
 *   environ afterwards.  The real 'environ' is never mutated, so the
 *   PSEUDO_* variables do not reappear and the child exits cleanly.
 *
 * See pseudo_mutated_environ() below for the check.  MALLOC_CHECK_=3 is
 * still set by the shell wrapper as a secondary safety net so that any
 * residual heap corruption also aborts the run.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Internal variable table — analogous to bash's variable hash table.  */
/* setenv/getenv/unsetenv operate on this table; 'environ' is rebuilt   */
/* on demand by maybe_make_export_env().                                */
/* ------------------------------------------------------------------ */

static char **internal_table = NULL;  /* "KEY=VALUE" strings           */
static int    internal_count  = 0;
static int    internal_cap    = 0;

static int table_find(const char *name, size_t nlen)
{
    for (int i = 0; i < internal_count; i++) {
        if (strncmp(internal_table[i], name, nlen) == 0 &&
            internal_table[i][nlen] == '=')
            return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Exported env functions — these override glibc's versions for code   */
/* that resolves symbols via the normal dynamic-linker search order    */
/* (i.e., callers inside bash/this binary itself).                     */
/*                                                                      */
/* dlsym(RTLD_NEXT, "setenv") from an LD_PRELOAD library searches      */
/* *after* the LD_PRELOAD library, finding glibc's implementation —    */
/* NOT these functions.  That is the exact asymmetry that pseudo's old  */
/* workaround relied on and that caused the bug.                        */
/*                                                                      */
/* The defensive NULL checks below intentionally mirror bash's own      */
/* env functions.  glibc declares setenv/getenv/unsetenv with the       */
/* 'nonnull' attribute, so comparing the parameters to NULL trips       */
/* -Wnonnull-compare; suppress it for this block since keeping the      */
/* guards is deliberate.                                                */
/* ------------------------------------------------------------------ */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"

int setenv(const char *name, const char *value, int overwrite)
{
    if (!name || !value || strchr(name, '=')) { errno = EINVAL; return -1; }
    size_t nlen = strlen(name);
    size_t vlen = strlen(value);

    /* Before main() initialises internal_table, update environ directly. */
    if (!internal_table) {
        /* Find and replace existing entry, or append if not found. */
        if (environ) {
            for (int i = 0; environ[i]; i++) {
                if (strncmp(environ[i], name, nlen) == 0 &&
                    environ[i][nlen] == '=') {
                    if (!overwrite) return 0;
                    char *e = malloc(nlen + 1 + vlen + 1);
                    if (!e) return -1;
                    memcpy(e, name, nlen); e[nlen] = '=';
                    memcpy(e + nlen + 1, value, vlen + 1);
                    free(environ[i]); environ[i] = e;
                    return 0;
                }
            }
        }
        /* Entry not found: can't resize kernel-provided environ safely;
         * just ignore (the entry will be added when main() starts). */
        return 0;
    }

    int idx = table_find(name, nlen);
    if (idx >= 0) {
        if (!overwrite) return 0;
        free(internal_table[idx]);
        internal_table[idx] = NULL;
    } else {
        /* Grow table if needed (keep a NULL sentinel). */
        if (internal_count + 1 >= internal_cap) {
            internal_cap = (internal_cap < 8) ? 16 : internal_cap * 2;
            char **t = realloc(internal_table,
                               (size_t)internal_cap * sizeof(char *));
            if (!t) return -1;
            internal_table = t;
        }
        idx = internal_count++;
        internal_table[internal_count] = NULL;  /* sentinel */
    }

    char *entry = malloc(nlen + 1 + vlen + 1);
    if (!entry) return -1;
    memcpy(entry, name, nlen);
    entry[nlen] = '=';
    memcpy(entry + nlen + 1, value, vlen + 1);
    internal_table[idx] = entry;
    return 0;
}

char *getenv(const char *name)
{
    if (!name) return NULL;
    size_t nlen = strlen(name);
    /*
     * Before main() initialises internal_table, fall back to searching
     * 'environ' directly.  This is needed because pseudo's library
     * constructor (which runs before main()) calls getenv() to read
     * PSEUDO_PREFIX and similar variables.
     */
    if (!internal_table) {
        for (int i = 0; environ && environ[i]; i++) {
            if (strncmp(environ[i], name, nlen) == 0 &&
                environ[i][nlen] == '=')
                return environ[i] + nlen + 1;
        }
        return NULL;
    }
    int idx = table_find(name, nlen);
    if (idx < 0) return NULL;
    return internal_table[idx] + nlen + 1;
}

int unsetenv(const char *name)
{
    if (!name || !*name || strchr(name, '=')) { errno = EINVAL; return -1; }
    size_t nlen = strlen(name);
    /* Before internal_table is initialised, remove from environ directly. */
    if (!internal_table) {
        if (environ) {
            for (int i = 0; environ[i]; i++) {
                if (strncmp(environ[i], name, nlen) == 0 &&
                    environ[i][nlen] == '=') {
                    free(environ[i]);
                    int j = i;
                    while (environ[j]) { environ[j] = environ[j+1]; j++; }
                    break;
                }
            }
        }
        return 0;
    }
    int idx = table_find(name, nlen);
    if (idx < 0) return 0;
    free(internal_table[idx]);
    memmove(&internal_table[idx], &internal_table[idx + 1],
            (size_t)(internal_count - idx) * sizeof(char *));
    internal_count--;
    return 0;
}

#pragma GCC diagnostic pop

/* ------------------------------------------------------------------ */
/* strip_asan_from_preload — remove any libasan*.so entry from an      */
/* LD_PRELOAD value string (colon-separated list).                     */
/*                                                                      */
/* The ASAN library must be first in LD_PRELOAD so that ASAN properly  */
/* intercepts malloc/free from all shared libraries (including         */
/* libpseudo.so).  But the pseudo server-spawn grandchild must NOT     */
/* inherit it: the server is not ASAN-compiled and loading the ASAN    */
/* runtime into an uninstrumented binary causes SQLite lock failures.  */
/*                                                                      */
/* By stripping libasan from the rebuilt 'environ' (produced by        */
/* maybe_make_export_env), the server inherits clean LD_PRELOAD while  */
/* ASAN detection in the direct child (fork of our binary) remains    */
/* active — ASAN is compiled into the binary and its hooks are already */
/* live from process startup.                                           */
/* ------------------------------------------------------------------ */
static char *strip_asan_from_preload(const char *preload)
{
    if (!preload) return NULL;
    char *buf = strdup(preload);
    if (!buf) return NULL;

    char *out = buf;
    const char *in  = preload;
    while (*in) {
        const char *sep = strchr(in, ':');
        size_t len = sep ? (size_t)(sep - in) : strlen(in);

        /* skip any entry that contains "libasan" */
        int skip = 0;
        for (size_t k = 0; k + 6 <= len; k++) {
            if (strncmp(in + k, "libasan", 7) == 0) { skip = 1; break; }
        }

        if (!skip) {
            memcpy(out, in, len);
            out += len;
            if (sep) *out++ = ':';
        }

        in += len;
        if (*in == ':') in++;
    }
    /* trim a trailing colon */
    if (out > buf && *(out - 1) == ':') out--;
    *out = '\0';
    return buf;
}

/* ------------------------------------------------------------------ */
/* strvec_flush — free every string in v[] then free v itself.         */
/* Equivalent to bash's strvec_flush() in lib/sh/stringvec.c.         */
/* ------------------------------------------------------------------ */
static void strvec_flush(char **v)
{
    if (!v) return;
    for (int i = 0; v[i]; i++)
        free(v[i]);
    free(v);
}

/* ------------------------------------------------------------------ */
/* maybe_make_export_env — rebuild 'environ' from the internal table.  */
/*                                                                      */
/* This is the function that triggers the use-after-free:              */
/*                                                                      */
/*   1. pseudo's fork wrapper called glibc's setenv (via               */
/*      dlsym(RTLD_NEXT)) which allocated block A and stored it in     */
/*      environ[LD_PRELOAD_idx].                                        */
/*                                                                      */
/*   2. strvec_flush(old_environ) frees block A.                       */
/*                                                                      */
/*   3. environ is updated to point to the freshly built array         */
/*      (built from internal_table, which has the original LD_PRELOAD  */
/*      without libpseudo.so).                                         */
/*                                                                      */
/*   4. When the exec wrapper runs pseudo_setupenvp(environ), the      */
/*      allocator may have already reused block A, causing the freed   */
/*      memory to be read with stale or garbage content.               */
/* ------------------------------------------------------------------ */
static void maybe_make_export_env(void)
{
    char **old = environ;

    /* Build fresh copy from internal_table. */
    char **new_env = malloc((size_t)(internal_count + 1) * sizeof(char *));
    if (!new_env) return;

    for (int i = 0; i < internal_count; i++) {
        new_env[i] = strdup(internal_table[i]);
        if (!new_env[i]) {
            for (int j = 0; j < i; j++) free(new_env[j]);
            free(new_env);
            return;
        }
    }
    new_env[internal_count] = NULL;

    /* Strip the ASAN runtime library from LD_PRELOAD in BOTH the rebuilt
     * environ AND internal_table so that the pseudo server-spawn grandchild
     * does not inherit it (via pseudo_setupenv reading internal_table).
     *
     * ASAN detection in the current process is unaffected: libasan.so is
     * already mapped in the process address space (loaded at startup via the
     * original LD_PRELOAD) and its malloc/free interception is active at the
     * GOT/PLT level for all shared libraries including libpseudo.so.
     * Removing it from environ/internal_table only affects exec'd children. */
    const char *ldp = "LD_PRELOAD=";
    size_t ldp_len = strlen(ldp);
    for (int i = 0; i < internal_count; i++) {
        if (strncmp(new_env[i], ldp, ldp_len) == 0) {
            char *stripped = strip_asan_from_preload(new_env[i] + ldp_len);
            if (stripped) {
                char *new_entry = malloc(ldp_len + strlen(stripped) + 1);
                if (new_entry) {
                    memcpy(new_entry, ldp, ldp_len);
                    strcpy(new_entry + ldp_len, stripped);
                    free(new_env[i]);
                    new_env[i] = new_entry;
                }
                free(stripped);
            }
            break;
        }
    }
    /* Also strip from internal_table so pseudo_setupenv() doesn't
     * re-inject libasan into environ via SETENV(). */
    for (int i = 0; i < internal_count; i++) {
        if (strncmp(internal_table[i], ldp, ldp_len) == 0) {
            char *stripped = strip_asan_from_preload(internal_table[i] + ldp_len);
            if (stripped) {
                char *new_entry = malloc(ldp_len + strlen(stripped) + 1);
                if (new_entry) {
                    memcpy(new_entry, ldp, ldp_len);
                    strcpy(new_entry + ldp_len, stripped);
                    free(internal_table[i]);
                    internal_table[i] = new_entry;
                }
                free(stripped);
            }
            break;
        }
    }

    /*
     * Free the old environ.  This releases every entry including any
     * block that glibc's setenv (called by pseudo_setupenv inside the
     * fork wrapper) had allocated.  After this, 'environ' briefly
     * points to freed memory until the assignment below.
     */
    strvec_flush(old);

    environ = new_env;
}

/* ------------------------------------------------------------------ */
/* run_pipeline_iteration                                               */
/*                                                                      */
/* Simulate one opkg-build pipeline:                                   */
/*   ( cd CTRL && find . | sort > list )                               */
/*   ( cd PKG  && find . | sort > list )                               */
/*   ( cd PKG  && tar  … | gzip  > data.tar.gz )                      */
/*   ( cd CTRL && tar  … | gzip  > ctrl.tar.gz )                      */
/*   ( cd TMP  && ar …               )                                 */
/*                                                                      */
/* Each child simulates one pipeline stage:                            */
/*   fork → [pseudo fork wrapper runs pseudo_setupenv() in the child]  */
/*   child: check whether pseudo mutated the real environ (bug), then  */
/*          maybe_make_export_env() and execve("/bin/true")            */
/* ------------------------------------------------------------------ */
#define PIPELINE_STAGES 5   /* find, sort, tar, gzip, ar */

/* Distinct exit code a child uses to report that pseudo mutated the
 * real 'environ' array (i.e. the unfixed glibc-setenv code path ran). */
#define BUG_ENV_MUTATED 42

/* ------------------------------------------------------------------ */
/* pseudo_mutated_environ — deterministic detector for the bug fixed   */
/* by commits 9c6b4c1..5abd42c.                                        */
/*                                                                      */
/* pseudo's fork wrapper runs pseudo_setupenv() in every child before   */
/* fork() returns.  pseudo_setupenv() re-adds the PSEUDO_* variables    */
/* (PSEUDO_BINDIR, PSEUDO_LIBDIR, …) that main() stripped from the      */
/* environment:                                                         */
/*                                                                      */
/*   Unfixed pseudo:  SETENV -> pseudo_real_setenv (dlsym RTLD_NEXT) -> */
/*     glibc setenv, which writes directly into the real 'environ'      */
/*     array.  The stripped PSEUDO_* vars therefore REAPPEAR in the     */
/*     real environ — exactly the environ/allocator desync that         */
/*     corrupts bash's heap in production.                              */
/*                                                                      */
/*   Fixed pseudo:  SETENV -> setenv -> our (bash's) setenv, which only */
/*     updates internal_table; the exec/system/popen wrappers build a   */
/*     private copy with pseudo_setupenvp() and restore environ.  The   */
/*     real 'environ' is left untouched, so the stripped PSEUDO_* vars  */
/*     do NOT reappear there.                                           */
/*                                                                      */
/* Returns 1 if a stripped PSEUDO_* var was found back in the real      */
/* environ (buggy code path), 0 otherwise.  Must be called BEFORE       */
/* maybe_make_export_env(), which would otherwise rebuild environ from  */
/* internal_table and discard the injected entries.                     */
/* ------------------------------------------------------------------ */
static int pseudo_mutated_environ(void)
{
    for (int i = 0; environ && environ[i]; i++) {
        if (strncmp(environ[i], "PSEUDO_BINDIR=", 14) == 0 ||
            strncmp(environ[i], "PSEUDO_LIBDIR=", 14) == 0)
            return 1;
    }
    return 0;
}

static int run_pipeline_iteration(void)
{
    pid_t pids[PIPELINE_STAGES];
    char *const args[] = { "/bin/true", NULL };

    /*
     * Fork all pipeline stages before waiting for any of them, just
     * as bash does for a pipeline.  pseudo's fork wrapper runs
     * pseudo_setupenv() in each child before fork() returns.
     */
    for (int s = 0; s < PIPELINE_STAGES; s++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }
        if (pid == 0) {
            /*
             * Child: pseudo's fork wrapper has already run
             * pseudo_setupenv() at this point.  Deterministically
             * detect whether it mutated the real environ (the bug)
             * before doing anything that would rebuild environ.
             */
            if (pseudo_mutated_environ())
                _exit(BUG_ENV_MUTATED);
            /*
             * Otherwise behave like bash: rebuild environ from the
             * internal table and exec the pipeline stage.
             */
            maybe_make_export_env();
            execve("/bin/true", args, environ);
            _exit(127);
        }
        pids[s] = pid;
    }

    /* Wait for all pipeline children. */
    for (int s = 0; s < PIPELINE_STAGES; s++) {
        int status;
        if (waitpid(pids[s], &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == BUG_ENV_MUTATED) {
            fprintf(stderr,
                "BUG: pseudo mutated the real environ in pipeline stage %d "
                "(pseudo_setupenv used glibc setenv via dlsym instead of the "
                "process's own setenv)\n", s);
            return -1;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "pipeline stage %d failed: status=%d\n",
                    s, status);
            return -1;
        }
    }
    return 0;
}

int main(void)
{
    /* Initialise internal_table from the process's current environ. */
    int count = 0;
    while (environ[count]) count++;

    internal_cap = count + 32;
    internal_table = malloc((size_t)internal_cap * sizeof(char *));
    if (!internal_table) { perror("malloc"); return 1; }

    for (int i = 0; i < count; i++) {
        internal_table[i] = strdup(environ[i]);
        if (!internal_table[i]) { perror("strdup"); return 1; }
    }
    internal_table[count] = NULL;
    internal_count = count;

    /*
     * Replace environ with a heap-allocated copy before any call to
     * maybe_make_export_env().  The initial environ supplied by the
     * kernel exec is in static memory, not the heap; calling free()
     * on those strings (as strvec_flush does) would crash immediately.
     * bash avoids this by building its own heap-allocated export_env
     * during shell initialisation; we replicate that here.
     */
    {
        char **heap_env = malloc((size_t)(count + 1) * sizeof(char *));
        if (!heap_env) { perror("malloc"); return 1; }
        for (int i = 0; i < count; i++) {
            heap_env[i] = strdup(internal_table[i]);
            if (!heap_env[i]) { perror("strdup"); return 1; }
        }
        heap_env[count] = NULL;
        environ = heap_env;   /* now environ is entirely heap-managed */
    }

    /*
     * Run 20 pipeline iterations.  With the unfixed pseudo code and
     * MALLOC_CHECK_=3, the heap corruption introduced by pseudo_setupenv
     * (glibc setenv) followed by strvec_flush causes an abort within
     * the first few iterations on affected platforms.
     */

    /*
     * Strip the ASAN runtime library from LD_PRELOAD and strip derived
     * PSEUDO_* vars (PSEUDO_BINDIR, PSEUDO_LIBDIR, etc.) from both
     * internal_table and heap environ BEFORE any pseudo-intercepted call.
     *
     * Reason for stripping ASAN from LD_PRELOAD:
     *   The pseudo server-spawn grandchild is not ASAN-compiled; loading the
     *   ASAN runtime into an uninstrumented binary causes SQLite lock failures.
     *
     * Reason for stripping derived PSEUDO_* vars:
     *   When the test runs inside the pseudo test framework, the outer pseudo's
     *   exec wrapper pre-populates all PSEUDO_* vars (PSEUDO_BINDIR etc.) in
     *   the binary's initial environ via pseudo_setupenvp().  If these are
     *   already present, pseudo_setupenv() treats them as existing (overwrite=0)
     *   and makes no change — no __add_to_environ() call, no heap-corruption
     *   opportunity, MALLOC_CHECK_=3 detects nothing.
     *   By stripping them here we force pseudo_setupenv() (in the fork wrapper
     *   of every child) to ADD them as NEW environ entries.  The unfixed code
     *   (glibc setenv via dlsym RTLD_NEXT) calls __add_to_environ() which
     *   calls realloc(environ_array).  strvec_flush() later frees that
     *   allocation, and MALLOC_CHECK_=3 aborts the server-spawn grandchild.
     *   The fixed code calls our binary's setenv() which updates internal_table
     *   only — no __add_to_environ(), no realloc, no corruption.
     *
     * We keep PSEUDO_PREFIX and PSEUDO_LOCALSTATEDIR so that pseudo can find
     * the server binary and socket directory.
     */
    {
        const char *ldp    = "LD_PRELOAD=";
        size_t      ldp_len = strlen(ldp);

        /* is_kept: return 1 if the env entry should NOT be stripped.
         * Keep non-PSEUDO, non-LD_PRELOAD, PSEUDO_PREFIX, PSEUDO_LOCALSTATEDIR.
         */
        /* NOTE: GCC nested functions are used here for locality; the lambda
         * captures ldp/ldp_len by enclosing scope (auto function). */
        int is_kept(const char *entry) {
            if (strncmp(entry, "PSEUDO_", 7) != 0 &&
                strncmp(entry, ldp, ldp_len) != 0)
                return 1;
            if (strncmp(entry, "PSEUDO_PREFIX=",       14) == 0) return 1;
            if (strncmp(entry, "PSEUDO_LOCALSTATEDIR=", 21) == 0) return 1;
            return 0;
        }

        /* Process internal_table */
        {
            int w = 0;
            for (int i = 0; i < internal_count; i++) {
                if (strncmp(internal_table[i], ldp, ldp_len) == 0) {
                    /* LD_PRELOAD: strip ASAN lib, keep pseudo lib */
                    char *s = strip_asan_from_preload(
                                  internal_table[i] + ldp_len);
                    char *e = s ? malloc(ldp_len + strlen(s) + 1) : NULL;
                    if (e) {
                        memcpy(e, ldp, ldp_len);
                        strcpy(e + ldp_len, s);
                    }
                    free(internal_table[i]);
                    free(s);
                    if (e) internal_table[w++] = e;
                } else if (is_kept(internal_table[i])) {
                    internal_table[w++] = internal_table[i];
                } else {
                    free(internal_table[i]); /* drop PSEUDO_BINDIR etc. */
                }
            }
            internal_count = w;
            internal_table[w] = NULL;
        }

        /* Process environ (the heap copy) */
        {
            int w = 0;
            for (int i = 0; environ[i]; i++) {
                if (strncmp(environ[i], ldp, ldp_len) == 0) {
                    char *s = strip_asan_from_preload(environ[i] + ldp_len);
                    char *e = s ? malloc(ldp_len + strlen(s) + 1) : NULL;
                    if (e) {
                        memcpy(e, ldp, ldp_len);
                        strcpy(e + ldp_len, s);
                    }
                    free(environ[i]);
                    free(s);
                    if (e) environ[w++] = e;
                } else if (is_kept(environ[i])) {
                    environ[w++] = environ[i];
                } else {
                    free(environ[i]); /* drop PSEUDO_BINDIR etc. */
                }
            }
            environ[w] = NULL;
        }
    }

    /*
     * Force pseudo server startup in the parent before any children are
     * forked.  Without this, all PIPELINE_STAGES children may try to spawn
     * the server simultaneously, causing a "lock_held" race condition on the
     * first iteration.
     */
    access(".", F_OK);

    int iterations = 20;
    for (int i = 0; i < iterations; i++) {
        if (run_pipeline_iteration() != 0) {
            fprintf(stderr, "FAILED on pipeline iteration %d\n", i + 1);
            return 1;
        }
    }

    return 0;
}
