/* Shared helpers for the WARP fuzz harnesses.
 *
 * A WARP model is a directory (manifest.json, trunk.bin, experts-L*.bin, codebooks.bin, ...) that the
 * engine treats as untrusted input. The harnesses build such a directory per input under $TMPDIR:
 * everything an input does not replace is a symlink to a small synthetic container that build.sh
 * generates (tools/make_test_container.py) under mayhem/fixture/<name>/, next to the binaries. */
#ifndef WARP_HARNESS_COMMON_H
#define WARP_HARNESS_COMMON_H

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "waste.h"

static const char *const WARP_FIXTURES[] = {
    "kimi", "rope", "glm", "glmvis", "qwen", "ds41", "ds41vis", "vq4p",
};
#define WARP_NFIXTURES ((int)(sizeof WARP_FIXTURES / sizeof WARP_FIXTURES[0]))

static char warp_fixture_root[PATH_MAX];

static void warp_find_fixtures(void)
{
    if (warp_fixture_root[0]) return;
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) { fputs("harness: cannot resolve /proc/self/exe\n", stderr); abort(); }
    exe[n] = 0;
    char *slash = strrchr(exe, '/');
    if (slash) *slash = 0;
    snprintf(warp_fixture_root, sizeof warp_fixture_root, "%s/mayhem/fixture", exe);
}

static const char *warp_fixture_dir(int idx, char *buf, size_t cap)
{
    warp_find_fixtures();
    snprintf(buf, cap, "%s/%s", warp_fixture_root, WARP_FIXTURES[(unsigned)idx % WARP_NFIXTURES]);
    return buf;
}

static char *warp_scratch_new(char *buf, size_t cap)
{
    const char *t = getenv("TMPDIR");
    if (!t || !*t) t = "/tmp";
    snprintf(buf, cap, "%s/warp-fuzz-XXXXXX", t);
    if (!mkdtemp(buf)) { perror("mkdtemp"); abort(); }
    return buf;
}

static void warp_scratch_free(const char *dir)
{
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char p[PATH_MAX];
            snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
            unlink(p);
        }
        closedir(d);
    }
    rmdir(dir);
}

static void warp_write(const char *dir, const char *name, const void *data, size_t len)
{
    char p[PATH_MAX];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "wb");
    if (!f) { perror("fopen"); abort(); }
    if (len && fwrite(data, 1, len, f) != len) { perror("fwrite"); abort(); }
    fclose(f);
}

/* Symlink every regular file of the fixture into `dir`, except `skip` (may be NULL). */
static void warp_link_fixture(const char *dir, const char *fix, const char *skip)
{
    DIR *d = opendir(fix);
    if (!d) { fprintf(stderr, "harness: fixture %s missing\n", fix); abort(); }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (skip && !strcmp(e->d_name, skip)) continue;
        char src[PATH_MAX], dst[PATH_MAX];
        snprintf(src, sizeof src, "%s/%s", fix, e->d_name);
        snprintf(dst, sizeof dst, "%s/%s", dir, e->d_name);
        if (symlink(src, dst)) { perror("symlink"); abort(); }
    }
    closedir(d);
}

static int warp_has_file(const char *dir, const char *name)
{
    char p[PATH_MAX];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    return access(p, R_OK) == 0;
}

static uint8_t *warp_read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "harness: cannot read %s\n", path); abort(); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc(n > 0 ? (size_t)n : 1);
    if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) abort();
    fclose(f);
    *len = n > 0 ? (size_t)n : 0;
    return b;
}

static waste_status warp_open(const char *dir, int verify, int vision, waste_ctx **out)
{
    waste_cfg cfg;
    waste_cfg_init(&cfg);
    cfg.ram_budget_bytes = 40ull << 20;
    cfg.ctx_tokens = 64;
    cfg.n_threads = 1;
    cfg.use_direct_io = 0;
    cfg.verify_records = verify;
    cfg.vision = vision;
    return waste_open(dir, &cfg, out);
}

/* A short, fixed drive of an open context: tokenize, decode a few tokens, read the logits. */
static void warp_exercise(waste_ctx *ctx)
{
    int32_t tok[64];
    size_t nt = 0;
    if (waste_tokenize(ctx, "Hello, world 2026", 1, tok, 64, &nt) != WASTE_OK || nt == 0) {
        /* the Qwen fixture has no tokenizer */
        tok[0] = 3; tok[1] = 5; tok[2] = 7; tok[3] = 9;
        nt = 4;
    }
    if (nt > 4) nt = 4;
    const float *logits = NULL;
    size_t vocab = 0;
    if (waste_eval(ctx, tok, nt, &logits, &vocab) == WASTE_OK && logits && vocab) {
        volatile float sink = logits[0] + logits[vocab - 1];
        (void)sink;
    }
}

#endif
