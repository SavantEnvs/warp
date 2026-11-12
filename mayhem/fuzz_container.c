/* Binary files of a model container: trunk.bin, the expert banks, codebooks.bin, and usage.waste.
 *
 *   byte 0   bits 0-2 fixture, bit 3 verify_records, bit 4 "input is usage.waste", bit 5 truncate
 *   byte 1   which .bin file of the fixture to damage
 *   rest     patches: offset u32 LE, length u8, bytes — overlaid on the valid file
 * In usage mode the rest of the input is the whole usage.waste. */
#include "harness_common.h"


static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2) return -1;
    const unsigned flags = data[0];
    char fix[PATH_MAX], dir[PATH_MAX];
    warp_fixture_dir(flags & 7, fix, sizeof fix);
    warp_scratch_new(dir, sizeof dir);

    const char *skip = NULL;
    char *bins[32];
    int nbins = 0;
    DIR *d = opendir(fix);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (l > 4 && !strcmp(e->d_name + l - 4, ".bin") && nbins < 32) bins[nbins++] = strdup(e->d_name);
    }
    if (d) closedir(d);
    qsort(bins, nbins, sizeof bins[0], cmp_str);

    if (flags & 0x10) {
        warp_link_fixture(dir, fix, NULL);
        warp_write(dir, "usage.waste", data + 2, size - 2);
    } else if (nbins) {
        skip = bins[data[1] % nbins];
        char src[PATH_MAX];
        snprintf(src, sizeof src, "%s/%s", fix, skip);
        size_t len = 0;
        uint8_t *buf = warp_read_all(src, &len);
        size_t pos = 2;
        size_t cut = len;
        for (int n = 0; pos + 5 <= size && n < 64; n++) {
            uint32_t off = (uint32_t)data[pos] | (uint32_t)data[pos + 1] << 8 |
                           (uint32_t)data[pos + 2] << 16 | (uint32_t)data[pos + 3] << 24;
            size_t plen = data[pos + 4];
            pos += 5;
            if (pos + plen > size) plen = size - pos;
            if (len) {
                size_t o = off % len;
                if (n == 0) cut = o;
                if (plen > len - o) plen = len - o;
                memcpy(buf + o, data + pos, plen);
            }
            pos += plen;
        }
        warp_write(dir, skip, buf, (flags & 0x20) ? cut : len);
        free(buf);
        warp_link_fixture(dir, fix, skip);
    }

    waste_ctx *ctx = NULL;
    if (warp_open(dir, (flags >> 3) & 1, warp_has_file(dir, "vision.json"), &ctx) == WASTE_OK) {
        warp_exercise(ctx);
        waste_close(ctx);
    }
    for (int i = 0; i < nbins; i++) free(bins[i]);
    warp_scratch_free(dir);
    return 0;
}
