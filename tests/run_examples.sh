#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 kivitas
# Licensed under the GNU GPL v3 or later; see the LICENSE file.
#
# Runs each tests/examples/*.kiv and compares stdout with the matching .out file.
# Each script runs in the interpreter, under GC stress, and, when this build has
# a JIT, with --jit. Also checks that syntax errors exit 1 and runtime errors exit 2.

set -eu

KIV=${1:?usage: run_examples.sh path/to/kiv}
HERE=$(cd "$(dirname "$0")" && pwd)
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT
fail=0

# check <script> <expected-output-file> [kiv options...]
check() {
    script=$1
    expected=$2
    shift 2
    if ! "$KIV" "$@" "$script" > "$TMP" 2> /dev/null; then
        echo "FAIL (non-zero exit): kiv $* $script"
        fail=1
        return
    fi
    if ! cmp -s "$expected" "$TMP"; then
        echo "FAIL (output differs): kiv $* $script"
        diff "$expected" "$TMP" || true
        fail=1
    fi
}

# expect_status <status> <script> [kiv options...]
expect_status() {
    want=$1
    script=$2
    shift 2
    set +e
    "$KIV" "$@" "$script" > /dev/null 2>&1
    got=$?
    set -e
    if [ "$got" -ne "$want" ]; then
        echo "FAIL (exit $got, wanted $want): kiv $* $script"
        fail=1
    fi
}

JIT=no
if "$KIV" --jit /dev/null > /dev/null 2>&1; then
    JIT=yes
fi

for src in "$HERE"/examples/*.kiv; do
    expected="${src%.kiv}.out"
    check "$src" "$expected"
    check "$src" "$expected" --gc-stress
    if [ "$JIT" = yes ]; then
        check "$src" "$expected" --jit
        check "$src" "$expected" --jit --gc-stress
    fi
done

expect_status 1 "$HERE/errors/syntax_error.kiv"
expect_status 2 "$HERE/errors/type_error.kiv"
if [ "$JIT" = yes ]; then
    expect_status 2 "$HERE/errors/type_error.kiv" --jit
fi

if [ "$fail" -eq 0 ]; then
    echo "examples: all passed (jit: $JIT)"
fi
exit "$fail"
