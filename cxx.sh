#!/bin/bash

MAJOR=5
MINOR=0
PATCH=0

if [ "" = "${CXX}" ]; then
  CXX=${CC}
fi
if [ "" = "${CXX}" ]; then
  CXX=$(command -v g++)
fi

HIT=0
ARGS=""
while test $# -gt 0
do
  case "$1" in
  -E) HIT=$((HIT+1))
    ;;
  -P) HIT=$((HIT+1))
    ;;
  -) HIT=$((HIT+1))
    ;;
  *) ARGS+="$1"
    ;;
  esac
  shift
done

if [ "3" = "${HIT}" ] && [ "2" = "$(cat - | wc -w)" ]; then
  echo "${MAJOR} ${MINOR}"
else
  ${CXX} ${ARGS}
fi
