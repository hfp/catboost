#!/bin/bash

MAJOR=5
MINOR=0
PATCH=0

CAT=$(command -v cat)
WC=$(command -v wc)

if [ "" = "${YCC}" ]; then
  YCC=$(command -v gcc)
fi
if [ "" = "${YCC}" ]; then
  echo "Error: compiler not found, please set YCC variable!"
  exit 1
fi

NHITS=0
while test $# -gt 0
do
  case "$1" in
  -E) NHITS=$((NHITS+1)); HIT=$1;
    ;;
  -P) NHITS=$((NHITS+1)); HIT=$1;
    ;;
  -) NHITS=$((NHITS+1)); HIT=$1;
    ;;
  *) ARGS+="$1";
    ;;
  esac
  shift
done

if [ "1" = "${NHITS}" ] && [ "-E" = "${HIT}" ] && [ "" != "${ARGS}" ] && [ -e ${ARGS} ]; then
  VERSION="-D__clang_major__=${MAJOR} -D__clang_minor__=${MINOR} -D__clang_patchlevel__=${PATCH}"
  eval "${YCC} ${VERSION} -E ${ARGS} 2>/dev/null"
elif [ "3" = "${NHITS}" ] && [ "" != "${CAT}" ] && [ "" != "${WC}" ] && [ "2" = "$(${CAT} - | ${WC} -w)" ]; then
  echo "${MAJOR} ${MINOR}"
else
  eval "${YCC} ${ARGS}"
fi
