#!/bin/sh
# الاستخدام: run_test.sh <ee> <program.ee> <expected>
in="${3%.expected}.in"
[ -f "$in" ] || in=/dev/null
"$1" "$2" < "$in" 2>&1 | diff - "$3"
