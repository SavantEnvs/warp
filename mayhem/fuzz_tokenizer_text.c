/* Text through the BPE encoder and decoder of a valid tokenizer. Byte 0 selects the pre-tokenization
 * variant (control tokens on/off, Han split, digit run length, DeepSeek pattern); the rest is the text. */
#include "harness_common.h"
#include "tokenizer.h"

static waste_tok *tok;

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc; (void)argv;
    char fix[PATH_MAX];
    warp_fixture_dir(0, fix, sizeof fix);
    tok = waste_tok_open(fix);
    if (!tok) { fputs("harness: cannot open fixture tokenizer\n", stderr); abort(); }
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    const unsigned f = data[0];
    waste_tok_set_han_split(tok, (f >> 1) & 1);
    waste_tok_set_digit_run(tok, (f & 4) ? 1 : 3);
    waste_tok_set_pattern(tok, (f & 8) ? WASTE_TOKPAT_DEEPSEEK : WASTE_TOKPAT_CL100K);

    const size_t n = size - 1;
    char *text = (char *)malloc(n + 1);
    memcpy(text, data + 1, n);
    text[n] = 0;
    const int cap = (int)n + 8;
    int32_t *ids = (int32_t *)malloc(sizeof(int32_t) * (size_t)cap);
    const int cnt = waste_tok_encode(tok, text, ids, cap, f & 1);
    if (cnt > 0) {
        char *out = (char *)malloc((size_t)cnt * 16 + 16);
        (void)waste_tok_decode(tok, ids, cnt, out, cnt * 16 + 16);
        free(out);
    }
    free(ids);
    free(text);
    return 0;
}
