/* An image file through the engine's decoders and preprocessors (stb_image behind the engine's own size
 * limit, then the Kimi, GLM and DeepSeek-V4.1 resize/normalise/patchify paths). */
#include "harness_common.h"
#include "model.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    char dir[PATH_MAX], path[PATH_MAX];
    warp_scratch_new(dir, sizeof dir);
    warp_write(dir, "img", data, size);
    snprintf(path, sizeof path, "%s/img", dir);

    int w = 0, h = 0;
    (void)waste_image_dimensions(path, &w, &h);
    (void)waste_image_size(path, &w, &h);
    static const float mean[3] = {0.5f, 0.5f, 0.5f}, stdv[3] = {0.5f, 0.5f, 0.5f};
    int gh = 0, gw = 0;
    float *px = waste_image_load(path, 64, mean, stdv, &gh, &gw);
    free(px);

    waste_vision_cfg v;
    memset(&v, 0, sizeof v);
    v.patch = 14; v.merge = 2; v.temporal = 2; v.max_patches = 64; v.min_tokens = 1;
    for (int i = 0; i < 3; i++) { v.mean[i] = 0.5f; v.std[i] = 0.5f; }
    px = waste_image_load_glm(path, &v, &gh, &gw);
    free(px);

    v.merge = 3; v.temporal = 0; v.min_tokens = 0;
    px = waste_image_load_ds41(path, &v, &gh, &gw);
    free(px);

    warp_scratch_free(dir);
    return 0;
}
