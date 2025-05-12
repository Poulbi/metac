#!/bin/bash
set -e

ScriptDir="$(dirname "$(readlink -f "$0")")"

printf 'meta.c\n'
gcc -ggdb -o "$ScriptDir"/../build/metac "$ScriptDir"/meta.c
