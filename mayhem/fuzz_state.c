/* Session checkpoint file (waste_state_load). Byte 0 picks an open context — Kimi-Linear, GLM, Qwen or
 * DeepSeek fixture, each with its own state layout — and the rest is the checkpoint. A load that
 * succeeds is followed by one decode step on the restored state; the context is reset after every input. */
#include "harness_common.h"

#define NCTX 4
static waste_ctx *ctxs[NCTX];
static const int ctx_fixture[NCTX] = {0, 2, 4, 5};   /* kimi, glm, qwen, ds41 */

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc; (void)argv;
    for (int i = 0; i < NCTX; i++) {
        char fix[PATH_MAX];
        warp_fixture_dir(ctx_fixture[i], fix, sizeof fix);
        if (warp_open(fix, 0, 0, &ctxs[i]) != WASTE_OK) {
            fprintf(stderr, "harness: cannot open fixture %s\n", fix);
            abort();
        }
    }
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    waste_ctx *ctx = ctxs[data[0] % NCTX];
    char dir[PATH_MAX];
    warp_scratch_new(dir, sizeof dir);
    warp_write(dir, "state.bin", data + 1, size - 1);
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/state.bin", dir);

    waste_state_reset(ctx);
    if (waste_state_load(ctx, path) == WASTE_OK) {
        int32_t tok = 3;
        const float *logits = NULL;
        size_t vocab = 0;
        if (waste_eval(ctx, &tok, 1, &logits, &vocab) == WASTE_OK && logits && vocab) {
            volatile float sink = logits[0];
            (void)sink;
        }
    }
    waste_state_reset(ctx);
    warp_scratch_free(dir);
    return 0;
}
