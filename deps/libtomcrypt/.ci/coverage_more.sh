#!/bin/bash

set -e

function pdiv() {
  printf "\n====== %s ======\n" "$*"
}

if [ "$#" = "1" -a "$(echo $1 | grep 'gmp')" != "" ]; then
   pdiv "Test GMP"
   ./test t gmp
fi

pdiv "Sizes"
./sizes
pdiv "Constants"
./constants

pdiv "Generate hashsum_tv.txt"
for i in $(for j in $(echo $(./hashsum -h | awk '/Algorithms/,EOF' | tail -n +2)); do echo $j; done | sort); do
  echo -n "$i: " && ./hashsum -a $i tests/test.key
done > hashsum_tv.txt

pdiv "Compare hashsum_tv.txt"
difftroubles=$(diff -i -w -B hashsum_tv.txt notes/hashsum_tv.txt | grep '^<') || true
if [ -n "$difftroubles" ]; then
  echo "FAILURE: hashsum_tv.tx"
  diff -i -w -B hashsum_tv.txt notes/hashsum_tv.txt
  echo "hashsum failed"
  exit 1
else
  echo "hashsum okay"
fi


exit 0
