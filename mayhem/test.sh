#!/usr/bin/env bash
#
# mayhem/test.sh — runs the project's own suite (tests/run.sh, using the binaries mayhem/build.sh built with
# the project's normal flags) and adds known-answer probes of the tokenizer through the CLI.
#
# Both halves assert behaviour: run.sh's summary line must show a plausible number of passes and no
# failures, and the probes compare token ids / decoded text to fixed values. A neutered program prints
# neither, so both fail.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

for bin in waste test_tokenizer test_state test_container libwaste.a; do
  [ -e "$bin" ] || { echo "missing $bin: mayhem/build.sh did not produce it" >&2; emit_ctrf "warp-run-sh" 0 1 0; exit 1; }
done

passed=0; failed=0; skipped=0

# 1) The upstream suite. It reports "<P> passed, <F> failed, <S> skipped" on its last line.
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT
bash tests/run.sh /nonexistent-model >"$LOG" 2>&1
sed 's/\x1b\[[0-9;]*m//g' "$LOG" > "$LOG.plain"
summary="$(grep -E '^[0-9]+ passed, [0-9]+ failed, [0-9]+ skipped$' "$LOG.plain" | tail -1)"
rm -f "$LOG.plain"
if [ -n "$summary" ]; then
  p="$(echo "$summary" | awk '{print $1}')"; f="$(echo "$summary" | awk '{print $3}')"; s="$(echo "$summary" | awk '{print $5}')"
  # tests/run.sh has 80+ checks that run without a real model; far fewer means it did not run properly
  if [ "$p" -lt 60 ]; then failed=$((failed + 1)); echo "run.sh: only $p checks passed" >&2; fi
  passed=$((passed + p)); failed=$((failed + f)); skipped=$((skipped + s))
else
  failed=$((failed + 1)); echo "run.sh: no summary line found" >&2; tail -20 "$LOG" >&2
fi

# 2) Known-answer probes: token ids of fixed strings against the deterministic synthetic tokenizer.
T="$(mktemp -d)"
trap 'rm -rf "$T"; rm -f "$LOG"' EXIT
if python3 tools/make_test_container.py "$T/m.waste" --tokenizer >/dev/null 2>&1; then
  probe() {  # <name> <expected> <actual>
    if [ "$2" = "$3" ]; then passed=$((passed + 1)); else failed=$((failed + 1)); echo "probe $1: expected [$2] got [$3]" >&2; fi
  }
  probe tokenize-ascii "72 101 108 108 111 44 269 265 108 100 32 50 48 50 54" \
        "$(./waste tokenize "$T/m.waste" "Hello, world 2026" 2>/dev/null)"
  probe tokenize-utf8 "104 195 169 108 108 111" "$(./waste tokenize "$T/m.waste" "héllo" 2>/dev/null)"
  probe detokenize "Hello, world 2026" \
        "$(./waste detokenize "$T/m.waste" "72 101 108 108 111 44 269 265 108 100 32 50 48 50 54" 2>/dev/null)"
else
  failed=$((failed + 1)); echo "could not generate the probe container" >&2
fi

emit_ctrf "warp-run-sh" "$passed" "$failed" "$skipped"
