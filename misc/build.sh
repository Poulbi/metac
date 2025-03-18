#!/bin/sh

set -ex

ThisDirectory="$(dirname "$(readlink -f "$0")")"

gcc -ggdb \
    -Wall -Wno-unused-but-set-variable -Wno-unused-variable \
    -o "$ThisDirectory"/../build/metac "$ThisDirectory"/../source/meta.c
