#!/usr/bin/env bash
# difftest.sh — differential testing against the host gcc/clang (Linux / WSL).
#
# For every program in tests/diff/ this compiles it twice:
#   * with mycc, then assembles the result with the host compiler;
#   * with the host compiler directly.
# Both executables must return the same exit code.
#
# Why this exists: the exit-code suite cannot tell "correct" from "wrong but
# plausible". BUG-001 (double evaluation of `a[i++] += 5`), BUG-002, BUG-003
# (8-byte struct copy) and BUG-020 (double index scaling) all returned a
# number and all passed 48 green tests. A reference implementation catches
# that class of bug for free.
#
# Usage:  ./difftest.sh            (expects ./mycc and tests/diff/*.c)
#         SRCDIR=build/fuzz QUIET=1 ./difftest.sh   (the fuzz corpus)
# Exit:   0 = all programs agreed, 1 = a mismatch or a build failure.
set -u

MYCC=${MYCC:-./mycc}
CC=${CC:-gcc}
OUT=${OUT:-/tmp/mycc-difftest}
TARGETFLAG=${TARGETFLAG:-}
LDFLAGS=${LDFLAGS:-}
SRCDIR=${SRCDIR:-tests/diff}
QUIET=${QUIET:-}

mkdir -p "$OUT"
status=0
total=0

for src in "$SRCDIR"/*.c; do
    [ -e "$src" ] || { echo "no $SRCDIR/*.c found"; exit 1; }
    base=$(basename "$src" .c)
    total=$((total + 1))

    if ! "$MYCC" $TARGETFLAG "$src" -o "$OUT/$base.mycc.s" > "$OUT/$base.mycc.log" 2>&1; then
        echo "FAIL: $src — mycc rejected it"
        cat "$OUT/$base.mycc.log"
        status=1
        continue
    fi
    if ! "$CC" $LDFLAGS -o "$OUT/$base.mycc.exe" "$OUT/$base.mycc.s" > "$OUT/$base.asm.log" 2>&1; then
        echo "FAIL: $src — assembling mycc's output failed"
        cat "$OUT/$base.asm.log"
        status=1
        continue
    fi
    if ! "$CC" -std=c99 -w -o "$OUT/$base.ref.exe" "$src" > "$OUT/$base.ref.log" 2>&1; then
        echo "FAIL: $src — the reference compiler rejected it (the test itself is wrong)"
        cat "$OUT/$base.ref.log"
        status=1
        continue
    fi

    "$OUT/$base.mycc.exe"; mine=$?
    "$OUT/$base.ref.exe"; theirs=$?

    if [ "$mine" != "$theirs" ]; then
        echo "FAIL: $src - mycc returned $mine, the reference compiler returned $theirs"
        status=1
    elif [ -z "$QUIET" ]; then
        echo "PASS: $src (both returned $mine)"
    fi
done

if [ "$status" = 0 ]; then
    if [ -n "$QUIET" ]; then
        echo "PASS: all $total programs agreed with the reference compiler"
    else
        echo "All differential tests passed"
    fi
fi
exit $status
