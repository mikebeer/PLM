#!/bin/sh
# Extra tests for the C89 version; the expected output (NAME.out)
# comes from the cREXX version.
#   c/tests/run.sh            run all tests with c/plmc-c
#   PLMC=./plmc c/tests/run.sh --update   rewrite the .out files
HERE=$(cd "$(dirname "$0")" && pwd)
PLMC="${PLMC:-$HERE/../plmc-c}"
cd "$HERE"
pass=0; fail=0
for src in *.plm; do
  name=${src%.plm}
  if [ -f "$name.in" ]; then in="$name.in"; else in=/dev/null; fi
  out=$("$PLMC" "$src" < "$in" 2>&1)
  if [ "$1" = "--update" ]; then
    printf '%s\n' "$out" > "$name.out"
    echo "updated $name"
  elif [ "$out" = "$(cat "$name.out")" ]; then
    pass=$((pass+1)); echo "ok    $name"
  else
    fail=$((fail+1)); echo "FAIL  $name"
    printf '%s\n' "$out" | diff - "$name.out" | head -10
  fi
  rm -f "$name.rxas" "$name.rxbin"
done
[ "$1" = "--update" ] && exit 0
echo "$pass passed, $fail failed"
[ $fail = 0 ]
