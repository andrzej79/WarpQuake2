#!/bin/sh
# A headless timedemo under vamos (amitools) on the host: deterministic, no
# task switches or interrupts, and 80-bit floats computed exactly as the 060
# does, so its frame CRCs compare two builds pixel for pixel.
#
#   tools/vamos_crc.sh [binary] [extra engine options...]
#
# Needs VAMOS (default temp/vamos-venv/bin/vamos) and the game data in
# baseq2/ next to this repository's root (git-ignored).  Prints the Q2BENCH
# line; the per-frame CRCs go to temp/vw/crc.txt.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
VAMOS=${VAMOS:-$ROOT/temp/vamos-venv/bin/vamos}
BIN=${1:-$ROOT/build/WarpQuake2}
[ $# -gt 0 ] && shift
W=$ROOT/temp/vw
mkdir -p "$W/warpq2"
[ -e "$W/baseq2" ] || ln -s "$ROOT/baseq2" "$W/baseq2"
cp "$BIN" "$W/WarpQuake2"
rm -f "$W/crc.txt"
cd "$W"
"$VAMOS" -C 68040 -s 1200 -m 262144 -H disable -V work:"$W" -- work:WarpQuake2 -headless -benchmark -crc \
  -crcfile work:crc.txt "$@" +set basedir work: +set game warpq2 +set fixedtime 100 +set timedemo 1 \
  +demomap demo1.dm2 2>&1 | grep -a 'Q2BENCH\|rror'
