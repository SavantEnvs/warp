/* manifest.json of a model container. Byte 0 picks one of the synthetic fixture containers (one per
 * architecture the engine reads); the rest of the input is that container's manifest.json. */
#include "harness_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    char fix[PATH_MAX], dir[PATH_MAX];
    warp_fixture_dir(data[0], fix, sizeof fix);
    warp_scratch_new(dir, sizeof dir);
    warp_write(dir, "manifest.json", data + 1, size - 1);
    warp_link_fixture(dir, fix, "manifest.json");

    waste_memplan plan;
    (void)waste_plan_memory(dir, 64, &plan);
    waste_ctx *ctx = NULL;
    if (warp_open(dir, 0, warp_has_file(dir, "vision.json"), &ctx) == WASTE_OK) {
        warp_exercise(ctx);
        waste_close(ctx);
    }
    warp_scratch_free(dir);
    return 0;
}
