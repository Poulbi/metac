#!/bin/sh

set -ex

ThisDirectory="$(dirname "$(readlink -f "$0")")"

gcc -ggdb -Wall -o "$ThisDirectory"/../build/metac "$ThisDirectory"/../source/meta.c
