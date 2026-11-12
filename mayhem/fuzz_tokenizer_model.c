/* tokenizer.model (tiktoken text: base64 token + rank per line) loaded by the BPE tokenizer, then used
 * to encode and decode a fixed set of strings (ASCII, accents, CJK, digits, control-token markup). */
#include "harness_common.h"
#include "tokenizer.h"

static const char *const TEXTS[] = {
    "Hello, world 2026",
    "h\xc3\xa9llo w\xc3\xb6rld \xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c!  \n\n  tabs\t",
    "<|im_start|>user\nwhat's 12345 + 678?<|im_end|>",
    "don't  I've   WE'LL 007 a1b22c333d4444",
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char dir[PATH_MAX];
    warp_scratch_new(dir, sizeof dir);
    warp_write(dir, "tokenizer.model", data, size);
    waste_tok *t = waste_tok_open(dir);
    warp_scratch_free(dir);
    if (!t) return -1;

    (void)waste_tok_vocab(t);
    (void)waste_tok_bos(t);
    (void)waste_tok_eos(t);
    for (unsigned i = 0; i < sizeof TEXTS / sizeof TEXTS[0]; i++) {
        int32_t ids[256];
        char out[1024];
        for (int sp = 0; sp < 2; sp++) {
            int n = waste_tok_encode(t, TEXTS[i], ids, 256, sp);
            if (n > 0) {
                (void)waste_tok_decode(t, ids, n, out, sizeof out);
                (void)waste_tok_decode_len1(t, ids[0]);
            }
        }
    }
    waste_tok_free(t);
    return 0;
}
