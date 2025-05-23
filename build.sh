#!/bin/bash

set -euo pipefail

make

rm -rf out
mkdir -p out/atmosphere/contents/4A00000000201F7C

cp systime.nsp out/atmosphere/contents/4A00000000201F7C/exefs.nsp
mkdir out/atmosphere/contents/4A00000000201F7C/flags
touch out/atmosphere/contents/4A00000000201F7C/flags/boot2.flag
