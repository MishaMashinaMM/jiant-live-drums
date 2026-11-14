#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Standalone, hardware-compatible fixed-point global EQ regression.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/jld-global-eq-test-$$"
trap 'rm -f "$out"' EXIT HUP INT TERM
cc -O2 -std=gnu99 -I firmware/src tests/global_eq_test.c firmware/src/track_eq_dsp.c -lm -o "$out"
"$out"
