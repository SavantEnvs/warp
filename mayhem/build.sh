#!/usr/bin/env bash
#
# mayhem/build.sh — builds (1) the project with its own Makefile and normal flags plus its test binaries
# (what mayhem/test.sh runs), (2) a sanitized + SanCov-instrumented libwaste.a out of tree under
# mayhem/build/, (3) the synthetic model fixtures the harnesses open, (4) one fuzz binary and one
# -standalone reproducer per harness under /mayhem.
#
# Runs inside the commit image as `mayhem` in /mayhem; re-runnable offline.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

: "${SRC:=/mayhem}"
cd "$SRC"

# 1) The project's own build with its normal flags: the oracle's binaries (waste, test_*, libwaste.so).
#    Upstream's objects stay in src/*.o and the like, all gitignored; CFLAGS comes from the environment
#    because the Makefile appends to it (-DVQ_SUPER, -MMD) and a command-line value would drop those.
env -u CFLAGS -u LDFLAGS CFLAGS="-O2 -std=gnu11 -Wall -Wextra $COVERAGE_FLAGS" LDFLAGS="$COVERAGE_FLAGS" \
    make -j"$MAYHEM_JOBS" all test

# 2) The sanitized library, compiled from the same source list the Makefile used for libwaste.a.
B="$SRC/mayhem/build"
rm -rf "$B"
mkdir -p "$B/obj"
mapfile -t OBJS < <(ar t libwaste.a)
SAN="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 -std=gnu11 -DVQ_SUPER=2"
compile_one() {
    local o="$1" extra=""
    local c="src/${o%.o}.c"
    case "$c" in
        src/simd_avx2.c)   extra="-mavx2 -mfma" ;;
        src/simd_avx512.c) extra="-mavx512f -mavx512bw -mavx512vbmi" ;;
        src/simd_i8mm.c)   extra="-march=armv8.2-a+i8mm" ;;
    esac
    # shellcheck disable=SC2086
    $CC $SAN $extra -c "$c" -o "$B/obj/$o"
}
export -f compile_one
export SAN B CC
printf '%s\n' "${OBJS[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c 'compile_one {}'
rm -f "$B/libwaste-san.a"
ar rcs "$B/libwaste-san.a" "$B"/obj/*.o

# 3) Synthetic model containers (stdlib-only generator shipped by upstream), one per architecture.
F="$SRC/mayhem/fixture"
rm -rf "$F"
mkdir -p "$F"
gen() { local n="$1"; shift; python3 tools/make_test_container.py "$@" "$F/$n" >/dev/null; }
gen kimi    --tokenizer
gen rope    --rope --tokenizer
gen glm     --glm --tokenizer
gen glmvis  --glm --vision --tokenizer
gen qwen    --qwen --tokenizer
gen ds41    --ds41 --tokenizer
gen ds41vis --ds41 --vision --tokenizer
gen vq4p    --index-bits 6 --tokenizer

# 4) Harnesses: libFuzzer binary + standalone reproducer each.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"
for h in fuzz_manifest fuzz_container fuzz_tokenizer_model fuzz_tokenizer_text fuzz_state fuzz_image; do
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -std=gnu11 -I"$SRC/src" -I"$SRC/mayhem" $LIB_FUZZING_ENGINE \
        "mayhem/$h.c" "$B/libwaste-san.a" "$B/lsan_off.o" -lm -lpthread -o "/mayhem/$h"
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -std=gnu11 -I"$SRC/src" -I"$SRC/mayhem" \
        "mayhem/$h.c" "$B/standalone_main.o" "$B/libwaste-san.a" "$B/lsan_off.o" -lm -lpthread \
        -o "/mayhem/$h-standalone"
done
