#!/bin/bash
set -e

ScriptDir="$(dirname "$(readlink -f "$0")")"

gcc -ggdb -o "$ScriptDir"/../build/metac "$ScriptDir"/meta.c
